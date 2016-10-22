/*
 * RTC.h
 *
 *  Created on: 24 de set de 2016
 *      Author: Athens
 */

#ifndef SRC_RTC_H_
#define SRC_RTC_H_

typedef void(*RTCEvent)();


void RTCInitialization(RTCEvent event);
void RTCTask();


#endif /* SRC_RTC_H_ */
