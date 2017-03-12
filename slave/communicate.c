/*
 * communicate.c
 *
 *  Created on: Jan 21, 2016
 *      Author: Yu
 */

#include <sys/socket.h>
#include <stdlib.h>
#include "socket.h"
#include "communicate.h"
#include "thread_global.h"
#include "proc.h"

int Send(int conn, uint64_t* buffer, int num)
{
    THREAD* threadinfo;
    int i;

    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);

    int ret;
    ret=send(conn, buffer, num*sizeof(uint64_t), 0);

    if(ret == -1)
    {
        printf("send error: num=%d index=%d, ", num, threadinfo->index);
        for(i=0;i<num;i++)
        {
            printf("%4ld", *(buffer+i));
        }
        printf("\n");
        exit(-1);
    }
    return ret;
}

int Receive(int conn, uint64_t* buffer, int num)
{
    THREAD* threadinfo;
    int i;

    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);

    int ret;

    ret=recv(conn, buffer, num*sizeof(uint64_t), 0);

    if(ret==-1)
    {
        printf("recv error:num=%d, index=%d", num, threadinfo->index);
        for(i=0;i<num;i++)
        {
            printf("%4ld", *(buffer+i));
        }
        printf("\n");
        exit(-1);
    }
    return ret;
}

int SSend(int conn, uint64_t* buffer, int num)
{
    int i;

    int ret;
    ret=send(conn, buffer, num*sizeof(uint64_t), 0);

    if(ret == -1)
    {
        printf("server send error: num=%d, ", num);
        for(i=0;i<num;i++)
        {
            printf("%4ld", *(buffer+i));
        }
        printf("\n");
        exit(-1);
    }
    return ret;
}
