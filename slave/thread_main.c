/*
 * threadmain.c
 *
 *  Created on: Nov 11, 2015
 *      Author: xiaoxin
 */
#include <sched.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>

#include"config.h"
#include"proc.h"
#include"mem.h"
#include"trans.h"
#include"thread_global.h"
#include"thread_main.h"
#include"util.h"
#include"data_record.h"
#include"lock_record.h"
#include"lock.h"
#include"data_am.h"
#include"socket.h"
#include"timestamp.h"

pthread_barrier_t barrier;

sem_t *wait_server;

TimeStampTz sessionStartTimestamp;

TimeStampTz sessionEndTimestamp;

uint64_t fastNewOrderCounter=0;

uint64_t transactionCount=0;

uint64_t transactionCommit=0;

uint64_t transactionAbort=0;

double tpmC, tpmTotal;

int SleepTime;

struct tm sessionStart, sessionEnd;

void EndReport(TransState* StateInfo, int terminals);

void EndReportBank(TransState* StateInfo, int terminals);


void InitSys(void)
{
    SleepTime=1;

    InitConfig();

    SetRandomSeed();

    // initialize the memory space to be used.
    InitMem();

    // initialize pthread_key_t array for thread global variables.
    InitThreadGlobalKey();

    InitRecord();
}

void ThreadRun(int nthreads)
{
    int i,err;

    pthread_t tid[nthreads];
    run_args argus[10];

    i=pthread_barrier_init(&barrier, NULL, nthreads);

    if(i != 0)
    {
        printf("[ERROR]Coundn't create the barrier\n");
        exit(-1);
    }

    for (i = 0; i < 10; i++)
    {
       argus[i].i = i+1;
       argus[i].type = 1;
    }
    /*
     * run threads here.
     */
    for(i=0;i<nthreads;i++)
    {
        err=pthread_create(&tid[i],NULL,testProcStart,(void*)(&argus[i]));
        if(err!=0)
        {
            printf("can't create thread:%s\n",strerror(err));
            return;
        }
    }

    for(i=0;i<nthreads;i++)
    {
        pthread_join(tid[i],NULL);
    }

    return;
}

void LoadData2(void)
{
   int err;
   // wait the the storage process successfully initialize the server.
   sem_wait(wait_server);
   sem_destroy(wait_server);

   printf("begin to load data......\n");
   pthread_t tid;
   run_args argus;

   argus.i = 0;
   argus.type = 0;

   err = pthread_create(&tid, NULL, testProcStart, (void*)(&argus));
   if(err!=0)
   {
      printf("can't create load data thread:%s\n",strerror(err));
      return;
   }

   pthread_join(tid, NULL);
}

void GetReady(void)
{
    // get the parameters from the configure file.
    InitNetworkParam();

    InitConfig();

    // connect the parameter server from the master node.
    InitParamClient();
    // get the parameters from the master node.
    GetParam();

    InitTimeSkew();

    InitRecordClient();
    /* connect the message server from the master node,
       the message server is used for inform the slave nodes
       that every nodes have loaded the data.
    */

    InitMessageClient();

    // the semaphore is used to synchronize the storage process and the transaction process.
    InitSemaphore();
}

void InitStorage()
{
    InitRecord();
    InitDatabaseState();
    InitServerData();
    InitServerBuffer();
    InitServer(nodeid);
}

void InitTransaction(void)
{
    SleepTime=1;

    SetRandomSeed();

    InitProcHead();

    InitMem();
    //initialize pthread_key_t array for thread global variables.
    InitThreadGlobalKey();

    InitClientBuffer();
}

void TransExitSys(void)
{
    FreeMem();
}

void StorageExitSys(void)
{
    int i;

    for(i=0;i<TABLENUM;i++)
    {
        free(TableList[i]);
    }

    freeState();
    freeLocalDataRecord();
}

// this semaphore is used by transaction process waiting for the storage process initialize the server.
void InitSemaphore(void)
{
    wait_server = sem_open("wait_server", O_RDWR|O_CREAT, 00777, 0);
}


