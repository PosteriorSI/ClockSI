/*
 * lock.c
 *
 *  Created on: Dec 2, 2015
 *      Author: xiaoxin
 */
/*
 * interface to operations about locks on ProcArray and InvisibleTable.
 */
#include"lock.h"

/*
 * interface to hold the read-write-lock.
 */
void AcquireWrLock(pthread_rwlock_t* lock, LockMode mode)
{
    if(mode == LOCK_SHARED)
    {
        pthread_rwlock_rdlock(lock);
    }
    else
    {
        pthread_rwlock_wrlock(lock);
    }
}

/*
 * interface to release the read-write-lock.
 */
void ReleaseWrLock(pthread_rwlock_t* lock)
{
    pthread_rwlock_unlock(lock);
}


