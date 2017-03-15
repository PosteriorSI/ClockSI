#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "master.h"

int time_skew_max;
int nodenum;
int threadnum;
int client_port;
int message_port;
int param_port;
int record_port;
char master_ip[20];

int oneNodeWeight;
int twoNodeWeight;
int redo_limit;

//hotspot control
int HOTSPOT_PERCENTAGE;
int HOTSPOT_FIXED_SIZE;

//duration control
int extension_limit;

//random read control
int random_read_limit;

BENCHMARK benchmark_type;
int transactions_per_terminal;
int payment_weight_value;
int order_status_weight_value;
int delivery_weight_value;
int stock_level_weight_value;
int lim_per_min_terminal;
int config_whse_count;
int config_commit_count;
int order_max_num;
int max_data_lock_num;
int scale_factor;
int FREQUENCY_AMALGAMATE;
int FREQUENCY_BALANCE;
int FREQUENCY_DEPOSIT_CHECKING;
int FREQUENCY_SEND_PAYMENT;
int FREQUENCY_TRANSACT_SAVINGS;
int FREQUENCY_WRITE_CHECK;

int ReadConfig(char * find_string, char * result)
{
   int i;
   int j;
   int k;
   FILE * fp;
   char buffer[50];
   char * p;
   if ((fp = fopen("config.txt", "r")) == NULL)
   {
       printf("can not open the configure file.\n");
       fclose(fp);
       return -1;
   }
   for (i = 0; i < LINEMAX; i++)
   {
      if (fgets(buffer, sizeof(buffer), fp) == NULL)
         continue;
      for (p = find_string, j = 0; *p != '\0'; p++, j++)
      {
         if (*p != buffer[j])
            break;
      }

      if (*p != '\0' || buffer[j] != ':')
      {
         continue;
      }

      else
      {
         k = 0;
         /* jump over the character ':' and the space character. */
         j = j + 2;
         while (buffer[j] != '\0')
         {
            *(result+k) = buffer[j];
            k++;
            j++;
         }
         *(result+k) = '\0';
         fclose(fp);
         return 1;
      }
   }
   fclose(fp);
   printf("can not find the configure you need\n");
   return -1;
}

void InitParam(void)
{
    int ip[NODENUM];
    //record the accept socket fd of the connected client
    int conn;
    int param_connect[NODENUM];

    int param_send_buffer[32+NODENUM];
    int param_recv_buffer[1];

    int master_sockfd;
    int port = param_port;
    // use the TCP protocol
    master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //bind
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);
    mastersock_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) == -1)
    {
        printf("parameter server bind error!\n");
        exit(1);
    }
    //listen

    if(listen(master_sockfd, LISTEN_QUEUE) == -1)
    {
        printf("parameter server listen error!\n");
        exit(1);
    }

    socklen_t slave_length;
    struct sockaddr_in slave_addr;
    slave_length = sizeof(slave_addr);
    int i = 0;
    int j;
    while(i < NODENUM)
    {
        conn = accept(master_sockfd, (struct sockaddr*)&slave_addr, &slave_length);
        param_connect[i] = conn;

        if(conn < 0)
        {
            printf("param master accept connect error!\n");
            exit(1);
        }
        i++;
    }

    for (j = 0; j < NODENUM; j++)
    {
        int ret;
        ret = recv(param_connect[j], param_recv_buffer, sizeof(param_recv_buffer), 0);
        if (ret == -1)
            printf("param master recv error\n");
        ip[j] = param_recv_buffer[0];
    }

    for (j = 0; j < NODENUM; j++)
    {
        param_send_buffer[0] = NODENUM;
        param_send_buffer[1] = THREADNUM;
        param_send_buffer[2] = client_port;
        param_send_buffer[3] = message_port;
        param_send_buffer[4] = record_port;
        param_send_buffer[5] = j;
        param_send_buffer[6] = time_skew_max;

        param_send_buffer[7] = oneNodeWeight;
        param_send_buffer[8] = twoNodeWeight;
        param_send_buffer[9] = redo_limit;
        
        param_send_buffer[10] = HOTSPOT_PERCENTAGE;
        param_send_buffer[11] = HOTSPOT_FIXED_SIZE;
        param_send_buffer[12] = extension_limit;
        //random read control
        param_send_buffer[13] = random_read_limit;

        param_send_buffer[14] = benchmark_type;
        param_send_buffer[15] = transactions_per_terminal;
        param_send_buffer[16] = payment_weight_value;
        param_send_buffer[17] = order_status_weight_value;
        param_send_buffer[18] = delivery_weight_value;
        param_send_buffer[19] = stock_level_weight_value;
        param_send_buffer[20] = lim_per_min_terminal;
        param_send_buffer[21] = config_whse_count;
        param_send_buffer[22] = config_commit_count;
        param_send_buffer[23] = order_max_num;
        param_send_buffer[24] = max_data_lock_num;
        param_send_buffer[25] = scale_factor;
        param_send_buffer[26] = FREQUENCY_AMALGAMATE;
        param_send_buffer[27] = FREQUENCY_BALANCE;
        param_send_buffer[28] = FREQUENCY_DEPOSIT_CHECKING;
        param_send_buffer[29] = FREQUENCY_SEND_PAYMENT;
        param_send_buffer[30] = FREQUENCY_TRANSACT_SAVINGS;
        param_send_buffer[31] = FREQUENCY_WRITE_CHECK;

        int k;
        for (k = 0; k < NODENUM; k++)
        {
            param_send_buffer[32+k] = ip[k];
        }
        int ret;
        int size = (32+NODENUM)*sizeof(int);
        ret = send(param_connect[j], param_send_buffer, size, 0);
        if (ret == -1)
            printf("param naster send error\n");
        close(param_connect[j]);
    }
}

