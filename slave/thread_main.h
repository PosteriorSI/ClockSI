/*
 * threadmain.h
 *
 *  Created on: Nov 11, 2015
 *      Author: xiaoxin
 */

#ifndef THREADMAIN_H_
#define THREADMAIN_H_

#include<stdint.h>
#include<semaphore.h>

#include"transactions.h"

typedef struct run_args
{
    int i;
    int type;
} run_args;

extern pthread_barrier_t barrier;

extern sem_t * wait_server;

extern void ThreadRun(int num);

extern void InitSys(void);

extern void InitSemaphore(void);

extern void TransExitSys(void);

extern void StorageExitSys(void);

extern void LoadData2(void);

extern void RunTerminals(int numTerminals);

extern void runTerminal(int terminalWarehouseID, int terminalDistrictID, pthread_t *tid, pthread_barrier_t *barrier, TransState* StateInfo);

extern void dataLoading(void);

extern void InitStorage();

extern void GetReady(void);

extern void InitTransaction(void);

#endif /* THREADMAIN_H_ */
