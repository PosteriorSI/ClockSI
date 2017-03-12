/*
 * config.h
 *
 *  Created on: Jan 5, 2016
 *      Author: xiaoxin
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/*
 * parameters configurations here.
 */

extern int configWhseCount;

extern  int configDistPerWhse;

extern int configCustPerDist;

extern int MaxBucketSize;

extern int configUniqueItems;

extern int configCommitCount;

extern int transactionsPerTerminal;
extern int paymentWeightValue;
extern int orderStatusWeightValue;
extern int deliveryWeightValue;
extern int stockLevelWeightValue;
extern int limPerMin_Terminal;

extern int NumTerminals;

extern int OrderMaxNum;

extern int MaxDataLockNum;

//smallbank
extern int configNumAccounts;
extern float scaleFactor;
extern int configAccountsPerBucket;

extern int FREQUENCY_AMALGAMATE;
extern int FREQUENCY_BALANCE;
extern int FREQUENCY_DEPOSIT_CHECKING;
extern int FREQUENCY_SEND_PAYMENT;
extern int FREQUENCY_TRANSACT_SAVINGS;
extern int FREQUENCY_WRITE_CHECK;


extern int MIN_BALANCE;
extern int MAX_BALANCE;

extern void InitConfig(void);

#endif /* CONFIG_H_ */
