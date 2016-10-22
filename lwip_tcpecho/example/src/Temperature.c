/*
 * Temperature.c
 *
 *  Created on: 24 de set de 2016
 *      Author: Athens
 */

#include "board.h"

#include "Temperature.h"


#define SHIELD_I2C          I2C1
#define IOX_INT_PORT        2
#define IOX_INT_PIN         2
#define IOX_INT_GPIO_PORT   5
#define IOX_INT_GPIO_PIN    2
#define IOX_MODE_FUNC    SCU_MODE_FUNC4

#define TEMPSEN_SLV_ADDR           0x4C    /* Temperature sensor Slave address */
#define IOX_SLV_ADDR               0x20    /* IO eXpansion slave address */
#define IMUSEN_SLV_ADDR            0x68    /* Bosch IMU Sensor slave address */

#define PININT_INDEX   0	/* PININT index used for GPIO mapping */
#define PININT_IRQ_HANDLER  GPIO0_IRQHandler	/* GPIO interrupt IRQ function name */
#define PININT_NVIC_NAME    PIN_INT0_IRQn	/* GPIO interrupt NVIC interrupt name */


/* Write data to io expansion register */
static void iox_write_reg(uint8_t reg, uint8_t val)
{
	uint8_t buf[2];
	buf[0] = reg;
	buf[1] = val;
	Chip_I2C_MasterSend(SHIELD_I2C, IOX_SLV_ADDR, buf, 2);
}

/* Read IO expansion register */
static int iox_read_reg(uint8_t reg)
{
	I2C_XFER_T xfer = {
		IOX_SLV_ADDR,
		0,
		1,
		0,
		1,
	};
	xfer.txBuff = xfer.rxBuff = &reg;
	Chip_I2C_MasterTransfer(SHIELD_I2C, &xfer);
	if (xfer.status != I2C_STATUS_DONE)
		return -1;
	return (int) reg;
}

/**
 * @brief	I2C Interrupt Handler
 * @return	None
 */
void I2C1_IRQHandler(void)
{
	Chip_I2C_MasterStateHandler(I2C1);
}

/**
 * @brief	I2C0 Interrupt handler
 * @return	None
 */
void I2C0_IRQHandler(void)
{
	Chip_I2C_MasterStateHandler(I2C0);
}

/**
 * @brief	Handle interrupt from GPIO pin or GPIO pin mapped to PININT
 * @return	Nothing
 */
void PININT_IRQ_HANDLER(void)
{
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(PININT_INDEX));
//	btn_rdy = 1;
}

/**
 * @brief	Initialize the IOX interface
 * @return	Nothing
 */
void iox_init(void)
{
	/* Start with LEDs off */
	iox_write_reg(3, 0xF0);

	/* Set LED Pins as output */
	iox_write_reg(7, 0x0F);

	/* Clear interrupt by reading inputs */
	iox_read_reg(0);
	iox_read_reg(1);

	/* Set pin back to GPIO (on some boards may have been changed to something
	   else by Board_Init()) */
	Chip_SCU_PinMuxSet(IOX_INT_PORT, IOX_INT_PIN,
					   (SCU_MODE_INBUFF_EN | SCU_MODE_PULLUP | IOX_MODE_FUNC) );

	/* Configure GPIO pin as input */
	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, IOX_INT_GPIO_PORT, IOX_INT_GPIO_PIN);

	/* Configure interrupt channel for the GPIO pin in SysCon block */
	Chip_SCU_GPIOIntPinSel(PININT_INDEX, IOX_INT_GPIO_PORT, IOX_INT_GPIO_PIN);

	/* Configure channel interrupt as edge sensitive and falling edge interrupt */
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(PININT_INDEX));
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(PININT_INDEX));
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(PININT_INDEX));

	/* Enable interrupt in the NVIC */
	NVIC_ClearPendingIRQ(PININT_NVIC_NAME);
	NVIC_SetPriority(PININT_NVIC_NAME, 3);
	NVIC_EnableIRQ(PININT_NVIC_NAME);
}


void TSInitialization(void)
{
	Board_I2C_Init(SHIELD_I2C);

	/* Initialize I2C */
	Chip_I2C_Init(SHIELD_I2C);
	Chip_I2C_SetClockRate(SHIELD_I2C, 400000);
	Chip_I2C_SetMasterEventHandler(SHIELD_I2C, Chip_I2C_EventHandler);
	NVIC_SetPriority(PININT_NVIC_NAME, 1);
	NVIC_EnableIRQ(SHIELD_I2C == I2C0 ? I2C0_IRQn : I2C1_IRQn);

	iox_init();
}


uint16_t TSRead()
{
//	const int y = 18;
	uint8_t buf[32] = {0};
	int16_t val;
	int dc; //, df;
	I2C_XFER_T xfer =
	{
		TEMPSEN_SLV_ADDR,
		0,
		1,
		0,
		2,
	};
	xfer.txBuff = xfer.rxBuff = &buf[0];
	Chip_I2C_MasterTransfer(SHIELD_I2C, &xfer);
	if (xfer.status != I2C_STATUS_DONE)
		return 0;
	val = (buf[0] << 8) | buf[1];
	DEBUGOUT("VAL = %d\r\n", val);
	val = (val >> 5);
	dc = (val * 1000) / 8;
//	df = ((dc * 9) / 500) + 320;
//	snprintf((char *) buf, 31, "TMP[C]: %d.%03d", dc / 1000, abs(dc) % 1000);
//	LCD_FillRect(3, y, LCD_X_RES - 3, y + 8, 0);
//	LCD_PutStrXY(3, y, (char *)buf);
//	snprintf((char *) buf, 31, "TMP[F]: %d.%01d%", df / 10, abs(df) % 10);
//	LCD_FillRect(3, y + 8 + 2, LCD_X_RES - 3, y + 16 + 2, 0);
//	LCD_PutStrXY(3, y + 8 + 2, (char *)buf);

	return dc;
}
