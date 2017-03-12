/*
 * transactions.c
 *
 *  Created on: Jan 5, 2016
 *      Author: xiaoxin
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "config.h"
#include "util.h"
#include "transactions.h"
#include "data_am.h"
#include "trans.h"
#include "thread_global.h"
#include "data.h"
#include "socket.h"

//limitiations of redo times.
#define REDOLIMIT redo_limit

//smallbank extension
#define EXTENSIONLIMIT extension_limit

int GetMaxOid(int table_id, int w_id, int d_id, int c_id);

int GetMinOid(int table_id, int w_id, int d_id);

void executeTransaction(TransactionsType type, int terminalWarehouseID, int terminalDistrictID, int *pabort, int *pglobal);

int LoadWhse(int numWarehouses);

int LoadItem(int ItemCount);

int LoadStock(int numWarehouses, int ItemCount);

int LoadDist(int numWarehouses, int DistPerWhse);

int LoadCust(int numWarehouses, int DistPerWhse, int CustPerDist);

int LoadOrder(int numWarehouses, int DistPerWhse, int CustPerDist);

int ReadWhse(int numWarehouses);

int ReadItem(int ItemCount);

int ReadStock(int numWarehouses, int ItemCount);

int ReadDist(int numWarehouses, int DistPerWhse);

int ReadCust(int numWarehouses, int DistPerWhse, int CustPerDist);

int ReadOrder(int numWarehouses, int DistPerWhse, int CustPerDist);

//smallbank
int LoadBankData(void);

void executeTransactionBank(TransactionsType type, int *pabort, int *pglobal, TupleId* acctArr, int* nodeArr);

//smallbank extension
int SendPaymentTransactionExtension(TupleId sendAcct, TupleId destAcct, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

int TransactSavingsTransactionExtension(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

int WriteCheckTransactionExtension(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

int AmalgamateTransactionExtension(TupleId acctId0, TupleId acctId1, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

int BalanceTransactionExtension(TupleId acctId, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

int DepositCheckingTransactionExtension(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr);

void randomReadGen(TupleId* acctArr, int* nodeArr, int* node_id, int node_num);

/***************************Load Data******************************/
uint64_t LoadData(void)
{
    uint64_t totalRows=0;
    totalRows+=LoadWhse(configWhseCount);
    printf("loaded warehouses %ld rows\n",totalRows);

    totalRows+=LoadItem(configUniqueItems);
    printf("loaded item %ld rows\n",totalRows);

    totalRows+=LoadStock(configWhseCount, configUniqueItems);
    printf("loaded stock %ld rows\n",totalRows);

    totalRows+=LoadDist(configWhseCount, configDistPerWhse);
    printf("loaded district %ld rows\n",totalRows);

    totalRows+=LoadCust(configWhseCount, configDistPerWhse, configCustPerDist);
    printf("loaded customer %ld rows\n",totalRows);

    totalRows+=LoadOrder(configWhseCount, configDistPerWhse, configCustPerDist);

    printf("loaded total rows are %ld \n",totalRows);

    return totalRows;
}

/*
 * load data of warehouse table, the table_id of warehouse table is '0'.
 */
int LoadWhse(int numWarehouses)
{
    int i;
    uint64_t k;
    int table_id;
    TupleId tuple_id, value;
    int result, index;

    int w_id;

    int node_id;

    node_id=nodeid;

    k=0;
    table_id=Warehouse_ID;
    StartTransaction();
    for(i=1;i<=numWarehouses;i++)
    {
        w_id=i;
        tuple_id=(TupleId)w_id;
        value=0;
        result=Data_Insert(table_id, tuple_id, value, node_id);
        if(result==0)
        {
            printf("LoadWhse failed for %d.\n",i);
            exit(-1);
        }
        k++;
        if(k%configCommitCount==0)
        {
            result=PreCommit();
            if(result == 1)
            {
                CommitTransaction();
            }
            else
            {
                // current transaction has to rollback.
                AbortTransaction();
            }
            StartTransaction();
        }
    }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        //current transaction has to rollback.
        AbortTransaction();
    }

    return k;
}

/*
 * load data for item table, the table_id of item table is '1'.
 */