void RunTerminals(int numTerminals)
{
    int i, j;
    int terminalWarehouseID, terminalDistrictID;

    int usedTerminal[configWhseCount][10];
    pthread_t tid[numTerminals];

    TransState* StateInfo=(TransState*)malloc(sizeof(TransState)*numTerminals);

    i=pthread_barrier_init(&barrier, NULL, numTerminals);

    if(i != 0)
    {
        printf("[ERROR]Coundn't create the barrier\n");
        exit(-1);
    }

    int cycle;

    for(i=0;i<configWhseCount;i++)
        for(j=0;j<10;j++)
            usedTerminal[i][j]=0;

    sessionStartTimestamp=GetCurrentTimestamp();

    printf("run terminals..\n");

    for(i=0;i<numTerminals;i++)
    {
        cycle=0;
        do
        {
            terminalWarehouseID=(int)GlobalRandomNumber(1, configWhseCount);
            terminalDistrictID=(int)GlobalRandomNumber(1, 10);
            cycle++;
        }while(usedTerminal[terminalWarehouseID-1][terminalDistrictID-1]);
        usedTerminal[terminalWarehouseID-1][terminalDistrictID-1]=1;

        printf("terminal %d is running, w_id=%d, d_id=%d\n",i,terminalWarehouseID,terminalDistrictID);

        runTerminal(terminalWarehouseID, terminalDistrictID, &tid[i], &barrier, &StateInfo[i]);
        sleep(SleepTime);

    }

    for(i=0;i<numTerminals;i++)
    {
        pthread_join(tid[i], NULL);
    }
    sessionEndTimestamp=GetCurrentTimestamp();

    pthread_barrier_destroy(&barrier);

    printf("begin report.\n");
    //EndReport(StateInfo, numTerminals);

    EndReportBank(StateInfo, numTerminals);
}

void runTerminal(int terminalWarehouseID, int terminalDistrictID, pthread_t *tid, pthread_barrier_t *barrier, TransState* StateInfo)
{
    int err;
    terminalArgs* args=(terminalArgs*)malloc(sizeof(terminalArgs));
    args->whse_id=terminalWarehouseID;
    args->dist_id=terminalDistrictID;
    args->type=1;

    args->barrier=barrier;
    args->StateInfo=StateInfo;

    err=pthread_create(tid,NULL,ProcStart,args);
    if(err!=0)
    {
        printf("can't create thread:%s\n",strerror(err));
        return;
    }
}

void dataLoading(void)
{
    int err;
    pthread_t tid;
    terminalArgs args;
    args.type=0;
    args.whse_id=100;
    args.dist_id=100;
    // wait the the storage process successfully initialize the server.
    sem_wait(wait_server);
    sem_destroy(wait_server);
    err=pthread_create(&tid, NULL, ProcStart, &args);
    if(err!=0)
    {
        printf("can't create thread:%s\n",strerror(err));
        return;
    }
    pthread_join(tid, NULL);
}

void EndReport(TransState* StateInfo, int terminals)
{
    int i;
    int min, sec, msec;

    int global_total=0, global_abort=0;
    for(i=0;i<terminals;i++)
    {
        transactionCommit+=StateInfo[i].trans_commit;
        transactionAbort+=StateInfo[i].trans_abort;
        fastNewOrderCounter+=StateInfo[i].NewOrder;

        global_total+=StateInfo[i].global_total;
        global_abort+=StateInfo[i].global_abort;

    }
    transactionCount=transactionCommit+transactionAbort;

    msec=(sessionEndTimestamp-sessionStartTimestamp)/1000-((terminals-1)*SleepTime*1000);


    tpmC=(double)((uint64_t)(6000000*fastNewOrderCounter)/msec)/100.0;
    tpmTotal=(double)((uint64_t)(6000000*transactionCount)/msec)/100.0;

    sec=(sessionEndTimestamp-sessionStartTimestamp)/1000000;
    min=sec/60;
    sec=sec%60;

    printf("tpmC = %.2lf\n",tpmC);
    printf("tpmTotal = %.2lf\n",tpmTotal);
    printf("session start at : %ld \n",sessionStartTimestamp);
    printf("session end at : %ld %ld\n",sessionEndTimestamp, sessionEndTimestamp-sessionStartTimestamp);
    printf("total rumtime is %d min, %d s\n", min, sec);
    printf("transactionCount:%d, transactionCommit:%d, transactionAbort:%d, %d %d, globaltotoal: %d global_abort: %d\n",transactionCount, transactionCommit, transactionAbort, fastNewOrderCounter, transactionCount, global_total, global_abort);
    for(i=0;i<terminals;i++)
    {
        printf("newOrder:%d, payment:%d, delivery:%d, orderStatus:%d, stockLevel:%d\n",StateInfo[i].NewOrder, StateInfo[i].Payment, StateInfo[i].Delivery, StateInfo[i].Order_status, StateInfo[i].Stock_level);
    }

    printf("sleeptime=%d, terminals=%d\n", SleepTime, terminals);

    in_addr_t help = inet_addr(local_ip);

    uint64_t buf[5];
    buf[0] = (uint64_t)help;
    buf[1] = (uint64_t)tpmC;
    buf[2] = (uint64_t)tpmTotal;
    buf[3] = transactionAbort;
    buf[4] = transactionCount;
    int ret;
    ret = send(recordfd, buf, sizeof(buf), 0);
    if (ret == -1)
    {
        printf("record send error\n");
    }

}

