/*
 * transactions.h
 *
 *  Created on: Jan 5, 2016
 *      Author: xiaoxin
 */

#ifndef TRANSACTIONS_H_
#define TRANSACTIONS_H_

#include <stdint.h>
#include "type.h"

//200,000(6) unique ID
#define ITEM_ID (uint64_t)1000000

//2*w unique ID
#define WHSE_ID (uint64_t)10000

//20 unique ID
#define DIST_ID (uint64_t)100

//96,000(5) unique ID
#define CUST_ID (uint64_t)100000

//10,000,000(8) unique ID
#define ORDER_ID (uint64_t) 1000000//100000000

//5-15 items per order.
#define ORDER_LINES (uint64_t)100

//quantity of each item is between (1, 10)
#define ITEM_QUANTITY 100

//'0' for bad credit, '1' for good credit.
#define CUST_CREDIT 10

//discount from 0 to 5.
#define CUST_DISCOUNT 10

/*
 * table_id: warehouse:0, item:1, stock:2, district:3, customer:4, history:5,
 * order:6, new_order:7, order_line:8.
 */
#define Warehouse_ID 0
#define Item_ID 1
#define Stock_ID 2
#define District_ID 3
#define Customer_ID 4
#define History_ID 5
#define Order_ID 6
#define NewOrder_ID 7
#define OrderLine_ID 8

//smallbank
#define Accounts_ID 0
#define Savings_ID 1
#define Checking_ID 2


typedef enum
{
    NEW_ORDER,
    PAYMENT,
    DELIVERY,
    ORDER_STATUS,
    STOCK_LEVEL,

    //smallbank
    AMALGAMATE,
    BALANCE,
    DEPOSITCHECKING,
    SENDPAYMENT,
    TRANSACTSAVINGS,
    WRITECHECK
}TransactionsType;

typedef struct TransState
{
    int trans_commit;
    int trans_abort;
    int NewOrder;
    int Payment;
    int Delivery;
    int Stock_level;
    int Order_status;

    int global_total;
    int global_abort;

    //smallbank
    int Amalgamate;
    int Balance;
    int DepositChecking;
    int SendPayment;
    int TransactSavings;
    int WriteCheck;

    int Amalgamate_C;
    int Balance_C;
    int DepositChecking_C;
    int SendPayment_C;
    int TransactSavings_C;
    int WriteCheck_C;
}TransState;

typedef enum
{
    TPCC,
    SMALLBANK
}BENCHMARK;

extern BENCHMARK benchmarkType;

extern int newOrderTransaction(int w_id, int d_id, int c_id, int o_ol_cnt, int o_all_local, int *itemIDs, int *supplierWarehouseIDs, int *orderQuantities, int* node_id, int node_num);

extern int paymentTransaction(int w_id, int c_w_id, int h_amount, int d_id, int c_d_id, int c_id, int* node_id, int node_num);

extern int deliveryTransaction(int w_id, int o_carrier_id, int* node_id, int node_num);

extern int orderStatusTransaction(int w_id, int d_id, int c_id, int* node_id, int node_num);

extern int stockLevelTransaction(int w_id, int d_id, int threshold, int* node_id, int node_num);

extern int testTransaction(int w_id, int d_id);

extern uint64_t LoadData(void);

extern void executeTransactions(int numTransactions, int terminalWarehouseID, int terminalDistrictID, TransState* StateInfo);

//smallbank
extern int SendPaymentTransaction(TupleId sendAcct, TupleId destAcct, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

extern int TransactSavingsTransaction(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

extern int WriteCheckTransaction(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

extern int AmalgamateTransaction(TupleId acctId0, TupleId acctId1, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

extern int BalanceTransaction(TupleId acctId, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

extern int DepositCheckingTransaction(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

extern void executeTransactionsBank(int numTransactions, TransState* StateInfo);

#endif /* TRANSACTIONS_H_ */
