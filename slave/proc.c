/*
 * proc.c
 *
 *  Created on: 2015-11-9
 *      Author: DELL
 */

/*
 * process actions are defined here.
 */
#include<malloc.h>
#include<pthread.h>
#include<stdlib.h>

#include "proc.h"
#include "mem.h"
#include "thread_global.h"
#include "trans.h"
#include "lock.h"
#include "util.h"
#include "socket.h"
#include "thread_main.h"

PROCHEAD* prohd;

void InitProcHead(void)
{
   /* initialize the process array information. */
   prohd=(PROCHEAD*)malloc(sizeof(PROCHEAD));
   prohd->maxprocs=THREADNUM;
   prohd->numprocs=0;
}

void ResetProc(void)
{
    prohd->maxprocs=THREADNUM;
    prohd->numprocs=0;
}

void *ProcStart(void* args)
{
    int i;
    int j;
    terminalArgs *temp;
    temp = (terminalArgs*) args;
    char* start=NULL;
    THREAD* threadinfo;

    Size size;

    pthread_mutex_lock(&prohd->ilock);
    i=prohd->numprocs++;
    pthread_mutex_unlock(&prohd->ilock);

    start=(char*)MemStart+MEM_PROC_SIZE*i;

    size=sizeof(THREAD);

    threadinfo=(THREAD*)MemAlloc((void*)start,size);

    if(threadinfo==NULL)
    {
        printf("memory alloc error during process running.\n");
        exit(-1);
    }

    pthread_setspecific(ThreadInfoKey,threadinfo);

    /* global index for thread */
    threadinfo->index=nodeid*threadnum+i;
    threadinfo->memstart=(char*)start;

    ProcTransactionIdAssign(threadinfo);

    if(temp->type == 1 && i == 0)
       threadinfo->curid=thread_0_tid+1;
    else
       threadinfo->curid=threadinfo->index*MaxTransId+1;

    if (temp->type == 0)
    {
       InitClient(nodeid, i);
    }

    else
    {
       /* ensure the connect with other nodes in distributed system. */
       for (j = 0; j < nodenum; j++)
       {
          // j could be itself?
          InitClient(j, i);
       }
    }

    InitRandomSeed();

    /* memory allocation for each transaction data struct. */
    InitTransactionStructMemAlloc();

    TransactionRunSchedule(args);

    return NULL;
}

void *testProcStart(void* args)
{
    int i;
    int j;
    run_args *temp;
    temp = (run_args*) args;
    char* start=NULL;
    THREAD* threadinfo;

    Size size;

    pthread_mutex_lock(&prohd->ilock);
    i=prohd->numprocs++;
    pthread_mutex_unlock(&prohd->ilock);

    start=(char*)MemStart+MEM_PROC_SIZE*i;

    size=sizeof(THREAD);

    threadinfo=(THREAD*)MemAlloc((void*)start,size);

    if(threadinfo==NULL)
    {
        printf("memory alloc error during process running.\n");
        exit(-1);
    }

    pthread_setspecific(ThreadInfoKey,threadinfo);

    /* global index for thread */
    threadinfo->index=nodeid*threadnum+i;
    threadinfo->memstart=(char*)start;

    ProcTransactionIdAssign(threadinfo);

    if(temp->type == 1 && i == 0)
       threadinfo->curid=thread_0_tid+1;
    else
       threadinfo->curid=threadinfo->index*MaxTransId+1;

    if (temp->type == 0)
    {
       InitClient(nodeid, i);
    }

    else
    {
       /* ensure the connect with other nodes in distributed system. */
       for (j = 0; j < nodenum; j++)
       {
          // j could be itself?
          InitClient(j, i);
       }
    }

    InitRandomSeed();

    /* memory allocation for each transaction data struct. */
    InitTransactionStructMemAlloc();

    testTransactionRunSchedule(args);

    return NULL;
}