//smallbank
void EndReportBank(TransState* StateInfo, int terminals)
{
    printf("begin report\n");
    int i;
    int min, sec, msec;
    int global_total=0, global_abort=0;

    for(i=0;i<terminals;i++)
    {
        transactionCommit+=StateInfo[i].trans_commit;
        transactionAbort+=StateInfo[i].trans_abort;

        global_total+=StateInfo[i].global_total;
        global_abort+=StateInfo[i].global_abort;
    }
    transactionCount=transactionCommit+transactionAbort;

    msec=(sessionEndTimestamp-sessionStartTimestamp)/1000-(terminals-1)*SleepTime*1000;
    //msec=(sessionEndTimestamp-sessionStartTimestamp)/1000;
    //assert(msec > 0);
    tpmC=(double)((uint64_t)(6000000*transactionCommit)/msec)/100.0;
    tpmTotal=(double)((uint64_t)(6000000*transactionCount)/msec)/100.0;

    sec=(sessionEndTimestamp-sessionStartTimestamp)/1000000;
    min=sec/60;
    sec=sec%60;

    printf("tpmC = %.2lf\n",tpmC);
    printf("tpmTotal = %.2lf\n",tpmTotal);
    printf("session start at : %ld \n",sessionStartTimestamp);
    printf("session end at : %ld %ld\n",sessionEndTimestamp, sessionEndTimestamp-sessionStartTimestamp);
    printf("total rumtime is %d min, %d s\n", min, sec);
    printf("transactionCount:%d, transactionCommit:%d, transactionAbort:%d, %d %d, globaltotoal: %d global_abort: %d\n",transactionCount, transactionCommit, transactionAbort, fastNewOrderCounter, transactionCount, global_total, global_abort);
    for(i=0;i<terminals;i++)
    {
        printf("AMALGAMATE:%d %d, BALANCE:%d %d, DEPOSITCHECKING:%d %d, SENDPAYMENT:%d %d, TRANSACTSAVINGS:%d %d, WRITECHECK:%d %d\n",StateInfo[i].Amalgamate, StateInfo[i].Amalgamate_C, StateInfo[i].Balance, StateInfo[i].Balance_C, StateInfo[i].DepositChecking, StateInfo[i].DepositChecking_C, StateInfo[i].SendPayment, StateInfo[i].SendPayment_C, StateInfo[i].TransactSavings, StateInfo[i].TransactSavings_C, StateInfo[i].WriteCheck, StateInfo[i].WriteCheck_C);
    }

        printf("nodenum = %d sleeptime=%d, terminals=%d\n", nodenum, SleepTime, terminals);

        in_addr_t help = inet_addr(local_ip);

        uint64_t buf[5];
        buf[0] = (uint64_t)help;
        buf[1] = (uint64_t)tpmC;
        buf[2] = (uint64_t)tpmTotal;
        buf[3] = transactionAbort;
        buf[4] = transactionCount;
        int ret;
        ret = send(recordfd, buf, sizeof(buf), 0);
        if (ret == -1)
        {
            printf("record send error\n");
        }    
}

