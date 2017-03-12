/*
 * timestamp.c
 *
 *  Created on: Dec 7, 2015
 *      Author: xiaoxin
 */
#include<stdlib.h>
#include"timestamp.h"
#include"config.h"
#include"util.h"
#include"socket.h"

int time_skew;

TimeStampTz GetCurrentTimestamp(void)
{
    TimeStampTz result;

    struct timeval tv;

    gettimeofday(&tv,NULL);

    result = (TimeStampTz)tv.tv_sec;

    result=result*USECS_PER_SEC+tv.tv_usec+time_skew;

    return result;
}

void InitTimeSkew(void)
{
    time_skew = RandomByPid(time_skew_max);
}
