/*
 * main.c
 *
 *  Created on: Dec 7, 2015
 *      Author: xiaoxin
 */
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "mem.h"
#include "thread_main.h"
#include "data_am.h"
#include "transactions.h"
#include "config.h"
#include "proc.h"
#include "socket.h"
#include "trans.h"

int main(int argc, char *argv[])
{
    pid_t pid;

    if (argc != 2)
    {
        printf("please enter the configure file's name\n");
    }
    if ((conf_fp = fopen(argv[1], "r")) == NULL)
    {
       printf("can not open the configure file.\n");
       fclose(conf_fp);
       return -1;
    }

    /* do some ready work before start the distributed system */
    GetReady();

    if ((pid = fork()) < 0)
    {
       printf("fork error\n");
    }

    else if(pid == 0)
    {
        /* storage process */
        InitStorage();
        StorageExitSys();
        //printf("storage process finished.\n");
    }

    else
    {
       /* transaction process */
       InitTransaction();
       dataLoading();
       //LoadData2();
       /* wait other slave nodes until all of them have loaded data. */
       WaitDataReady();
       /* begin run the benchmark. */
       RunTerminals(THREADNUM);
       //ThreadRun(THREADNUM);

       TransExitSys();
       //test_report();
       //printf("transaction process finished.\n");
    }
    return 0;
}