void InitRecord(void)
{
    //record the accept socket fd of the connected client
    int conn;

    int record_connect[NODENUM];

    int master_sockfd;
    int port = record_port;

    // use the TCP protocol
    master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //bind
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);
    mastersock_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) == -1)
    {
        printf("record server bind error!\n");
        exit(1);
    }

    //listen
    if(listen(master_sockfd, LISTEN_QUEUE) == -1)
    {
        printf("record server listen error!\n");
        exit(1);
    }

    socklen_t slave_length;
    struct sockaddr_in slave_addr;
    slave_length = sizeof(slave_addr);
       int i = 0;

    while(i < NODENUM)
    {
        conn = accept(master_sockfd, (struct sockaddr*)&slave_addr, &slave_length);
        record_connect[i] = conn;
        if(conn < 0)
        {
            printf("record master accept connect error!\n");
            exit(1);
        }
        i++;
    }

    int j;
    int ret;

    uint64_t **buf;
    buf = (uint64_t**)malloc(NODENUM*sizeof(uint64_t*));
    for (j = 0; j < NODENUM; j++)
    {
        buf[j] = (uint64_t*)malloc(5*sizeof(uint64_t));
    }

    // inform the node in the system begin start the transaction process.
    for (j = 0; j < NODENUM; j++)
    {
        ret = recv(record_connect[j], buf[j], 5*sizeof(uint64_t), 0);
        if (ret == -1)
            printf("record master recv error\n");
    }

    for (i = 0; i < NODENUM; i++)
        for (j = 0; j < NODENUM-i-1; j++)
        {
            uint64_t temp[5];
            if (buf[j][0] > buf[j+1][0])
            {
                temp[0] = buf[j][0];
                temp[1] = buf[j][1];
                temp[2] = buf[j][2];
                temp[3] = buf[j][3];
                temp[4] = buf[j][4];
                buf[j][0] = buf[j+1][0];
                buf[j][1] = buf[j+1][1];
                buf[j][2] = buf[j+1][2];
                buf[j][3] = buf[j+1][3];
                buf[j][4] = buf[j+1][4];
                buf[j+1][0] = temp[0];
                buf[j+1][1] = temp[1];
                buf[j+1][2] = temp[2];
                buf[j+1][3] = temp[3];
                buf[j+1][4] = temp[4];
            }
        }

    // flush the buffer
    printf("\n");
    for (i = 0; i < NODENUM; i++)
    {
      struct in_addr help;
      help.s_addr = (in_addr_t)buf[i][0];
      char * result = inet_ntoa(help);
        printf("%s %ld %ld %ld %ld\n", result, buf[i][1], buf[i][2], buf[i][3], buf[i][4]);
    }
    //now we can reform the node to begin to run the transaction.
}

