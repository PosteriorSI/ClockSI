/*
 * config.c
 *
 *  Created on: Jan 5, 2016
 *      Author: xiaoxin
 */

#include <stdlib.h>
#include <stdint.h>
#include "config.h"
#include "util.h"
#include "transactions.h"
#include "data.h"

#define TPCC_TABLENUM  9
#define SMALLBANK_TABLENUM 3

// number of warehouses
int configWhseCount;

// number of districts per warehouse
int configDistPerWhse;

// number of customers per district
int configCustPerDist;

// number of items
int configUniqueItems;

int MaxBucketSize;

// max number of tuples operated in one transaction
int configCommitCount;

// number of transactions per terminal
int transactionsPerTerminal;

// ratio of each transaction in one terminal
int paymentWeightValue;
int orderStatusWeightValue;
int deliveryWeightValue;
int stockLevelWeightValue;

int limPerMin_Terminal;

//make sure that 'NumTerminals' <= 'MAXPROCS'.
int NumTerminals;

//the limited max number of new orders for each district.
int OrderMaxNum;

//the max number of wr-locks held in one transaction.
int MaxDataLockNum;

//smallbank
int configNumAccounts;
float scaleFactor;
int configAccountsPerBucket;

int FREQUENCY_AMALGAMATE;
int FREQUENCY_BALANCE;
int FREQUENCY_DEPOSIT_CHECKING;
int FREQUENCY_SEND_PAYMENT;
int FREQUENCY_TRANSACT_SAVINGS;
int FREQUENCY_WRITE_CHECK;

int MIN_BALANCE;
int MAX_BALANCE;

void InitConfig(void)
{
	benchmarkType=TPCC;
	TABLENUM=TPCC_TABLENUM;
    //transPerTerminal
    transactionsPerTerminal=20000;

    //we didn't build index on tables, so range query in order-status and delivery transactions are very slow,
    //there we set 'orderStatusWeightValue' and 'deliveryWeightValue' to '0', so we actually didn't implement
    //those two transactions order-status transaction and delivery transaction.
    paymentWeightValue=43;
    orderStatusWeightValue=0;
    deliveryWeightValue=0;
    stockLevelWeightValue=4;

    limPerMin_Terminal=0;

    configWhseCount=1;
    configDistPerWhse=10;
    configCustPerDist=300;
    MaxBucketSize=1000000;
    configUniqueItems=1000;

    configCommitCount=60;

    OrderMaxNum=12000;

    MaxDataLockNum=80;

    //smallbank
   scaleFactor=0.1;
   configNumAccounts=(int)(scaleFactor*1000000);
   configAccountsPerBucket=10000;

   FREQUENCY_AMALGAMATE=15;
   FREQUENCY_BALANCE=15;
   FREQUENCY_DEPOSIT_CHECKING=15;
   FREQUENCY_SEND_PAYMENT=25;
   FREQUENCY_TRANSACT_SAVINGS=15;
   FREQUENCY_WRITE_CHECK=15;

   MIN_BALANCE=10000;
   MAX_BALANCE=50000;

   //hotspot control
   //HOTSPOT_PERCENTAGE=25;
   //HOTSPOT_FIXED_SIZE=100;
}
