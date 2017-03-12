#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "master.h"

int main()
{
    pid_t pid1, pid2;
    InitNetworkParam();

    if ((pid1 = fork()) < 0)
        printf("fork parameter server process error\n");

    if (pid1 == 0)
    {
        InitParam();
        //printf("parameter server end\n");
        exit(1);
    }

    if ((pid2 = fork()) < 0)
        printf("fork message server process error\n");

    if (pid2 == 0)
    {
        InitMessage();
        //printf("message server end\n");
        exit(1);
    }

    InitRecord();
    //printf("message server end\n");
    return 0;
}