void InitMessage(void)
{
    /* record the accept socket fd of the connected client */
    int conn;

    int message_connect[nodenum];
    int message_buffer[1];

    int master_sockfd;
    int port = message_port;

    master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);
    mastersock_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) == -1)
    {
        printf("message server bind error!\n");
        exit(1);
    }

    if(listen(master_sockfd, LISTEN_QUEUE) == -1)
    {
        printf("message server listen error!\n");
        exit(1);
    }

    socklen_t slave_length;
    struct sockaddr_in slave_addr;
    slave_length = sizeof(slave_addr);
       int i = 0;

    while(i < nodenum)
    {
        conn = accept(master_sockfd, (struct sockaddr*)&slave_addr, &slave_length);
        message_connect[i] = conn;
        if(conn < 0)
        {
            printf("message master accept connect error!\n");
            exit(1);
        }
        i++;
    }

    int j;
    int ret;

    /* inform the node in the system begin start the transaction process. */
    for (j = 0; j < nodenum; j++)
    {
        ret = recv(message_connect[j], message_buffer, sizeof(message_buffer), 0);
        if (ret == -1)
            printf("message master recv error\n");
    }

    for (j = 0; j < nodenum; j++)
    {
        message_buffer[0] = 999;
        ret = send(message_connect[j], message_buffer, sizeof(message_buffer), 0);
        if (ret == -1)
            printf("message master send error\n");
        close(message_connect[j]);
    }
    /* now we can reform the node to begin to run the transaction. */
}

void InitNetworkParam(void)
{
   char buffer[20];

   ReadConfig("thread_num", buffer);
   threadnum = atoi(buffer);

   ReadConfig("node_num", buffer);
   nodenum = atoi(buffer);

   ReadConfig("record_port", buffer);
   record_port = atoi(buffer);

   ReadConfig("client_port", buffer);
   client_port = atoi(buffer);

   ReadConfig("param_port", buffer);
   param_port = atoi(buffer);

   ReadConfig("message_port", buffer);
   message_port = atoi(buffer);

   ReadConfig("time_skew_max", buffer);
   time_skew_max = atoi(buffer);

   ReadConfig("master_ip", master_ip);

   ReadConfig("one_weight", buffer);
   oneNodeWeight = atoi(buffer);

   ReadConfig("two_weight", buffer);
   twoNodeWeight = atoi(buffer);

   ReadConfig("redo_limit", buffer);
   redo_limit = atoi(buffer);
   
   ReadConfig("hotspot_percentage", buffer);
   HOTSPOT_PERCENTAGE = atoi(buffer);
   
   ReadConfig("hotspot_fixed_size", buffer);
   HOTSPOT_FIXED_SIZE = atoi(buffer);
   
   ReadConfig("extension_limit", buffer);
   extension_limit = atoi(buffer);
   
   ReadConfig("random_read_limit", buffer);
   random_read_limit = atoi(buffer);

   ReadConfig("benchmark_type", buffer);

   if (strncmp(buffer, "TPCC", 4) == 0)
   {
       benchmark_type = TPCC;
   }
   else if (strncmp(buffer, "SMALLBANK", 9) == 0)
   {
       benchmark_type = SMALLBANK;
   }
   else
   {
       printf("read benchmark type error!\n");
       exit(-1);
   }

   ReadConfig("transactions_per_terminal", buffer);
   transactions_per_terminal = atoi(buffer);

   ReadConfig("payment_weight_value", buffer);
   payment_weight_value = atoi(buffer);

   ReadConfig("order_status_weight_value", buffer);
   order_status_weight_value = atoi(buffer);

   ReadConfig("delivery_weight_value", buffer);
   delivery_weight_value = atoi(buffer);

   ReadConfig("stock_level_weight_value", buffer);
   stock_level_weight_value = atoi(buffer);

   ReadConfig("lim_per_min_terminal", buffer);
   lim_per_min_terminal = atoi(buffer);

   ReadConfig("config_whse_count", buffer);
   config_whse_count = atoi(buffer);

   ReadConfig("config_commit_count", buffer);
   config_commit_count = atoi(buffer);

   ReadConfig("order_max_num", buffer);
   order_max_num = atoi(buffer);

   ReadConfig("max_data_lock_num", buffer);
   max_data_lock_num = atoi(buffer);
   
   ReadConfig("scale_factor", buffer);
   scale_factor = atoi(buffer);
   
   ReadConfig("FREQUENCY_AMALGAMATE", buffer);
   FREQUENCY_AMALGAMATE = atoi(buffer);
   
   ReadConfig("FREQUENCY_BALANCE", buffer);
   FREQUENCY_BALANCE = atoi(buffer);
   
   ReadConfig("FREQUENCY_DEPOSIT_CHECKING", buffer);
   FREQUENCY_DEPOSIT_CHECKING = atoi(buffer);

   ReadConfig("FREQUENCY_SEND_PAYMENT", buffer);
   FREQUENCY_SEND_PAYMENT = atoi(buffer);
   
   ReadConfig("FREQUENCY_TRANSACT_SAVINGS", buffer);
   FREQUENCY_TRANSACT_SAVINGS = atoi(buffer);
   
   ReadConfig("FREQUENCY_WRITE_CHECK", buffer);
   FREQUENCY_WRITE_CHECK = atoi(buffer);
}
