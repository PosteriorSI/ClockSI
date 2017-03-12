/*
 * thread_global.c
 *
 *  Created on: Nov 11, 2015
 *      Author: xiaoxin
 */
#include<malloc.h>

#include"thread_global.h"
#include"type.h"

pthread_key_t* keyarray=NULL;

void InitThreadGlobalKey(void)
{
    Size size;
    int i;

    size=sizeof(pthread_key_t);

    keyarray=(pthread_key_t*)malloc(size*GlobalLockNum);

    if(keyarray==NULL)
    {
        printf("malloc error for keyarray.\n");
        return;
    }

    // initialize the pthread_key_t array.
    for(i=0;i<GlobalLockNum;i++)
    {
        pthread_key_create(&keyarray[i],NULL);
    }
}

