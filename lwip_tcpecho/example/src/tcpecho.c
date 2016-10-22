/*
 * @brief LWIP no-RTOS TCP Echo example
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/timers.h"
#include "netif/etharp.h"

#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif

#include "board.h"
#include "lpc_phy.h"
#include "arch/lpc18xx_43xx_emac.h"
#include "arch/lpc_arch.h"
#include "echo.h"

/* Jorge */
#include <string.h>
#include "lwip/tcp.h"
#include "RTC.h"
#include "Temperature.h"

/* Jorge */
static struct tcp_pcb *_pcb;
static struct ip_addr ip;
static uint32_t data = 0xdeadbeef;
static err_t _error = ERR_CONN;

//char _POSTMessage[1024] =
//{
//		"POST http://posttestserver.com/post.php HTTP/1.1\r\n"
//		"User-Agent: Fiddler\r\n"
//		"Host: posttestserver.com\r\n"
//		"Content-Length: 13\r\n"
//		"\r\n"
//		"Jorge Freitas"
//};

//char _POSTMessage[1024] =
//{
//		"POST /dbTeste/col HTTP/1.1\r\n"
//		"Host: 54.191.0.152:8080\r\n"
//		"Accept: application/json\r\n"
//		"Content-Type: application/json\r\n"
//		"Content-Length: 18\r\n"
//		"\r\n"
//		"{\"chorou\":\"parou\"}"
//};

const char _POSTMessage[512] =
{
		"POST /dbTeste/col HTTP/1.1\r\n"
		"Host: 54.191.0.152:8080\r\n"
		"Accept: application/json\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 64\r\n"
		"\r\n"
		"{\"temp\": %05d, \"coord\":[{\"lat\":-23.8941901,\"log\":-46.4213524}]}"
};

char _Message[512] = { 0 };

uint32_t TCPSendPacket(void);
err_t TCPRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
void TCPErrorCallback(void *arg, err_t err);
void TCPClose();

void RTCEventHandler();

/* connection established callback, err is unused and only return 0 */
err_t ConnectCallback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	_error = ERR_OK;

	LWIP_DEBUGF(LWIP_DBG_ON, ("Connection Established.\n\r"));
	LWIP_DEBUGF(LWIP_DBG_ON, ("Now sending a packet\n\r"));
    TCPSendPacket();

//    RTCInitialization(RTCEventHandler);

    return 0;
}

err_t TCPConnect()
{
	_pcb = tcp_new();

//	tcp_connect(_pcb, &ip, 80, ConnectCallback);
	_error = tcp_connect(_pcb, &ip, 8080, ConnectCallback); //Juliano Port

	if(_error == ERR_OK)
	{
		_error = ERR_CONN;
		/* dummy data to pass to callbacks*/
		tcp_arg(_pcb, &data);

		/* register callbacks with the pcb */
		tcp_err(_pcb, TCPErrorCallback);
		tcp_recv(_pcb, TCPRecvCallback);
//		tcp_sent(_pcb, tcpSendCallback); //FIXME What is that?
//		tcp_write(_pcb, _POSTMessage, strlen(_POSTMessage), 1);
//		tcp_output(_pcb);

		return ERR_OK;
	}

	else
	{
		DEBUGOUT("Database connection error: %d\r\n", _error);

		TCPClose();
	}

	return _error;
}

uint32_t TCPSendPacket(void)
{
	err_t error;
//	uint16_t value = TSRead();
//    char *string = "HEAD /process.php?data1=12&data2=5 HTTP/1.0\r\nHost: mywebsite.com\r\n\r\n ";
//    uint32_t len = strlen(string);

    /* push to buffer */
	memset(_Message, 0, sizeof(_Message));
	sprintf(_Message, _POSTMessage, TSRead());
    error = tcp_write(_pcb, _Message, strlen(_Message), 0/*TCP_WRITE_FLAG_COPY*/);

    if (error) {
        LWIP_DEBUGF(LWIP_DBG_ON, ("ERROR: Code: %d (tcp_send_packet :: tcp_write)\n\r", error));
        return 1;
    }

    /* now send */
    error = tcp_output(_pcb);
    if (error) {
        LWIP_DEBUGF(LWIP_DBG_ON, ("ERROR: Code: %d (tcp_send_packet :: tcp_output)\n\r", error));
        return 1;
    }
    return 0;
}

err_t TCPRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    LWIP_DEBUGF(LWIP_DBG_ON, ("Data received.\n\r"));
    if (p == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_ON, ("The remote host closed the connection.\n\r"));
        LWIP_DEBUGF(LWIP_DBG_ON, ("Now I'm closing the connection.\n\r"));
        TCPClose();
        return ERR_ABRT;
    }
    else
    {
        LWIP_DEBUGF(LWIP_DBG_ON, ("Number of pbufs %d\n\r", pbuf_clen(p)));
        LWIP_DEBUGF(LWIP_DBG_ON, ("Contents of pbuf %s\n\r", (char *)p->payload));

        pbuf_free(p);
    }

//    TCPClose(); //Jorge

    return 0;
}

void TCPClose()
{
//    tcp_arg(_pcb, NULL);
//    tcp_sent(_pcb, NULL);
//    tcp_recv(_pcb, NULL);
//    tcp_close(_pcb);

	LWIP_DEBUGF(LWIP_DBG_ON, ("Closing the Mongo Database connection.\n\r"));

    tcp_arg(_pcb, NULL);
    tcp_sent(_pcb, NULL);
    tcp_recv(_pcb, NULL);
    tcp_err(_pcb, NULL);
    tcp_poll(_pcb, NULL, 0);

    tcp_close(_pcb);
}

