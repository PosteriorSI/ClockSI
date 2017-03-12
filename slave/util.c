/*
 * util.c
 *
 *  Created on: Jan 5, 2016
 *      Author: xiaoxin
 */
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <malloc.h>

#include "config.h"
#include "thread_global.h"

int RandomByPid(int max)
{
    // prevent the float point exception
    if (max == 0)
    {
        return 0;
    }

    else
    {
        srand(pthread_self());

        return (rand()%max);
    }
}

void InitRandomSeed(void)
{
    unsigned int *seed=(unsigned int*)malloc(sizeof(unsigned int));

    pthread_setspecific(RandomSeedKey, seed);

    *seed=(unsigned int)time(NULL);
}

void SetRandomSeed(void)
{
    srand((unsigned)time(NULL));
}

int RandomNumber(int min, int max)
{
    unsigned int* seed;

    seed=(unsigned int*)pthread_getspecific(RandomSeedKey);

    return rand_r(seed)%(max-min+1)+min;
}


int GlobalRandomNumber(int min, int max)
{
    return rand()%(max-min+1)+min;
}
int nonUniformRandom(int v, int min, int max)
{
    return (int)(((RandomNumber(0, v) | RandomNumber(min, max)) + RandomNumber(0, v))%(max-min+1)+min);
}

int getCustomerID(void)
{
    //change the parameter here.
    return nonUniformRandom(123, 1, configCustPerDist);
}

int getItemID(void)
{
    //change the parameter here.
    return nonUniformRandom(91, 1, configUniqueItems);
}

