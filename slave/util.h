/*
 * util.h
 *
 *  Created on: Jan 5, 2016
 *      Author: xiaoxin
 */

#ifndef UTIL_H_
#define UTIL_H_

extern int RandomByPid(int max);

extern void InitRandomSeed(void);

extern void SetRandomSeed(void);

extern int RandomNumber(int min, int max);

extern int GlobalRandomNumber(int min, int max);

extern int getCustomerID(void);

extern int getItemID(void);

#endif /* UTIL_H_ */