void TCPErrorCallback(void *arg, err_t err)
{
	DEBUGOUT("Error %d.\n\r", err);

	if(err != ERR_OK)
	{
		TCPClose();

		TCPConnect();
	}
}

void RTCEventHandler()
{
	TCPSendPacket();
}


/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* NETIF data */
static struct netif lpc_netif;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	/* LED0 is used for the link status, on = PHY cable detected */
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED state is off to show an unconnected cable state */
	Board_LED_Set(0, false);

	/* Setup a 1mS sysTick for the primary time base */
	SysTick_Enable(1);
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	main routine for example_lwip_tcpecho_sa_18xx43xx
 * @return	Function should not exit.
 */
int main(void)
{
	uint32_t physts, rp = 0;
	ip_addr_t ipaddr, netmask, gw;

	prvSetupHardware();

	/* Initialize LWIP */
	lwip_init();

	LWIP_DEBUGF(LWIP_DBG_ON, ("Starting LWIP TCP echo server...\n\r"));

	/* Static IP assignment */
#if LWIP_DHCP
	IP4_ADDR(&gw, 0, 0, 0, 0);
	IP4_ADDR(&ipaddr, 0, 0, 0, 0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);
#else
	IP4_ADDR(&gw, 10, 1, 10, 1);
	IP4_ADDR(&ipaddr, 10, 1, 10, 234);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	APP_PRINT_IP(&ipaddr);
#endif

	/* Add netif interface for lpc17xx_8x */
	netif_add(&lpc_netif, &ipaddr, &netmask, &gw, NULL, lpc_enetif_init,
			  ethernet_input);
	netif_set_default(&lpc_netif);
	netif_set_up(&lpc_netif);

#if LWIP_DHCP
	dhcp_start(&lpc_netif);
#endif

	/* Initialize and start application */
//	echo_init();

	/* Jorge */
	RTCInitialization(RTCEventHandler);
	TSInitialization();
	uint16_t temperature = TSRead();

    /* create an ip */
//    struct ip_addr ip;
//    IP4_ADDR(&ip, 64,90,48,15);    //IP of my PHP server

//	tcp_bind(_pcb, IP_ADDR_ANY, 7);
    /* End Jorge */


	/* This could be done in the sysTick ISR, but may stay in IRQ context
	   too long, so do this stuff with a background loop. */
	while (1) {
		/* Handle packets as part of this loop, not in the IRQ handler */
		lpc_enetif_input(&lpc_netif);

		/* lpc_rx_queue will re-qeueu receive buffers. This normally occurs
		   automatically, but in systems were memory is constrained, pbufs
		   may not always be able to get allocated, so this function can be
		   optionally enabled to re-queue receive buffers. */
#if 0
		while (lpc_rx_queue(&lpc_netif)) {}
#endif

		/* Free TX buffers that are done sending */
		lpc_tx_reclaim(&lpc_netif);

		/* LWIP timers - ARP, DHCP, TCP, etc. */
		sys_check_timeouts();

		/* Call the PHY status update state machine once in a while
		   to keep the link status up-to-date */
		physts = lpcPHYStsPoll();

		/* Only check for connection state when the PHY status has changed */
		if (physts & PHY_LINK_CHANGED) {
			if (physts & PHY_LINK_CONNECTED) {
				Board_LED_Set(0, true);

				/* Set interface speed and duplex */
				if (physts & PHY_LINK_SPEED100) {
					Chip_ENET_SetSpeed(LPC_ETHERNET, 1);
					NETIF_INIT_SNMP(&lpc_netif, snmp_ifType_ethernet_csmacd, 100000000);
				}
				else {
					Chip_ENET_SetSpeed(LPC_ETHERNET, 0);
					NETIF_INIT_SNMP(&lpc_netif, snmp_ifType_ethernet_csmacd, 10000000);
				}
				if (physts & PHY_LINK_FULLDUPLX) {
					Chip_ENET_SetDuplex(LPC_ETHERNET, true);
				}
				else {
					Chip_ENET_SetDuplex(LPC_ETHERNET, false);
				}

				netif_set_link_up(&lpc_netif);
				rp = 0;
			}
			else {
				Board_LED_Set(0, false);
				netif_set_link_down(&lpc_netif);
			}

			DEBUGOUT("Link connect status: %d\r\n", ((physts & PHY_LINK_CONNECTED) != 0));
		}
		if (!rp && lpc_netif.ip_addr.addr) {
			static char tmp_buff[16];
			DEBUGOUT("IP_ADDR    : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.ip_addr, tmp_buff, 16));
			DEBUGOUT("NET_MASK   : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.netmask, tmp_buff, 16));
			DEBUGOUT("GATEWAY_IP : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.gw, tmp_buff, 16));
			Board_LED_Set(1, true);
			rp = 1;

			/* Jorge */
			IP4_ADDR(&ip, 54,191,0,152);    //IP of Juliano Server

			TCPConnect();
		}

		if (physts & PHY_LINK_CONNECTED)
		{
			uint32_t minute, sec;
			minute = Chip_RTC_GetTime(LPC_RTC, RTC_TIMETYPE_MINUTE);
			sec = Chip_RTC_GetTime(LPC_RTC, RTC_TIMETYPE_SECOND);
			if (!(minute % 5) && !sec && !_error)
			{
				TCPSendPacket();

				msDelay(1000);
			}
		}
	}

	/* Never returns, for warning only */
	return 0;
}
