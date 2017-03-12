/*
 * lock.h
 *
 *  Created on: Dec 2, 2015
 *      Author: xiaoxin
 */

#ifndef LOCK_H_
#define LOCK_H_


#include <pthread.h>
#include "type.h"
#include "lock_record.h"

extern void AcquireWrLock(pthread_rwlock_t* lock, LockMode mode);

extern void ReleaseWrLock(pthread_rwlock_t* lock);
#endif /* LOCK_H_ */
