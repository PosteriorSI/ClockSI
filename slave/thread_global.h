/*
 * thread_global.h
 *
 *  Created on: Nov 11, 2015
 *      Author: xiaoxin
 */

#ifndef THREAD_GLOBAL_H_
#define THREAD_GLOBAL_H_

#include<pthread.h>

extern pthread_key_t* keyarray;

#define ThreadInfoKey keyarray[0]

#define ProcIdMgrKey keyarray[1]

#define TransactionDataKey keyarray[2]

#define DataMemKey keyarray[3]

#define DatalockMemKey keyarray[4]

#define SnapshotDataKey keyarray[5]

#define RandomSeedKey keyarray[6]

#define GlobalLockNum 20


extern void InitThreadGlobalKey(void);

#endif /* THREAD_GLOBAL_H_ */