int LoadItem(int ItemCount)
{
    int i;
    uint64_t k;
    int table_id;
    TupleId tuple_id, value;
    int result, index;

    int i_id, price;

    int node_id;

    node_id=nodeid;

    table_id=Item_ID;
    k=0;

    StartTransaction();
    for(i=1;i<=ItemCount;i++)
    {
        i_id=i;
        tuple_id=(TupleId)i_id;
        // random price.
        price=(int)RandomNumber(1, 100);
        value=(TupleId)price;
        result=Data_Insert(table_id, tuple_id, value, node_id);

        if(result==0)
        {
            printf("LoadItem failed for %d.\n",i);
            exit(-1);
        }
        k++;
        if(k%configCommitCount==0)
        {
            result=PreCommit();
            if(result == 1)
            {
                CommitTransaction();
            }
            else
            {
                AbortTransaction();
            }
            StartTransaction();
        }
    }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

/*
 * load data for stock table, the table_id of stock table is '2'.
 */
int LoadStock(int numWarehouses, int ItemCount)
{
    int i, w;
    uint64_t k;
    int table_id;
    TupleId tuple_id, value;
    int result, index;

    int s_i_id, s_w_id, quantity;

    int node_id;

    node_id=nodeid;

    k=0;
    table_id=Stock_ID;
    StartTransaction();
    for(i=1;i<=ItemCount;i++)
        for(w=1;w<=numWarehouses;w++)
        {
            s_i_id=i;
            s_w_id=w;

            tuple_id=(TupleId)(s_i_id+(TupleId)s_w_id*ITEM_ID);
            quantity=RandomNumber(10, 100);

            value=(TupleId)quantity;
            result=Data_Insert(table_id, tuple_id, value, node_id);
            if(result==0)
            {
                printf("LoadStock failed for s_i_id:%d, s_w_id:%d.\n",s_i_id, s_w_id);
                exit(-1);
            }
            k++;
            if(k%configCommitCount==0)
            {
                result=PreCommit();
                if(result == 1)
                {
                    CommitTransaction();
                }
                else
                {
                    AbortTransaction();
                }
                StartTransaction();
            }
        }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

/*
 * load data for district table, the table_id of district table is '3'.
 */
int LoadDist(int numWarehouses, int DistPerWhse)
{
    int d_id, d_w_id, d_next_o_id;
    int d, w, result, index;
    int table_id;
    uint64_t k;
    TupleId tuple_id, value;

    int node_id;

    node_id=nodeid;

    table_id=District_ID;
    k=0;

    StartTransaction();
    for(w=1;w<=numWarehouses;w++)
        for(d=1;d<=DistPerWhse;d++)
        {
            d_w_id=w;
            d_id=d;
            d_next_o_id=configCustPerDist+1;

            tuple_id=(TupleId)(d_w_id+(TupleId)d_id*WHSE_ID);
            value=(TupleId)d_next_o_id;
            result=Data_Insert(table_id, tuple_id, value, node_id);
            if(result==0)
            {
                printf("LoadDist failed for d_id:%d, d_w_id:%d.\n",d_id, d_w_id);
                exit(-1);
            }
            k++;

            if(k%configCommitCount==0)
            {
                result=PreCommit();
                if(result == 1)
                {
                    CommitTransaction();
                }
                else
                {
                    AbortTransaction();
                }
                StartTransaction();
            }
        }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

/*
 * load data for customer table, the table_id of customer table is '4'.
 */
int LoadCust(int numWarehouses, int DistPerWhse, int CustPerDist)
{
    int w,d,c;
    int c_id, c_w_id, c_d_id, c_discount, c_credit, c_balance, c_payment_cnt, c_delivery_cnt;
    int table_id, result, index;
    uint64_t k;
    TupleId cust_id, cust_value;
    TupleId hist_id, hist_value;

    int h_c_id, h_c_w_id, h_c_d_id, h_w_id, h_d_id, h_amount;

    int node_id;

    node_id=nodeid;

    table_id=Customer_ID;
    k=0;

    StartTransaction();
    for(w=1;w<=numWarehouses;w++)
        for(d=1;d<=DistPerWhse;d++)
            for(c=1;c<=CustPerDist;c++)
            {
                c_id=c;
                c_d_id=d;
                c_w_id=w;

                cust_id=(TupleId)(c_id+c_w_id*CUST_ID+(TupleId)c_d_id*CUST_ID*WHSE_ID);
                // to compute.
                // discount from 1 to 5.
                c_discount=RandomNumber(1,5);

                if(RandomNumber(1,100) <= 90)// 90% good credit.
                {
                    c_credit=1;
                }
                else//10% bad credit.
                {
                    c_credit=0;
                }

                c_balance=0;

                c_payment_cnt=1;
                c_delivery_cnt=0;

                h_c_id=c;
                h_c_w_id=w;
                h_c_d_id=d;
                h_w_id=w;
                h_d_id=d;
                h_amount=10;

                cust_value=(TupleId)(c_credit+(TupleId)c_discount*CUST_CREDIT+(TupleId)c_balance*CUST_CREDIT*CUST_DISCOUNT);

                result=Data_Insert(Customer_ID, cust_id, cust_value, node_id);
                if(result==0)
                {
                    printf("LoadCust failed for cust_id:%ld, c_id:%d, c_d_id:%d, c_w_id:%d.\n",cust_id, c_id, c_d_id, c_w_id);
                    validation(Customer_ID);
                    exit(-1);
                }

                hist_id=(TupleId)(h_c_id+h_c_w_id*CUST_ID+(TupleId)h_c_d_id*CUST_ID*WHSE_ID);
                hist_value=(TupleId)(h_w_id+h_d_id*WHSE_ID+(TupleId)h_amount*WHSE_ID*DIST_ID);

                result=Data_Insert(History_ID, hist_id, hist_value, node_id);
                if(result==0)
                {
                    printf("LoadHist failed for hist_id:%ld, h_c_id:%d, h_c_w_id:%d, h_c_d_id:%d.\n",hist_id, h_c_id, h_c_w_id, h_c_d_id);
                    exit(-1);
                }
                k+=2;
                if(k%configCommitCount==0)
                {
                    result=PreCommit();
                    if(result == 1)
                    {
                        CommitTransaction();
                    }
                    else
                    {
                        AbortTransaction();
                    }
                    StartTransaction();
                }
            }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

int LoadOrder(int numWarehouses, int DistPerWhse, int CustPerDist)
{
    TupleId no_id, no_value;
    TupleId oo_id, oo_value;
    TupleId ol_id, ol_value;

    uint64_t k;
    int o_id, o_w_id,  o_d_id, o_c_id, o_carrier_id, o_ol_cnt;
    int no_w_id, no_d_id, no_o_id;
    int ol_o_id, ol_d_id, ol_w_id, ol_number, ol_i_id, ol_amount, ol_supply_w_id, ol_quantity;
    int w, d, c, l;
    int result, index;
    int Neworder_Count=0;

    int node_id;

    node_id=nodeid;

    k=0;

    StartTransaction();
    for(w=1;w<=numWarehouses;w++)
        for(d=1;d<=DistPerWhse;d++)
            for(c=1;c<=CustPerDist;c++)
            {
                o_id=c;
                o_w_id=w;
                o_d_id=d;
                o_c_id=RandomNumber(1, CustPerDist);
                o_carrier_id=RandomNumber(1, 10);
                o_ol_cnt=RandomNumber(5, 10);

                oo_id=(TupleId)(o_id+(TupleId)o_w_id*ORDER_ID+(TupleId)o_d_id*ORDER_ID*WHSE_ID);
                oo_value=(TupleId)(o_c_id+o_ol_cnt*CUST_ID+(TupleId)o_carrier_id*CUST_ID*ORDER_LINES);
                result=Data_Insert(Order_ID, oo_id, oo_value, node_id);

                if(result==0)
                {
                    printf("LoadOrder failed for o_c_id:%d, o_w_id:%d, o_d_id:%d.\n",o_c_id, o_w_id, o_d_id);
                    validation(Order_ID);
                    // break;
                    exit(-1);
                }

                k++;
                // add to avoid too many rows in one transaction.
                if(k%configCommitCount==0)
                {
                    result=PreCommit();
                    if(result == 1)
                    {
                        CommitTransaction();
                    }
                    else
                    {
                        // we should know from which DataRecord to roll back.
                        AbortTransaction();
                    }
                    StartTransaction();
                }

                // 900 rows in the NEW-ORDER table corresponding to the last
                // 900 rows in the ORDER table for that district (i.e., with
                // NO_O_ID between 2,101 and 3,000)
                if(c > 2100)
                //if(c > 210)
                {
                    no_w_id=w;
                    no_d_id=d;
                    no_o_id=c;

                    no_id=(TupleId)(no_o_id+(TupleId)no_w_id*ORDER_ID+(TupleId)no_d_id*ORDER_ID*WHSE_ID);
                    no_value=0;
                    result=Data_Insert(NewOrder_ID, no_id, no_value, node_id);

                    if(result==0)
                    {
                        printf("LoadnewOrder failed for no_o_id:%d, no_w_id:%d, no_d_id:%d.\n",no_o_id, no_w_id, no_d_id);
                        exit(-1);
                    }
                    Neworder_Count++;
                    k++;
                    // add to avoid too many rows in one transaction.
                    if(k%configCommitCount==0)
                    {
                        result=PreCommit();
                        if(result == 1)
                        {
                            CommitTransaction();
                        }
                        else
                        {
                            AbortTransaction();
                        }
                        StartTransaction();
                    }
                }

                for(l=1;l<=o_ol_cnt;l++)
                {
                    ol_o_id=c;
                    ol_w_id=w;
                    ol_d_id=d;
                    ol_number=l;
                    ol_i_id=RandomNumber(1,configUniqueItems);

                    ol_supply_w_id=RandomNumber(1, numWarehouses);
                    ol_quantity=5;

                    if(ol_o_id < 2101)
                    {
                        ol_amount=0;
                    }
                    else
                    {
                        ol_amount=RandomNumber(1, 1000);
                    }
                    ol_id=(TupleId)(ol_o_id+(TupleId)ol_w_id*ORDER_ID+(TupleId)ol_d_id*ORDER_ID*WHSE_ID+(TupleId)ol_number*ORDER_ID*WHSE_ID*DIST_ID);
                    ol_value=(TupleId)(ol_i_id+(TupleId)ol_supply_w_id*ITEM_ID+(TupleId)ol_quantity*ITEM_ID*WHSE_ID+(TupleId)ol_amount*ITEM_ID*WHSE_ID*ITEM_QUANTITY);
                    result=Data_Insert(OrderLine_ID, ol_id, ol_value, node_id);

                    if(result==0)
                    {
                        printf("LoadorderLinefailed for ol_o_id:%d, ol_w_id:%d, ol_d_id:%d, ol_number:%d.\n",ol_o_id, ol_w_id, ol_d_id, ol_number);
                        exit(-1);
                    }

                    k++;

                    if(k%configCommitCount==0)
                    {
                        result=PreCommit();
                        if(result == 1)
                        {
                            CommitTransaction();
                        }
                        else
                        {
                            AbortTransaction();
                        }
                        StartTransaction();
                    }
                }
            }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }
    return k;
}

/***************************Transaction Interface******************************/
void executeTransactions(int numTransactions, int terminalWarehouseID, int terminalDistrictID, TransState* StateInfo)
{
    int transactionWeight;
    int i;
    int result;

    int is_global;
    int abort;

    int newOrder, newOrderCount=0;

    int count[5]={0};
    int flag;

    //getchar();
    StateInfo->trans_commit=0;
    StateInfo->trans_abort=0;
    StateInfo->Delivery=0;
    StateInfo->NewOrder=0;
    StateInfo->Order_status=0;
    StateInfo->Stock_level=0;
    StateInfo->Payment=0;

    StateInfo->global_total=0;
    StateInfo->global_abort=0;

    for(i=0;i<numTransactions;i++)
    {
        transactionWeight=(int)RandomNumber(1, 100);
        newOrder=0;

        if(transactionWeight <= paymentWeightValue)
        {
            count[0]++;
            executeTransaction(PAYMENT, terminalWarehouseID, terminalDistrictID, &abort, &is_global);
            StateInfo->Payment++;
        }
        else if(transactionWeight <= paymentWeightValue + stockLevelWeightValue)
        {
            count[1]++;
            executeTransaction(STOCK_LEVEL, terminalWarehouseID, terminalDistrictID, &abort, &is_global);
            StateInfo->Stock_level++;
        }
        else if(transactionWeight <= paymentWeightValue + stockLevelWeightValue + orderStatusWeightValue)
        {
            count[2]++;
            executeTransaction(ORDER_STATUS, terminalWarehouseID, terminalDistrictID, &abort, &is_global);
            StateInfo->Order_status++;
        }
        else if(transactionWeight <= paymentWeightValue + stockLevelWeightValue + orderStatusWeightValue + deliveryWeightValue)
        {
            count[3]++;
            executeTransaction(DELIVERY, terminalWarehouseID, terminalDistrictID, &abort, &is_global);
            StateInfo->Delivery++;
        }
        else
        {
            count[4]++;
            executeTransaction(NEW_ORDER, terminalWarehouseID, terminalDistrictID, &abort, &is_global);
            newOrderCount++;
            newOrder=1;
            if(abort!=REDOLIMIT)
                StateInfo->NewOrder++;
        }

        if (abort < REDOLIMIT)
        {
            StateInfo->trans_commit++;
            StateInfo->trans_abort += abort;
            if (is_global)
            {
                StateInfo->global_abort += abort;
                StateInfo->global_total += (abort+1);
            }
        }
        else
        {
            StateInfo->trans_abort += REDOLIMIT;
            if (is_global)
            {
               StateInfo->global_abort += REDOLIMIT;
               StateInfo->global_total += REDOLIMIT;
            }
        }
    }
}

/*
 * @return:'0' for commit, '-1' for abort.
 */
void executeTransaction(TransactionsType type, int terminalWarehouseID, int terminalDistrictID, int *pabort, int *pglobal)
{
    int i;
    int result;

    int abort = 0;
    bool distributed=true;

    int redo;

    int districtID, customerID, numItems;
    int allLocal;
    int itemIDs[15], supplierWarehouseIDs[15], orderQuantities[15];

    int node_id[5], node_num, nid;
    int node_used[NODENUMMAX]={0};

    int LocalWeight;

    LocalWeight=(int)RandomNumber(1,100);

    if(LocalWeight <= oneNodeWeight)
    {
        node_num=1;
        distributed=false;
    }
    else if(LocalWeight <= (oneNodeWeight+twoNodeWeight))
        node_num=2;
    else
        node_num=3;

    node_id[0]=nodeid;
    node_used[nodeid]=1;

    if(node_num > nodenum)
        node_num=nodenum;

    for(i=1;i<node_num;i++)
    {
        do
        {
            nid=(int)RandomNumber(1,nodenum);
        }while(node_used[nid-1]);
        node_used[nid-1]=1;
        node_id[i]=nid-1;
    }

    // PAYMENT
    int customerDistrictID, customerWarehouseID;
    int v;
    int paymentAmount;

    // STOCK_LEVEL
    int threshold;

    // DELIVERY
    int orderCarrierID;

    switch(type)
    {
    case NEW_ORDER:
        districtID=RandomNumber(1, configDistPerWhse);
        customerID=getCustomerID();
        allLocal=1;

        numItems=RandomNumber(5, 10);

        for(i=0;i<numItems;i++)
        {
            itemIDs[i]=getItemID();

            if(RandomNumber(1, 100) > 1)
            {
                supplierWarehouseIDs[i]=terminalWarehouseID;
            }
            else
            {
                do
                {
                    supplierWarehouseIDs[i]=RandomNumber(1, configWhseCount);
                }while(supplierWarehouseIDs[i]==terminalWarehouseID && configWhseCount > 1);
                allLocal=0;
            }
            orderQuantities[i]=RandomNumber(1, 10);
        }

        if(RandomNumber(1, 100) == 1)
        {
            itemIDs[numItems-1]=-1;
        }
        for(redo=0;redo<REDOLIMIT;redo++)
        {
            result=newOrderTransaction(terminalWarehouseID, districtID, customerID, numItems, allLocal, itemIDs, supplierWarehouseIDs, orderQuantities, node_id, node_num);
            if(result==0)
                break;
            else
            {
                abort++;
            }
        }
        break;
    case PAYMENT:
        districtID=RandomNumber(1, 10);
        v=RandomNumber(1, 100);

        if(v <= 85)
        {
            customerDistrictID=districtID;
            customerWarehouseID=terminalWarehouseID;
        }
        else
        {
            customerDistrictID=RandomNumber(1, 10);
            do
            {
                customerWarehouseID=RandomNumber(1, configWhseCount);
            }while(customerWarehouseID == terminalWarehouseID && configWhseCount > 1);
        }

        // 100% lookup by customerID.
        customerID=getCustomerID();

        paymentAmount=RandomNumber(1, 5000);
        for(redo=0;redo<REDOLIMIT;redo++)
        {
            result=paymentTransaction(terminalWarehouseID, customerWarehouseID, paymentAmount, districtID, customerDistrictID, customerID, node_id, node_num);
            if(result==0)
                break;
            else
            {
                abort++;
            }
        }
        break;
    case STOCK_LEVEL:
        threshold=RandomNumber(10, 20);
        for(redo=0;redo<REDOLIMIT;redo++)
        {
            result=stockLevelTransaction(terminalWarehouseID, terminalDistrictID, threshold, node_id, node_num);
            if(result==0)
                break;
            else
            {
                abort++;
            }
        }
        break;
    case ORDER_STATUS:
        districtID=RandomNumber(1, 10);
        // districtID=1;
        customerID=getCustomerID();
        for(redo=0;redo<REDOLIMIT;redo++)
        {
            result=orderStatusTransaction(terminalWarehouseID, districtID, customerID, node_id, node_num);
            if(result==0)
                break;
            else
            {
                abort++;
            }
        }
        break;
    case DELIVERY:
        orderCarrierID=RandomNumber(1, 10);
        for(redo=0;redo<REDOLIMIT;redo++)
        {
            result=deliveryTransaction(terminalWarehouseID, orderCarrierID, node_id, node_num);
            if(result==0)
                break;
            else
            {
                abort++;
            }
        }
        break;
    default:
        result=testTransaction(terminalWarehouseID, terminalDistrictID);
    }
    if(distributed)
        *pglobal = true;
    else
        *pglobal = false;

    *pabort = abort;
}
/*
 * new order transaction.
 */
int newOrderTransaction(int w_id, int d_id, int c_id, int o_ol_cnt, int o_all_local, int *itemIDs, int *supplierWarehouseIDs, int *orderQuantities, int* node_id, int node_num)
{
    StartTransaction();

    TupleId whse_id, cust_id, whse_value, cust_value;
    TupleId dist_id, dist_value;
    TupleId no_id, no_value;
    TupleId oorder_id, oorder_value;
    TupleId item_id, item_value;
    TupleId ol_id, ol_value;
    TupleId s_id, s_value;

    int c_discount;
    int o_carrier_id;
    int d_next_o_id, o_id;
    int i_price;
    int ol_number, ol_supply_w_id, ol_i_id, ol_quantity, ol_amount;
    int s_quantity;

    int result, index;
    bool newOrderRowInserted;
    int transaction_result;
    int flag;

    int node_id1, node_id2, node_id3;

    switch(node_num)
    {
    case 1:
        node_id1=node_id[0];
        node_id2=node_id[0];
        node_id3=node_id[0];
        break;
    case 2:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[1];
        break;
    case 3:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[2];
        break;
    default:
        printf("node num error. %d\n", node_num);
    }

    whse_id=(TupleId)w_id;
    cust_id=(TupleId)(c_id+w_id*CUST_ID+(TupleId)d_id*CUST_ID*WHSE_ID);

    whse_value=Data_Read(Warehouse_ID, whse_id, node_id1, &flag);
    if(flag==0)
    {
        printf("stmtGetCustWhse() not found, whse_id=%ld\n",whse_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    cust_value=Data_Read(Customer_ID, cust_id, node_id1, &flag);
    if(flag==0)
    {
        printf("stmtGetCustWhse() not found, cust_id=%ld\n",cust_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    //random value for 'c_discount, c_last, c_credit, w_tax'.
    c_discount=(int)((cust_value/CUST_CREDIT)%CUST_DISCOUNT);

    newOrderRowInserted=false;
    while(!newOrderRowInserted)
    {
        dist_id=(TupleId)(w_id+d_id*WHSE_ID);

        dist_value=Data_Read(District_ID, dist_id, node_id1, &flag);
        if(flag==0)
        {
            printf("stmtGetDist() not found, dist_id=%ld\n", dist_id);
            exit(-1);
        }
        else if(flag==-3)
        {
            AbortTransaction();
            return -1;
        }

        d_next_o_id=(int)(dist_value%ORDER_ID);

        if(d_next_o_id < configCustPerDist)
        {
            printf("d_next_o_id=%d, dist_value=%ld, dist_id=%ld\n",d_next_o_id, dist_value, dist_id);
            //PrintTable(District_ID);
            exit(-1);
        }

        // random value for 'd_tax'.
        o_id=d_next_o_id;

        no_id=(TupleId)(o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID);
        no_value=0;

        result=Data_Insert(NewOrder_ID, no_id, no_value, node_id1);
        if(result > 0)
            newOrderRowInserted=true;
        break;
    }

    d_next_o_id=d_next_o_id+1;
    dist_value=(TupleId)d_next_o_id;

    result=Data_Update(District_ID, dist_id, dist_value, node_id1);
    if(result == 0)
    {
        printf("stmtUpdateDist() update failed, dist_id=%ld\n",dist_id);
        exit(-1);
    }

    o_carrier_id=0;
    oorder_id=(TupleId)(o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID);
    oorder_value=(TupleId)(c_id+o_ol_cnt*CUST_ID+(TupleId)o_carrier_id*CUST_ID*ORDER_LINES);

    result=Data_Insert(Order_ID, oorder_id, oorder_value, node_id1);
    if(result == 0)
    {
        AbortTransaction();
        return -1;
    }

    for(ol_number=1;ol_number<=o_ol_cnt;ol_number++)
    {
        ol_supply_w_id = supplierWarehouseIDs[ol_number-1];
        ol_i_id = itemIDs[ol_number-1];
        ol_quantity = orderQuantities[ol_number-1];

        if(ol_i_id < 0)
        {
            /*
             * to rollback here.
             */
            AbortTransaction();
            return -1;
        }

        item_id=(TupleId)ol_i_id;
        item_value=Data_Read(Item_ID, item_id, node_id2, &flag);
        if(flag == 0)
        {
            printf("stmtGetItem() not found, item_id=%ld\n",item_id);
            exit(-1);
        }
        else if(flag==-3)
        {
            AbortTransaction();
            return -1;
        }

        i_price=(int)(item_value);

        s_id=(TupleId)(ol_i_id+(TupleId)ol_supply_w_id*ITEM_ID);
        s_value=Data_Read(Stock_ID, s_id, node_id2, &flag);
        if(flag == 0)
        {
            printf("stmtGetStock() not found, s_id=%ld\n",s_id);
            exit(-1);
        }
        else if(flag==-3)
        {
            AbortTransaction();
            return -1;
        }

        s_quantity=(int)(s_value);

        if(s_quantity - ol_quantity >= 10) {
            s_quantity -= ol_quantity;
        } else {
            s_quantity = s_quantity - ol_quantity + 91;
        }

        //remote_cnt here.
        s_value=(TupleId)(s_quantity);
        result=Data_Update(Stock_ID, s_id, s_value, node_id2);
        if(result == 0)
        {
            printf("stmtUpdateStock() update failed, s_id=%ld\n",s_id);
            exit(-1);
        }

        ol_amount=ol_quantity * i_price;

        //stmtInsertOrderLine
        ol_id=(TupleId)(o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID+(TupleId)ol_number*ORDER_ID*WHSE_ID*DIST_ID);
        ol_value=(TupleId)(ol_i_id+(TupleId)ol_supply_w_id*ITEM_ID+(TupleId)ol_quantity*ITEM_ID*WHSE_ID+(TupleId)ol_amount*ITEM_ID*WHSE_ID*ITEM_QUANTITY);

        result=Data_Insert(OrderLine_ID, ol_id, ol_value, node_id1);
        if(result == 0)
        {
            AbortTransaction();
            return -1;
        }
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;
}

/*
 * payment transaction.
 */
int paymentTransaction(int w_id, int c_w_id, int h_amount, int d_id, int c_d_id, int c_id, int* node_id, int node_num)
{
    StartTransaction();

    TupleId whse_id, whse_value;
    TupleId dist_id, dist_value;
    TupleId cust_id, cust_value;
    TupleId hist_id, hist_value;

    int c_credit, c_discount, c_balance;
    int h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id;
    int result, index;
    int transaction_result;
    int flag;

    int node_id1, node_id2, node_id3;

    switch(node_num)
    {
    case 1:
        node_id1=node_id[0];
        node_id2=node_id[0];
        node_id3=node_id[0];
        break;
    case 2:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[1];
        break;
    case 3:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[2];
        break;
    default:
        printf("node num error. %d\n", node_num);
    }

    //payUpdateWhse
    whse_id=(TupleId)w_id;
    whse_value=(TupleId)h_amount;

    result=Data_Update(Warehouse_ID, whse_id, whse_value, node_id1);
    if(result == 0)
    {
        printf("payUpdateWhse() update failed, whse_id=%ld\n",whse_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    //payGetWhse
    whse_value=Data_Read(Warehouse_ID, whse_id, node_id1, &flag);
    if(flag==0)
    {
        printf("payGetWhse() not found,whse_id=%ld\n",whse_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    //payUpdateDist
    dist_id=(TupleId)(w_id+d_id*WHSE_ID);
    dist_value=(TupleId)h_amount;

    //payGetDist
    dist_value=Data_Read(District_ID, dist_id, node_id1, &flag);
    if(flag==0)
    {
        printf("payGetDist() not found,dist_id=%ld\n",dist_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    // payment is by customer ID

    //payGetCust
    cust_id=(TupleId)(c_id+c_w_id*CUST_ID+(TupleId)c_d_id*CUST_ID*WHSE_ID);
    cust_value=Data_Read(Customer_ID, cust_id, node_id2, &flag);
    if(flag == 0)
    {
        printf("payGetCust() not found, cust_id=%ld\n",cust_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    //c_credit+c_discount*CUST_CREDIT+c_balance*CUST_CREDIT*CUST_DISCOUNT
    c_credit=(int)(cust_value%CUST_CREDIT);

    cust_value=(cust_value-c_credit)/CUST_CREDIT;
    c_discount=(int)(cust_value%CUST_DISCOUNT);

    cust_value=(cust_value-c_discount)/CUST_DISCOUNT;
    c_balance=(int)(cust_value);

    c_balance+=h_amount;

    if(c_credit == 0)//bad credit
    {
        //payGetCustCdata
        cust_value=Data_Read(Customer_ID, cust_id, node_id2, &flag);
        if(flag == 0)
        {
            printf("payGetCust() not found, cust_id=%ld\n",cust_id);
            exit(-1);
        }
        else if(flag==-3)
        {
            AbortTransaction();
            return -1;
        }

        //payUpdateCustBalCdata
        cust_value=(TupleId)(c_credit+(TupleId)c_discount*CUST_CREDIT+(TupleId)c_balance*CUST_CREDIT*CUST_DISCOUNT);
        result=Data_Update(Customer_ID, cust_id, cust_value, node_id2);
        if(result == 0)
        {
            printf("payUpdateCustBalCdata() update failed, cust_id=%ld\n",cust_id);
            exit(-1);
        }

    }
    else//good credit
    {
        //payUpdateCustBal
        cust_value=(TupleId)(c_credit+c_discount*CUST_CREDIT+(TupleId)c_balance*CUST_CREDIT*CUST_DISCOUNT);
        Data_Update(Customer_ID, cust_id, cust_value, node_id2);
        if(result == 0)
        {
            printf("payUpdateCustBal() update failed, cust_id=%ld\n",cust_id);
            exit(-1);
        }
    }

    //payInsertHist
    hist_id=(TupleId)(c_id+(TupleId)c_w_id*CUST_ID+(TupleId)c_d_id*CUST_ID*WHSE_ID);
    hist_value=(TupleId)(w_id+d_id*WHSE_ID);

    result=Data_Insert(History_ID, hist_id, hist_value, node_id3);

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }
    return transaction_result;
}

/*
 *
 */
int deliveryTransaction(int w_id, int o_carrier_id, int* node_id, int node_num)
{
    StartTransaction();

    TupleId no_id, no_value;
    TupleId oo_id, oo_value;
    TupleId ol_id, ol_value, part_ol_id;
    TupleId cust_id, cust_value;

    int no_o_id, no_d_id, no_w_id, d_id, c_id;
    int o_c_id, o_ol_cnt;
    int ol_o_id, ol_w_id, ol_d_id, ol_number, ol_amount, ol_total;
    int c_balance;
    bool newOrderRemoved;
    int result, index;
    int skippedDeliveries=0;
    int transaction_result;
    int flag;

    int node_id1, node_id2, node_id3;

    switch(node_num)
    {
    case 1:
        node_id1=node_id[0];
        node_id2=node_id[0];
        node_id3=node_id[0];
        break;
    case 2:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[1];
        break;
    case 3:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[2];
        break;
    default:
        printf("node num error. %d\n", node_num);
    }

    no_w_id=w_id;
    for(d_id=1;d_id<=10;d_id++)
    {
        no_d_id=d_id;

        no_o_id=-1;
        no_o_id=GetMinOid(NewOrder_ID, w_id, no_d_id);


        if(no_o_id > 0)
        {
            no_id=(TupleId)(no_o_id+(TupleId)no_w_id*ORDER_ID+(TupleId)no_d_id*ORDER_ID*WHSE_ID);
        }

        newOrderRemoved=false;

        if(no_o_id != -1)
        {
            //delivDeleteNewOrder
            result=Data_Delete(NewOrder_ID, no_id, node_id1);
            if(result==0)
            {
                printf("delivDeleteNewOrder() delete failed,no_id=%ld\n",no_id);
                exit(-1);
            }
        }

        if(no_o_id != -1)
        {
            //delivGetCustId
            oo_id=(TupleId)(no_o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID);
            oo_value=Data_Read(Order_ID, oo_id, node_id1, &flag);
            if(flag==0)
            {
                printf("delivGetCustId() not found, oo_id=%ld\n",oo_id);
                exit(-1);
            }
            else if(flag==-3)
            {
                AbortTransaction();
                return -1;
            }

            o_c_id=(int)(oo_value%CUST_ID);
            o_ol_cnt=(int)((oo_value/CUST_ID)%ORDER_LINES);

            c_id=o_c_id;
            //delivUpdateCarrierId
            oo_value=(TupleId)(o_c_id+o_ol_cnt*CUST_ID+(TupleId)o_carrier_id*CUST_ID*ORDER_LINES);
            result=Data_Update(Order_ID, oo_id, oo_value, node_id1);
            if(result==0)
            {
                printf("delivUpdateCarrierId() update failed, oo_id=%ld\n",oo_id);
                exit(-1);
            }

            //delivUpdateDeliveryDate

            /*
             * skip this operation.
             */

            //delivSumOrderAmount
            ol_o_id=no_o_id;
            ol_w_id=w_id;
            ol_d_id=d_id;
            part_ol_id=(TupleId)(ol_o_id+(TupleId)ol_w_id*ORDER_ID+(TupleId)ol_d_id*ORDER_ID*WHSE_ID);
            ol_total=0;
            for(ol_number=1;ol_number<=o_ol_cnt;ol_number++)
            {
                ol_id=(TupleId)(part_ol_id+(TupleId)ol_number*ORDER_ID*WHSE_ID*DIST_ID);
                ol_value=Data_Read(OrderLine_ID, ol_id, node_id2, &flag);
                if(flag==0)
                {
                    printf("delivSumOrderAmount() not found, ol_id=%ld\n",ol_id);
                    exit(-1);
                }
                else if(flag==-3)
                {
                    AbortTransaction();
                    return -1;
                }

                ol_amount=(int)(ol_value/(ITEM_ID*WHSE_ID*ITEM_QUANTITY));

                ol_total+=ol_amount;
            }

            //delivUpdateCustBalDelivCnt
            cust_id=(TupleId)(c_id+(TupleId)w_id*CUST_ID+(TupleId)d_id*CUST_ID*WHSE_ID);
            cust_value=Data_Read(Customer_ID, cust_id, node_id2, &flag);
            if(flag==0)
            {
                printf("delivUpdateCustBalDelivCnt() not found , cust_id=%ld\n",cust_id);
                exit(-1);
            }
            else if(flag==-3)
            {
                AbortTransaction();
                return -1;
            }

            cust_value+=(TupleId)(ol_total*CUST_CREDIT*CUST_DISCOUNT);
            result=Data_Update(Customer_ID, cust_id, cust_value, node_id2);
            if(result==0)
            {
                printf("delivUpdateCustBalDelivCnt() update failed, cust_id=%ld\n",cust_id);
                exit(-1);
            }
        }
        else
            skippedDeliveries++;
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        newOrderRemoved=true;
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }
    return 0;
}

/*
 *
 */
int orderStatusTransaction(int w_id, int d_id, int c_id, int* node_id, int node_num)
{
    StartTransaction();

    TupleId cust_id, cust_value;
    TupleId oo_id, oo_value;
    TupleId ol_id, ol_value;


    int c_balance;
    int o_id, o_carrier_id, o_ol_cnt;
    int ol_o_id, ol_w_id, ol_d_id, ol_number, ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d;
    int result, index;
    int transaction_result;
    int flag;

    int node_id1, node_id2, node_id3;

    switch(node_num)
    {
    case 1:
        node_id1=node_id[0];
        node_id2=node_id[0];
        node_id3=node_id[0];
        break;
    case 2:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[1];
        break;
    case 3:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[2];
        break;
    default:
        printf("node num error. %d\n", node_num);
    }

    //ordStatGetCustBal
    cust_id=(TupleId)(c_id+w_id*CUST_ID+(TupleId)d_id*CUST_ID*WHSE_ID);
    cust_value=Data_Read(Customer_ID, cust_id, node_id1, &flag);
    if(flag==0)
    {
        printf("ordStatGetCustBal() not found, cust_id=%ld\n",cust_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    c_balance=(int)(cust_value/(CUST_CREDIT*CUST_DISCOUNT));

    // find the newest order for the customer
    oo_id=(TupleId)((TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID);
    oo_value=(TupleId)(c_id);

    //oo_value=Data_Read(Order_ID, oo_id, 0, oo_value);
    o_id=GetMaxOid(Order_ID, w_id, d_id, c_id);


    // retrieve the carrier & order date for the most recent order.
    // o_id=(int)(oo_value);
    if(o_id > 0)
    {
        oo_id=(TupleId)(o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID);
        oo_value=Data_Read(Order_ID, oo_id, node_id1, &flag);
        if(flag==0)
        {
            printf("orderStatus_get_order() not found , oo_id=%ld\n",oo_id);
            exit(-1);
        }
        else if(flag==-3)
        {
            AbortTransaction();
            return -1;
        }

        if(oo_value > 0)
        {
            o_carrier_id=(int)(oo_value/(CUST_ID*ORDER_LINES));
            o_ol_cnt=(int)((oo_value/CUST_ID)%ORDER_LINES);
        }

        // retrieve the order lines for the most recent order
        ol_o_id=o_id;
        ol_w_id=w_id;
        ol_d_id=d_id;
        for(ol_number=1;ol_number<=o_ol_cnt;ol_number++)
        {
            ol_id=(TupleId)(ol_o_id+(TupleId)ol_w_id*ORDER_ID+(TupleId)ol_d_id*ORDER_ID*WHSE_ID+(TupleId)ol_number*ORDER_ID*WHSE_ID*DIST_ID);
            ol_value=Data_Read(OrderLine_ID, ol_id, node_id2, &flag);
            if(flag==0)
            {
                printf("orderstatus_orderline() not found, ol_id=%ld\n",ol_id);
                exit(-1);
            }
            else if(flag==-3)
            {
                AbortTransaction();
                return -1;
            }

            //ol_i_id+ol_supply_w_id*ITEM_ID+ol_quantity*ITEM_ID*WHSE_ID+ol_amount*ITEM_ID*WHSE_ID*ITEM_QUANTITY
            ol_i_id=(int)(ol_value%ITEM_ID);
            ol_supply_w_id=(int)((ol_value/ITEM_ID)%WHSE_ID);
            ol_quantity=(int)((ol_value/(ITEM_ID*WHSE_ID))%ITEM_QUANTITY);
            ol_amount=(int)(ol_value/(ITEM_ID*WHSE_ID*ITEM_QUANTITY));
        }
    }
    else
    {
        ;
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }
    return 0;
}

/*
 *
 */
int stockLevelTransaction(int w_id, int d_id, int threshold, int* node_id, int node_num)
{
    StartTransaction();

    TupleId dist_id, dist_value;
    TupleId ol_id, ol_value;
    TupleId stock_id, stock_value;

    TupleId oo_id, oo_value;

    int o_id = 0;
    int stock_count = 0;
    int ol_o_id, ol_number, ol_cnt, ol_i_id;
    int s_quantity;
    int result, index;
    int transaction_result;
    int flag;

    int node_id1, node_id2, node_id3;

    switch(node_num)
    {
    case 1:
        node_id1=node_id[0];
        node_id2=node_id[0];
        node_id3=node_id[0];
        break;
    case 2:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[1];
        break;
    case 3:
        node_id1=node_id[0];
        node_id2=node_id[1];
        node_id3=node_id[2];
        break;
    default:
        printf("node num error. %d\n", node_num);
    }

    //stockGetDistOrderId
    dist_id=(TupleId)(w_id+d_id*WHSE_ID);
    dist_value=Data_Read(District_ID, dist_id, node_id1, &flag);
    if(flag==0)
    {
        printf("stockGetDistOrderId() not found, dist_id=%ld\n",dist_id);
        exit(-1);
    }
    else if(flag==-3)
    {
        AbortTransaction();
        return -1;
    }

    o_id=(int)(dist_value);

    //stockGetCountStock
    //ol_o_id = (o_id-20 > 0) ? o_id-20 : 1;
    ol_o_id = (o_id-20 > 0) ? o_id-20 : 1;

    for(;ol_o_id < o_id;ol_o_id++)//for each order
    {
        //get the order_line number.
        oo_id=(TupleId)(ol_o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID);
        oo_value=Data_Read(Order_ID, oo_id, node_id1, &flag);
        if(flag==0)
        {
            printf("stocklevel_eachorder() not found, oo_id=%ld\n",oo_id);
            exit(-1);
        }
        else if(flag==-3)
        {
            AbortTransaction();
            return -1;
        }

        ol_cnt=(int)((oo_value/CUST_ID)%ORDER_LINES);

        //for each item in the order.
        for(ol_number=1;ol_number<=ol_cnt;ol_number++)
        {
            //get item_id.
            ol_id=(TupleId)(ol_o_id+(TupleId)w_id*ORDER_ID+(TupleId)d_id*ORDER_ID*WHSE_ID+(TupleId)ol_number*ORDER_ID*WHSE_ID*DIST_ID);
            ol_value=Data_Read(OrderLine_ID, ol_id, node_id1, &flag);
            if(flag==0)
            {
                printf("stocklevel_eachitem() not found, ol_id=%ld\n",ol_id);
                exit(-1);
            }
            else if(flag==-3)
            {
                AbortTransaction();
                return -1;
            }

            ol_i_id=(int)(ol_value%ITEM_ID);

            //get the stock_count.
            stock_id=(TupleId)(ol_i_id+(TupleId)w_id*ITEM_ID);
            stock_value=Data_Read(Stock_ID, stock_id, node_id2, &flag);
            if(flag==0)
            {
                printf("stocklevel_stockcount() not found, stock_id=%ld\n",stock_id);
                exit(-1);
            }
            else if(flag==-3)
            {
                AbortTransaction();
                return -1;
            }

            s_quantity=(int)(stock_value);

            if(s_quantity < threshold)stock_count++;
        }
    }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return 0;
}

int GetMaxOid(int table_id, int w_id, int d_id, int c_id)
{
    TupleId tuple_id, tuple_value;
    int bucket_id, min, max, bucket_size;
    int i, temp_cid, temp_oid;
    int o_id=0;
    bucket_id=(w_id-1)*10+(d_id-1);
    bucket_size=BucketSize[table_id];

    min=bucket_id*bucket_size;
    max=min+bucket_size;

    for(i=min;i<max;i++)
    {

        if(tuple_value>0)
        {
            temp_cid=(int)(tuple_value%CUST_ID);
            if(temp_cid == c_id)
            {
                temp_oid=(int)(tuple_id%ORDER_ID);
                o_id=(temp_oid > o_id) ? temp_oid : o_id;
            }
        }
    }
    return o_id;
}

int GetMinOid(int table_id, int w_id, int d_id)
{
    TupleId tuple_id, tuple_value;
    int bucket_id, bucket_size;
    int i, temp_oid;
    uint64_t min, max;
    int o_id=20000000;
    bucket_id=(w_id-1)*10+(d_id-1);
    bucket_size=BucketSize[table_id];

    min=bucket_id*bucket_size;
    max=min+bucket_size;


    for(i=min;i<max;i++)
    {
        tuple_id=0;

        if(tuple_id > 0)
        {
            temp_oid=(int)(tuple_id%ORDER_ID);

            o_id=(temp_oid < o_id) ? temp_oid : o_id;
        }
    }
    if(o_id==20000000)
    {
        validation(table_id);
    }
    return (o_id < 20000000) ? o_id : -1;
}

int ReadWhse(int numWarehouses)
{
    int i;
    uint64_t k;
    int table_id;
    TupleId tuple_id, value;
    int result, index;

    int w_id;

    int flag;

    int node_id;

    node_id=nodeid;

    k=0;
    table_id=Warehouse_ID;
    StartTransaction();
    for(i=1;i<=numWarehouses;i++)
    {
        w_id=i;
        tuple_id=(TupleId)w_id;
        value=0;
        //printf("before insert.\n");
        result=Data_Read(table_id, tuple_id, node_id, &flag);
        if(flag==0)
        {
            printf("ReadWhse failed for %d : %ld.\n",table_id, tuple_id);
            exit(-1);
        }
        k++;
        if(k%configCommitCount==0)
        {
            result=PreCommit();
            if(result == 1)
            {
                CommitTransaction();
            }
            else
            {
                AbortTransaction();
            }
            StartTransaction();
        }
    }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }
    return k;
}

int ReadItem(int ItemCount)
{
    int i;
    uint64_t k;
    int table_id;
    TupleId tuple_id, value;
    int result, index;

    int i_id, price;

    int flag;

    int node_id;

    node_id=nodeid;

    table_id=Item_ID;
    k=0;

    StartTransaction();
    for(i=1;i<=ItemCount;i++)
    {
        i_id=i;
        tuple_id=(TupleId)i_id;
        // random price.
        price=(int)RandomNumber(1, 100);
        value=(TupleId)price;
        result=Data_Read(table_id, tuple_id, node_id, &flag);

        if(flag==0)
        {
            printf("ReadItem failed for %d : %ld.\n",table_id, tuple_id);
            exit(-1);
        }
        k++;
        if(k%configCommitCount==0)
        {
            result=PreCommit();
            if(result == 1)
            {
                CommitTransaction();
            }
            else
            {
                AbortTransaction();
            }
            StartTransaction();
        }
    }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

int ReadStock(int numWarehouses, int ItemCount)
{
    int i, w;
    uint64_t k;
    int table_id;
    TupleId tuple_id, value;
    int result, index;

    int s_i_id, s_w_id, quantity;

    int flag;

    int node_id;

    node_id=nodeid;

    k=0;
    table_id=Stock_ID;
    StartTransaction();
    for(i=1;i<=ItemCount;i++)
        for(w=1;w<=numWarehouses;w++)
        {
            s_i_id=i;
            s_w_id=w;

            tuple_id=(TupleId)(s_i_id+(TupleId)s_w_id*ITEM_ID);
            quantity=RandomNumber(10, 100);

            value=(TupleId)quantity;
            result=Data_Read(table_id, tuple_id, node_id, &flag);
            if(flag == 0)
            {
                printf("ReadStock failed for %d : %ld.\n",table_id, tuple_id);
                exit(-1);
            }
            k++;
            if(k%configCommitCount==0)
            {
                result=PreCommit();
                if(result == 1)
                {
                    CommitTransaction();
                }
                else
                {
                    AbortTransaction();
                }
                StartTransaction();
            }
        }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

int ReadDist(int numWarehouses, int DistPerWhse)
{
    int d_id, d_w_id, d_next_o_id;
    int d, w, result, index;
    int table_id;
    uint64_t k;
    TupleId tuple_id, value;

    int flag;

    int node_id;

    node_id=nodeid;

    table_id=District_ID;
    k=0;

    StartTransaction();
    for(w=1;w<=numWarehouses;w++)
        for(d=1;d<=DistPerWhse;d++)
        {
            d_w_id=w;
            d_id=d;
            //d_next_o_id=3001;
            d_next_o_id=configCustPerDist+1;

            tuple_id=(TupleId)(d_w_id+(TupleId)d_id*WHSE_ID);
            value=(TupleId)d_next_o_id;
            result=Data_Read(table_id, tuple_id, node_id, &flag);
            if(flag==0)
            {
                printf("ReadDist failed for %d : %ld.\n",table_id, tuple_id);
                exit(-1);
            }
            k++;

            if(k%configCommitCount==0)
            {
                result=PreCommit();
                if(result == 1)
                {
                    CommitTransaction();
                }
                else
                {
                    AbortTransaction();
                }
                StartTransaction();
            }
        }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

int ReadCust(int numWarehouses, int DistPerWhse, int CustPerDist)
{
    int w,d,c;
    int c_id, c_w_id, c_d_id, c_discount, c_credit, c_balance, c_payment_cnt, c_delivery_cnt;
    int table_id, result, index;
    uint64_t k;
    TupleId cust_id, cust_value;
    TupleId hist_id, hist_value;

    int h_c_id, h_c_w_id, h_c_d_id, h_w_id, h_d_id, h_amount;

    int flag;

    int node_id;

    node_id=nodeid;

    table_id=Customer_ID;
    k=0;

    StartTransaction();
    for(w=1;w<=numWarehouses;w++)
        for(d=1;d<=DistPerWhse;d++)
            for(c=1;c<=CustPerDist;c++)
            {
                c_id=c;
                c_d_id=d;
                c_w_id=w;

                cust_id=(TupleId)(c_id+c_w_id*CUST_ID+(TupleId)c_d_id*CUST_ID*WHSE_ID);
                //to compute.
                //discount from 1 to 5.
                c_discount=RandomNumber(1,5);

                if(RandomNumber(1,100) <= 90)//90% good credit.
                {
                    c_credit=1;
                }
                else//10% bad credit.
                {
                    c_credit=0;
                }

                c_balance=0;

                c_payment_cnt=1;
                c_delivery_cnt=0;

                h_c_id=c;
                h_c_w_id=w;
                h_c_d_id=d;
                h_w_id=w;
                h_d_id=d;
                h_amount=10;

                cust_value=(TupleId)(c_credit+(TupleId)c_discount*CUST_CREDIT+(TupleId)c_balance*CUST_CREDIT*CUST_DISCOUNT);

                result=Data_Read(Customer_ID, cust_id, node_id, &flag);
                if(flag==0)
                {
                    printf("ReadCust failed for %d : %ld.\n",Customer_ID, cust_id);
                    validation(Customer_ID);
                    exit(-1);
                }

                hist_id=(TupleId)(h_c_id+h_c_w_id*CUST_ID+(TupleId)h_c_d_id*CUST_ID*WHSE_ID);
                hist_value=(TupleId)(h_w_id+h_d_id*WHSE_ID+(TupleId)h_amount*WHSE_ID*DIST_ID);

                result=Data_Read(History_ID, hist_id, node_id, &flag);
                if(flag==0)
                {
                    printf("ReadHist failed for %d : %ld.\n",History_ID, hist_id);
                    exit(-1);
                }
                k+=2;
                if(k%configCommitCount==0)
                {
                    result=PreCommit();
                    if(result == 1)
                    {
                        CommitTransaction();
                    }
                    else
                    {
                        AbortTransaction();
                    }
                    StartTransaction();
                }
            }
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        AbortTransaction();
    }

    return k;
}

int ReadOrder(int numWarehouses, int DistPerWhse, int CustPerDist)
{
    return 0;
}

int testTransaction(int w_id, int d_id)
{

    TupleId item_id, item_value;

    int flag, result, index;
    int transaction_result;
    int min, max;

    int i;


    //item_id=(TupleId)getItemID();
    item_id=(TupleId)d_id;

    TupleId tuple_id, tuple_value;;
    TransactionData* tdata;

    THREAD* threadinfo;

    int node_id;

    node_id=nodeid;

    threadinfo=(THREAD*)pthread_getspecific(ThreadInfoKey);
    tdata=(TransactionData*)pthread_getspecific(TransactionDataKey);

    index=threadinfo->index;

    min=index*3000;
    max=min+3000;

    StartTransaction();

    for(i=0;i<10;i++)
    {
        //tuple_id=(TupleId)(w_id+(TupleId)d_id*WHSE_ID);
        tuple_value=0;

        //tuple_id=(TupleId)getItemID();
        tuple_id=(TupleId)(getItemID()%(max-min)+min);
        result=Data_Update(Item_ID, tuple_id, tuple_value, node_id);
        if(result==0)
        {
            printf("delivUpdateCustBalDelivCnt() update failed, cust_id=%ld\n",tuple_id);
            exit(-1);
        }
        else if(result < 0)
        {
            AbortTransaction();
            return -1;
        }
    }

    result=PreCommit();
    if(result==1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }
    return transaction_result;
}


//smallbank
int LoadBankData(void)
{
    bool success;
    int result;
    int i, node_id;
    uint64_t k;
    TupleId acctName, acctId, balance;
    
    k=0;
    node_id=nodeid;
    StartTransaction();

    for(i=1;i<=configNumAccounts;i++)
    {
        acctName=i;
        acctId=i;

        //ACCOUNT
        result=Data_Insert(Accounts_ID, acctId, acctName, node_id);
        if(result==0)
        {
            printf("LoadAccountData failed for %d.\n",i);
            exit(-1);
        }

        //CHECKINGS
        balance=RandomNumber(MIN_BALANCE, MAX_BALANCE);
        result=Data_Insert(Checking_ID, acctId, balance, node_id);
        if(result==0)
        {
            printf("LoadCheckingsData failed for %d.\n",i);
            exit(-1);
        }

        //SAVINGS
        balance=RandomNumber(MIN_BALANCE, MAX_BALANCE);
        result=Data_Insert(Savings_ID, acctId, balance, node_id);
        if(result==0)
        {
            printf("LoadSavingsData failed for %d.\n",i);
            exit(-1);
        }

        k+=3;

        //configCommitCount must be muti-times of 3.
        if(k%configCommitCount==0)
        {
            result=PreCommit();
            if(result == 1)
            {
                CommitTransaction();
            }
            else
            {
                // current transaction has to rollback.
                AbortTransaction();
            }
            StartTransaction();
        }
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
    }
    else
    {
        //current transaction has to rollback.
        AbortTransaction();
    }

    printf("loaded total rows are %ld \n",k);

    return k;
}

void executeTransactionsBank(int numTransactions, TransState* StateInfo)
{
    int transactionWeight;
    int i;

    int is_global;
    int abort;

    // prevent run the thread too long time
    TimeStampTz help_start;
    
    //random read control
    TupleId* acctArr=NULL;
    int* nodeArr=NULL;

    if(random_read_limit > 0)
    {
        acctArr=(TupleId*)malloc(sizeof(TupleId)*random_read_limit*2);
        nodeArr=(int*)malloc(sizeof(int)*random_read_limit*2);

        if(acctArr==NULL || nodeArr==NULL)
        {
            printf("random read malloc error\n");
            exit(-1);
        }
    }

    StateInfo->trans_commit=0;
    StateInfo->trans_abort=0;
    StateInfo->Delivery=0;
    StateInfo->NewOrder=0;
    StateInfo->Payment=0;
    StateInfo->Order_status=0;
    StateInfo->Stock_level=0;

    StateInfo->global_total=0;
    StateInfo->global_abort=0;

    //smallbank
    StateInfo->Amalgamate=0;
    StateInfo->Balance=0;
    StateInfo->DepositChecking=0;
    StateInfo->WriteCheck=0;
    StateInfo->SendPayment=0;
    StateInfo->TransactSavings=0;

    StateInfo->Amalgamate_C=0;
    StateInfo->Balance_C=0;
    StateInfo->DepositChecking_C=0;
    StateInfo->WriteCheck_C=0;
    StateInfo->SendPayment_C=0;
    StateInfo->TransactSavings_C=0;

    help_start = GetCurrentTimestamp();
    for(i=0;i<numTransactions;i++)
    {
        if (i % 100 == 0)
        {
            if (GetCurrentTimestamp()-help_start > 120*1000000)
            break;
        }
        transactionWeight=(int)RandomNumber(1, 100);

        if(transactionWeight <= FREQUENCY_AMALGAMATE)
        {
            executeTransactionBank(AMALGAMATE, &abort, &is_global, acctArr, nodeArr);
            StateInfo->Amalgamate++;

            if(abort != REDOLIMIT)
                StateInfo->Amalgamate_C++;
        }
        else if(transactionWeight <= FREQUENCY_AMALGAMATE + FREQUENCY_BALANCE)
        {
            executeTransactionBank(BALANCE, &abort, &is_global, acctArr, nodeArr);
            StateInfo->Balance++;

            if(abort != REDOLIMIT)
                StateInfo->Balance_C++;
        }
        else if(transactionWeight <= FREQUENCY_AMALGAMATE + FREQUENCY_BALANCE + FREQUENCY_DEPOSIT_CHECKING)
        {
            executeTransactionBank(DEPOSITCHECKING, &abort, &is_global, acctArr, nodeArr);
            StateInfo->DepositChecking++;

            if(abort != REDOLIMIT)
                StateInfo->DepositChecking_C++;
        }
        else if(transactionWeight <= FREQUENCY_AMALGAMATE + FREQUENCY_BALANCE + FREQUENCY_DEPOSIT_CHECKING + FREQUENCY_SEND_PAYMENT)
        {
            executeTransactionBank(SENDPAYMENT, &abort, &is_global, acctArr, nodeArr);
            StateInfo->SendPayment++;

            if(abort != REDOLIMIT)
                StateInfo->SendPayment_C++;            
        }
        else if(transactionWeight <= FREQUENCY_AMALGAMATE + FREQUENCY_BALANCE + FREQUENCY_DEPOSIT_CHECKING + FREQUENCY_SEND_PAYMENT + FREQUENCY_TRANSACT_SAVINGS)
        {
            executeTransactionBank(TRANSACTSAVINGS, &abort, &is_global, acctArr, nodeArr);
            StateInfo->TransactSavings++;

            if(abort != REDOLIMIT)
                StateInfo->TransactSavings_C++;

        }
        else
        {
            executeTransactionBank(WRITECHECK, &abort, &is_global, acctArr, nodeArr);
            StateInfo->WriteCheck++;

            if(abort != REDOLIMIT)
                StateInfo->WriteCheck_C++;            
        }

        // commit
        if (abort < REDOLIMIT)
        {
            StateInfo->trans_commit++;
            StateInfo->trans_abort += abort;
            if (is_global)
            {
                StateInfo->global_abort += abort;
                StateInfo->global_total += (abort+1);
            }
        }
        else
        {
            StateInfo->trans_abort += REDOLIMIT;
            if (is_global)
            {
               StateInfo->global_abort += REDOLIMIT;
               StateInfo->global_total += REDOLIMIT;
            }
        }
    }
}

/*
 * @return:'0' for commit, '-1' for abort.
 */
void executeTransactionBank(TransactionsType type, int *pabort, int *pglobal, TupleId* acctArr, int* nodeArr)
{
    int redo;
    bool distributed=true;
    int i;
    int result;
    
    int acctId0, acctId1, amount;

    int abort = 0;
    int node_id[5], node_num, nid;
    int node_used[NODENUMMAX]={0};

    int LocalWeight;

    LocalWeight=(int)RandomNumber(1,100);

    if(LocalWeight <= oneNodeWeight)
    {
        distributed=false;
        node_num=1;
    }
    else if(LocalWeight <= oneNodeWeight+twoNodeWeight)
        node_num=2;
    else
        node_num=3;

    if(node_num > nodenum)
        node_num=nodenum;

    node_id[0]=nodeid;
    node_used[nodeid]=1;

    for(i=1;i<node_num;i++)
    {
        node_id[i]=(nodeid+i)%nodenum;
    }

    int customerDistrictID, customerWarehouseID;
    int v;
    int paymentAmount;

    int threshold;

    int orderCarrierID;

    //hotspot control
    bool isHotSpot=false;
    int accountsRange;
    isHotSpot=(RandomNumber(1,100)<=HOTSPOT_PERCENTAGE)?true:false;
    accountsRange=(isHotSpot==true)?(HOTSPOT_FIXED_SIZE):(configNumAccounts-HOTSPOT_FIXED_SIZE);

    switch(type)
    {
    case AMALGAMATE:
        //acctId0=RandomNumber(1, configNumAccounts);
        acctId0=RandomNumber(1, accountsRange);

        while(1)
        {
            //acctId1=RandomNumber(1, configNumAccounts);
            acctId1=RandomNumber(1, accountsRange);

            if(acctId1 != acctId0)
                break;
        }

        if(isHotSpot==false)
        {
            acctId0+=HOTSPOT_FIXED_SIZE;
            acctId1+=HOTSPOT_FIXED_SIZE;
        }

        //random read
        randomReadGen(acctArr, nodeArr, node_id, node_num);

        for (redo = 0; redo < REDOLIMIT; redo++)
        {
             result=AmalgamateTransaction(acctId0, acctId1, node_id, node_num, acctArr, nodeArr);
             if(result==0)
                break;
             else
             {
                abort++;
             }
        }        
        break;
    case BALANCE:
        //acctId0=RandomNumber(1, configNumAccounts);
        acctId0=RandomNumber(1, accountsRange);
        if(isHotSpot==false)
            acctId0+=HOTSPOT_FIXED_SIZE;

        //random read
        randomReadGen(acctArr, nodeArr, node_id, node_num);

        for (redo = 0; redo < REDOLIMIT; redo++)
        {
             result=BalanceTransaction(acctId0, node_id, node_num, acctArr, nodeArr);
             if(result==0)
                break;
             else
             {
                abort++;
             }
        }        
        break;
    case DEPOSITCHECKING:
        //acctId0=RandomNumber(1, configNumAccounts);
        acctId0=RandomNumber(1, accountsRange);
        if(isHotSpot==false)
            acctId0+=HOTSPOT_FIXED_SIZE;
        amount=RandomNumber(1, 10000);

        //random read
        randomReadGen(acctArr, nodeArr, node_id, node_num);

        for (redo = 0; redo < REDOLIMIT; redo++)
        {
             result=DepositCheckingTransaction(acctId0, amount, node_id, node_num, acctArr, nodeArr);
             if(result==0)
                break;
             else
             {
                abort++;
             }
        }            
        break;
    case SENDPAYMENT:
        //acctId0=RandomNumber(1, configNumAccounts);
        acctId0=RandomNumber(1, accountsRange);

        while(1)
        {
            //acctId1=RandomNumber(1, configNumAccounts);
            acctId1=RandomNumber(1, accountsRange);

            if(acctId1 != acctId0)
                break;
        }

        if(isHotSpot==false)
        {
            acctId0+=HOTSPOT_FIXED_SIZE;
            acctId1+=HOTSPOT_FIXED_SIZE;
        }

        amount=RandomNumber(1, 10000);

        //random read
        randomReadGen(acctArr, nodeArr, node_id, node_num);

        for (redo = 0; redo < REDOLIMIT; redo++)
        {
             result=SendPaymentTransaction(acctId0, acctId1, amount, node_id, node_num, acctArr, nodeArr);
             if(result==0)
                break;
             else
             {
                abort++;
             }
        }
        break;
    case TRANSACTSAVINGS:
        //acctId0=RandomNumber(1, configNumAccounts);
        acctId0=RandomNumber(1, accountsRange);
        if(isHotSpot==false)
            acctId0+=HOTSPOT_FIXED_SIZE;
        amount=RandomNumber(1, 10000);
        
        //random read
        randomReadGen(acctArr, nodeArr, node_id, node_num);

        for (redo = 0; redo < REDOLIMIT; redo++)
        {
             result=TransactSavingsTransaction(acctId0, amount, node_id, node_num, acctArr, nodeArr);
             if(result==0)
                break;
             else
             {
                abort++;
             }
        }    

        break;
    case WRITECHECK:
        //acctId0=RandomNumber(1, configNumAccounts);
        acctId0=RandomNumber(1, accountsRange);
        if(isHotSpot==false)
            acctId0+=HOTSPOT_FIXED_SIZE;
        amount=RandomNumber(1, 10000);

        //random read
        randomReadGen(acctArr, nodeArr, node_id, node_num);

        for (redo = 0; redo < REDOLIMIT; redo++)
        {
             result=WriteCheckTransaction(acctId0, amount, node_id, node_num, acctArr, nodeArr);
             if(result==0)
                break;
             else
             {
                abort++;
             }
        }        
        break;
    default:
        //result=testTransaction(terminalWarehouseID, districtID);
        printf("transaction type error\n");
    }

    if(distributed)
        *pglobal = true;
    else
        *pglobal = false;

    *pabort = abort;
}


int SendPaymentTransaction(TupleId sendAcct, TupleId destAcct, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    StartTransaction();

    bool success;
    TupleId sendBal, destBal, acctName;
    int table_id, flag;
    TupleId acctId;
    
    int nodeid1, nodeid2;
    int result;
    int transaction_result;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        nodeid2=node_id[0];
        break;
    case 2:
        nodeid1=node_id[0];
        nodeid2=node_id[1];
        break;
    case 3:
        nodeid1=node_id[0];
        nodeid2=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
        break;
    }

    //get account information.

    //GetAccount
    acctId=sendAcct;
    
    acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("sendGetAccount() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    //GetAccount
    acctId=destAcct;
    acctName=Data_Read(Accounts_ID, acctId, nodeid2, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("destGetAccount() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    

    //get the sender's account balance.

    //GetCheckingBalance
    acctId=sendAcct;
    sendBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("sendGetCheckingBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }


    //sender has no so much money.
    if(sendBal < amount)
    {
        AbortTransaction();
        return -1;
    }

    // Debt
    //UpdateCheckingBalance
    acctId=sendAcct;
    sendBal=sendBal-amount;

    result=Data_Update(Checking_ID, acctId, sendBal, nodeid1);
    if(result == 0)
    {
        printf("UpdateCheckingBalance() update failed, acctId=%ld\n",acctId);
        exit(-1);
    }
    

    //Credit
    //UpdateCheckingBalance
    acctId=destAcct;
    destBal=Data_Read(Checking_ID, acctId, nodeid2, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("UpdateCheckingBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }
    
    destBal+=amount;
    
    result=Data_Update(Checking_ID, acctId, destBal, nodeid2);
    if(result == 0)
    {
        printf("UpdateCheckingBalance() update failed, acctId=%ld\n",acctId);
        exit(-1);
    }

    //read extension
    if(SendPaymentTransactionExtension(sendAcct, destAcct, amount, node_id, node_num, acctArr, nodeArr)<0)
    {
        AbortTransaction();
        return -1;
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;

}

int TransactSavingsTransaction(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    StartTransaction();

    bool success;
    int flag, result, transaction_result;
    int nodeid1;
    TupleId acctName, balance, checkingBal, savingBal;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    //GetAccount
    acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetAccount() not found, acctId=%ld\n",acctId);
        exit(-1);
    }


    //GetSavingsBalance
    savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    //balance=savingBal+amount;
    balance=savingBal-amount;

    if(balance < 0)
    {
        AbortTransaction();
        return -1;
    }

    //UpdateSavingsBalance
    savingBal=savingBal-amount;
    result=Data_Update(Savings_ID, acctId, savingBal, nodeid1);
    if(result == 0)
    {
        printf("UpdateSavingsBalance() update failed, acctId=%ld\n",acctId);
        exit(-1);
    }

    //read extension
    if(TransactSavingsTransactionExtension(acctId, amount, node_id, node_num, acctArr, nodeArr)<0)
    {
        AbortTransaction();
        return -1;
    }
    
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;

}

int WriteCheckTransaction(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    StartTransaction();

    bool success;
    int nodeid1;
    int flag, result, transaction_result;
    TupleId acctName, balance, savingBal, checkingBal, totalBal;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    //GetAccount
    acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetAccount() not found, acctId=%ld\n",acctId);
        exit(-1);
    }


    //GetSavingsBalance
    savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }


    //GetCheckingBalance
    checkingBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetCheckingBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    totalBal=savingBal+checkingBal;

    if(totalBal < amount)
    {
        //UpdateCheckingBalance
        checkingBal=(checkingBal-(amount-1)<0)?0:(checkingBal-(amount-1));
        result=Data_Update(Checking_ID, acctId, checkingBal, nodeid1);
        if(result == 0)
        {
            printf("UpdateCheckingBalance() update failed, acctId=%ld\n",acctId);
            exit(-1);
        }
    }
    else
    {
        //UpdateCheckingBalance
        checkingBal=(checkingBal-amount<0)?0:(checkingBal-amount);
        result=Data_Update(Checking_ID, acctId, checkingBal, nodeid1);
        if(result == 0)
        {
            printf("UpdateCheckingBalance() update failed, acctId=%ld\n",acctId);
            exit(-1);
        }
    }

    //read extension
    if(WriteCheckTransactionExtension(acctId, amount, node_id, node_num, acctArr, nodeArr)<0)
    {
        AbortTransaction();
        return -1;
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;

}

int AmalgamateTransaction(TupleId acctId0, TupleId acctId1, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    StartTransaction();

    bool success;
    int flag, result, transaction_result;
    TupleId acctName, balance, checkingBal0, savingBal0, checkingBal1, savingBal1, acctId, totalBal;
    int nodeid1, nodeid2;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        nodeid2=node_id[0];
        break;
    case 2:
        nodeid1=node_id[0];
        nodeid2=node_id[1];
        break;
    case 3:
        nodeid1=node_id[0];
        nodeid2=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
        break;
    }

    //get account information.

    //GetAccount
    acctName=Data_Read(Accounts_ID, acctId0, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetAccount() not found, acctId0=%ld\n",acctId0);
        exit(-1);
    }


    //GetAccount
    acctName=Data_Read(Accounts_ID, acctId1, nodeid2, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetAccount() not found, acctId1=%ld\n",acctId1);
        exit(-1);
    }


    // Get Balance Information

    //GetSavingsBalance
    savingBal0=Data_Read(Savings_ID, acctId0, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetSavingsBalance() not found, acctId0=%ld\n",acctId1);
        exit(-1);
    }


    //GetCheckingBalance
    checkingBal1=Data_Read(Checking_ID, acctId1, nodeid2, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetCheckingBalance() not found, acctId1=%ld\n",acctId1);
        exit(-1);
    }


    totalBal=savingBal0+checkingBal1;

    // Update Balance Information

    //ZeroCheckingBalance
    checkingBal0=0;
    result=Data_Update(Checking_ID, acctId0, checkingBal0, nodeid1);
    if(result == 0)
    {
        printf("ZeroCheckingBalance() update failed, acctId0=%ld\n",acctId0);
        exit(-1);
    }

    //UpdateSavingsBalance
    savingBal1=Data_Read(Savings_ID, acctId1, nodeid2, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetSavingBalance() not found, acctId1=%ld\n",acctId1);
        exit(-1);
    }

    savingBal1=(savingBal1-totalBal<0)?0:(savingBal1-totalBal);
    result=Data_Update(Savings_ID, acctId1, savingBal1, nodeid2);
    if(result == 0)
    {
        printf("UpdateSavingsBalance() update failed, acctId1=%ld\n",acctId1);
        exit(-1);
    }

    //read extension
    if(AmalgamateTransactionExtension(acctId0, acctId1, node_id, node_num, acctArr, nodeArr)<0)
    {
        AbortTransaction();
        return -1;
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;


}

int BalanceTransaction(TupleId acctId, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    StartTransaction();

    bool success;
    int flag, result, transaction_result;
    TupleId acctName, checkingBal, savingBal, totalBal;
    int nodeid1;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    //GetAccount
    acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetAccount() not found, acctId=%ld\n",acctId);
        exit(-1);
    }


    //GetSavingsBalance
    savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    //GetCheckingBalance
    checkingBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetCheckingBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    //read extension
    if(BalanceTransactionExtension(acctId, node_id, node_num, acctArr, nodeArr)<0)
    {
        AbortTransaction();
        return -1;
    }


    totalBal=savingBal+checkingBal;
    
    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;

}

int DepositCheckingTransaction(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    StartTransaction();

    bool success;
    int flag, result, transaction_result;
    TupleId acctName, savingBal, checkingBal;
    int nodeid1;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    //GetAccount
    acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("GetAccount() not found, acctId=%ld\n",acctId);
        exit(-1);
    }

    //UpdateCheckingBalance
    checkingBal=Data_Read(Checking_ID, acctId, nodeid1,  &flag);
    if(flag == -3)
    {
        AbortTransaction();
        return -1;
    }
    else if(flag==0)
    {
        printf("UpdateCheckingBalance() not found, acctId=%ld\n",acctId);
        exit(-1);
    }


    checkingBal+=amount;
    result=Data_Update(Checking_ID, acctId, checkingBal, nodeid1);
    if(result == 0)
    {
        printf("UpdateCheckingBalance() update failed, acctId=%ld\n",acctId);
        exit(-1);
    }

    //read extension
    if(DepositCheckingTransactionExtension(acctId, amount, node_id, node_num, acctArr, nodeArr)<0)
    {
        AbortTransaction();
        return -1;
    }

    result=PreCommit();
    if(result == 1)
    {
        CommitTransaction();
        transaction_result=0;
    }
    else
    {
        AbortTransaction();
        transaction_result=-1;
    }

    return transaction_result;


}

//smallbank extension
int SendPaymentTransactionExtension(TupleId sendAcct, TupleId destAcct, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    int flag;
    int repeat=0;
    TupleId acctId0, acctId1, acctName, sendBal, destBal;

    int nodeid1, nodeid2;
    int result;
    int transaction_result;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        nodeid2=node_id[0];
        break;
    case 2:
        nodeid1=node_id[0];
        nodeid2=node_id[1];
        break;
    case 3:
        nodeid1=node_id[0];
        nodeid2=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
        break;
    }

    while(repeat++<EXTENSIONLIMIT)
    {
        //get account information.

        //GetAccount
        acctId0=sendAcct;

        acctName=Data_Read(Accounts_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("sendGetAccount() not found, acctId=%ld\n",acctId0);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetAccount
        acctId1=destAcct;
        acctName=Data_Read(Accounts_ID, acctId1, nodeid2, &flag);
        if(flag==0)
        {
            printf("destGetAccount() not found, acctId=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //get the sender's account balance.

        //GetCheckingBalance
        acctId0=sendAcct;
        sendBal=Data_Read(Checking_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("sendGetCheckingBalance() not found, acctId=%ld\n",acctId0);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    int i;
    for(i=0;i<random_read_limit*2;i+=2)
    {
        acctId0=acctArr[i];
        acctId1=acctArr[i+1];
        nodeid1=nodeArr[i];
        nodeid2=nodeArr[i+1];
        //get account information.

        //GetAccount
        //acctId0=sendAcct;

        acctName=Data_Read(Accounts_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("sendGetAccount() not found, acctId=%ld\n",acctId0);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetAccount
        //acctId1=destAcct;
        acctName=Data_Read(Accounts_ID, acctId1, nodeid2, &flag);
        if(flag==0)
        {
            printf("destGetAccount() not found, acctId=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //get the sender's account balance.

        //GetCheckingBalance
        //acctId0=sendAcct;
        sendBal=Data_Read(Checking_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("sendGetCheckingBalance() not found, acctId=%ld\n",acctId0);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    return 0;
}

int TransactSavingsTransactionExtension(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    int flag;
    int repeat=0;
    int nodeid1;
    TupleId acctName, balance, checkingBal, savingBal;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    while(repeat++<EXTENSIONLIMIT)
    {
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetSavingsBalance
        savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    int i;
    for(i=0;i<random_read_limit;i++)
    {
        acctId=acctArr[i];
        nodeid1=nodeArr[i];
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetSavingsBalance
        savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    return 0;
}

int WriteCheckTransactionExtension(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    int repeat=0;
    int nodeid1;
    int flag;
    TupleId acctName, balance, savingBal, checkingBal, totalBal;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    while(repeat++<EXTENSIONLIMIT)
    {
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetSavingsBalance
        savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetCheckingBalance
        checkingBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetCheckingBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    int i;
    for(i=0;i<random_read_limit;i++)
    {
        acctId=acctArr[i];
        nodeid1=nodeArr[i];
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetSavingsBalance
        savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetCheckingBalance
        checkingBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetCheckingBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    return 0;
}

int AmalgamateTransactionExtension(TupleId acctId0, TupleId acctId1, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    int repeat=0;
    int flag;
    TupleId acctName, balance, checkingBal0, savingBal0, checkingBal1, savingBal1, acctId, totalBal;
    int nodeid1, nodeid2;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        nodeid2=node_id[0];
        break;
    case 2:
        nodeid1=node_id[0];
        nodeid2=node_id[1];
        break;
    case 3:
        nodeid1=node_id[0];
        nodeid2=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
        break;
    }

    while(repeat++<EXTENSIONLIMIT)
    {
        //get account information.

        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId0=%ld\n",acctId0);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId1, nodeid2, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId1=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        // Get Balance Information

        //GetSavingsBalance
        savingBal0=Data_Read(Savings_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId0=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetCheckingBalance
        checkingBal1=Data_Read(Checking_ID, acctId1, nodeid2, &flag);
        if(flag==0)
        {
            printf("GetCheckingBalance() not found, acctId1=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    int i;
    for(i=0;i<random_read_limit*2;i+=2)
    {
        acctId0=acctArr[i];
        acctId1=acctArr[i+1];
        nodeid1=nodeArr[i];
        nodeid2=nodeArr[i+1];
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId0=%ld\n",acctId0);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId1, nodeid2, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId1=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        // Get Balance Information

        //GetSavingsBalance
        savingBal0=Data_Read(Savings_ID, acctId0, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId0=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetCheckingBalance
        checkingBal1=Data_Read(Checking_ID, acctId1, nodeid2, &flag);
        if(flag==0)
        {
            printf("GetCheckingBalance() not found, acctId1=%ld\n",acctId1);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    return 0;
}

int BalanceTransactionExtension(TupleId acctId, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    int repeat=0;
    int flag;
    TupleId acctName, checkingBal, savingBal, totalBal;
    int nodeid1;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    while(repeat++<EXTENSIONLIMIT)
    {
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetSavingsBalance
        savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }


        //GetCheckingBalance
        checkingBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetCheckingBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    int i;
    //random read control
    for(i=0;i<random_read_limit;i++)
    {
        acctId=acctArr[i];
        nodeid1=nodeArr[i];
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //GetSavingsBalance
        savingBal=Data_Read(Savings_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetSavingsBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }


        //GetCheckingBalance
        checkingBal=Data_Read(Checking_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetCheckingBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    return 0;
}

int DepositCheckingTransactionExtension(TupleId acctId, int amount, int* node_id, int node_num, TupleId* acctArr, int* nodeArr)
{
    int repeat=0;
    int flag;
    TupleId acctName, savingBal, checkingBal;
    int nodeid1;

    switch(node_num)
    {
    case 1:
        nodeid1=node_id[0];
        break;
    case 2:
        nodeid1=node_id[1];
        break;
    case 3:
        nodeid1=node_id[2];
        break;
    default:
        printf("node_num error %d\n", node_num);
    }

    while(repeat++<EXTENSIONLIMIT)
    {
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //UpdateCheckingBalance
        checkingBal=Data_Read(Checking_ID, acctId, nodeid1,  &flag);
        if(flag==0)
        {
            printf("UpdateCheckingBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    int i;
    //random read control
    for(i=0;i<random_read_limit;i++)
    {
        acctId=acctArr[i];
        nodeid1=nodeArr[i];
        //GetAccount
        acctName=Data_Read(Accounts_ID, acctId, nodeid1, &flag);
        if(flag==0)
        {
            printf("GetAccount() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }

        //UpdateCheckingBalance
        checkingBal=Data_Read(Checking_ID, acctId, nodeid1,  &flag);
        if(flag==0)
        {
            printf("UpdateCheckingBalance() not found, acctId=%ld\n",acctId);
            exit(-1);
        }
        else if(flag==-3)
        {
            //AbortTransactionbak();
            return -1;
        }
    }

    return 0;
}

void randomReadGen(TupleId* acctArr, int* nodeArr, int* node_id, int node_num)
{
    int i;
    for(i=0;i<random_read_limit*2;i++)
    {
        acctArr[i]=RandomNumber(1, configNumAccounts);
        nodeArr[i]=node_id[RandomNumber(1,node_num)-1];
    }
}

