#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "proc.h"
#include "socket.h"
#include "data.h"
#include "thread_main.h"
#include "config.h"

void* Respond(void *sockp);

int oneNodeWeight;
int twoNodeWeight;

int redo_limit;

// hotspot control
int HOTSPOT_PERCENTAGE;
int HOTSPOT_FIXED_SIZE;

// duration control
int extension_limit;

// random read control
int random_read_limit;

int nodenum;
int threadnum;
int time_skew_max;
int recordfd;
FILE * conf_fp;
int nodeid;

int master_port;
int message_port;
int param_port;
int record_port;
int port_base;

char master_ip[20];
char local_ip[20];

//the number of nodes should not be larger than NODE NUMBER MAX
char node_ip[NODENUMMAX][20];

//send buffer and receive buffer for client
uint64_t ** send_buffer;
uint64_t ** recv_buffer;

//send buffer for server respond.
uint64_t ** ssend_buffer;
uint64_t ** srecv_buffer;

/*
  * this array is used to save the connect with other nodes in the distributed system.
  * the connect with other slave node should be maintained until end of the process.  */

int connect_socket[NODENUMMAX][THREADNUMMAX];

int message_socket;
int param_socket;

pthread_t * server_tid;

// read the configure parameters from the configure file.
int ReadConfig(char * find_string, char * result)
{
    int i;
    int j;
    int k;
    char buffer[50];
    char * p;

    for (i = 0; i < LINEMAX; i++)
    {
        if (fgets(buffer, sizeof(buffer), conf_fp) == NULL)
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
            // jump over the character ':' and the space character.
            j = j + 2;
            while (buffer[j] != '\0')
            {
                *(result+k) = buffer[j];
                k++;
                j++;
            }
                *(result+k) = '\0';
                rewind(conf_fp);
                return 1;
        }
    }
    rewind(conf_fp);
    printf("can not find the configure you need\n");
    return -1;
}

void InitClientBuffer(void)
{
   int i;

   send_buffer = (uint64_t **) malloc (THREADNUM * sizeof(uint64_t *));
   recv_buffer = (uint64_t **) malloc (THREADNUM * sizeof(uint64_t *));

   if (send_buffer == NULL || recv_buffer == NULL)
       printf("client buffer pointer malloc error\n");

   for (i = 0; i < THREADNUM; i++)
   {
       send_buffer[i] = (uint64_t *) malloc (SEND_BUFFER_MAXSIZE * sizeof(uint64_t));
       recv_buffer[i] = (uint64_t *) malloc (RECV_BUFFER_MAXSIZE * sizeof(uint64_t));
       if ((send_buffer[i] == NULL) || recv_buffer[i] == NULL)
           printf("client buffer malloc error\n");
   }
}

void InitServerBuffer(void)
{
   int i;
   ssend_buffer = (uint64_t **) malloc ((NODENUM*THREADNUM+1) * sizeof(uint64_t *));
   srecv_buffer = (uint64_t **) malloc ((NODENUM*THREADNUM+1) * sizeof(uint64_t *));

   if (ssend_buffer == NULL || srecv_buffer == NULL)
       printf("server buffer pointer malloc error\n");

   for (i = 0; i < NODENUM*THREADNUM+1; i++)
   {
       ssend_buffer[i] = (uint64_t *) malloc (SSEND_BUFFER_MAXSIZE * sizeof(uint64_t));
       srecv_buffer[i] = (uint64_t *) malloc (SRECV_BUFFER_MAXSIZE * sizeof(uint64_t));
       if ((ssend_buffer[i] == NULL) || srecv_buffer[i] == NULL)
           printf("server buffer malloc error\n");
   }
}

void InitParamClient(void)
{
    int slave_sockfd;
    slave_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int port = param_port;
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_addr.s_addr = inet_addr(master_ip);
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);

    if (connect(slave_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) < 0)
    {
        printf("client master param connect error!\n");
        exit(1);
    }

    param_socket = slave_sockfd;
}

void InitMessageClient(void)
{
    int slave_sockfd;
    slave_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int port = message_port;
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_addr.s_addr = inet_addr(master_ip);
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);

    if (connect(slave_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) < 0)
    {
        printf("client master message connect error!\n");
        exit(1);
    }

    message_socket = slave_sockfd;
}

void InitClient(int nid, int threadid)
{
    int slave_sockfd;
    slave_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int port = port_base + nid;
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_addr.s_addr = inet_addr(node_ip[nid]);
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);

    if (connect(slave_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) < 0)
    {
        printf("client connect error!\n");
        exit(1);
    }

    connect_socket[nid][threadid] = slave_sockfd;
}

void InitRecordClient(void)
{
    int slave_sockfd;
    slave_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int port = record_port;
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_addr.s_addr = inet_addr(master_ip);
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);

    if (connect(slave_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) < 0)
    {
        printf("client record connect error\n");
        exit(1);
    }
    recordfd = slave_sockfd;
}

void InitServer(int nid)
{
    int status;
    int conn;
    void * pstatus;

    server_tid = (pthread_t *) malloc ((NODENUM*THREADNUM+1)*sizeof(pthread_t));
    server_arg *argu = (server_arg *)malloc((NODENUM*THREADNUM+1)*sizeof(server_arg));

    int master_sockfd;
    int port = port_base + nid;
    // use the TCP protocol
    master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // bind
    struct sockaddr_in mastersock_addr;
    memset(&mastersock_addr, 0, sizeof(mastersock_addr));
    mastersock_addr.sin_family = AF_INET;
    mastersock_addr.sin_port = htons(port);
    mastersock_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_sockfd, (struct sockaddr *)&mastersock_addr, sizeof(mastersock_addr)) == -1)
    {
        printf("bind error!\n");
        exit(1);
    }

    // listen
    if(listen(master_sockfd, LISTEN_QUEUE) == -1)
    {
        printf("listen error!\n");
        exit(1);
    }

    // force the transaction process waiting for the completion of the server's initialization.
    sem_post(wait_server);

    printf("server %d is ready\n", nodeid);
    // receive or transfer data
    socklen_t slave_length;
    struct sockaddr_in slave_addr;
    slave_length = sizeof(slave_addr);
        int i = 0;

    while(i < NODENUM*THREADNUM+1)
    {
        conn = accept(master_sockfd, (struct sockaddr*)&slave_addr, &slave_length);
        argu[i].conn = conn;
        argu[i].index = i;
        if(conn < 0)
        {
            printf("server accept connect error!\n");
            exit(1);
        }
        status = pthread_create(&server_tid[i], NULL, Respond, &(argu[i]));
        if (status != 0)
            printf("create thread %d error %d!\n", i, status);
        i++;
    }

    // wait the child threads to complete.
    for (i = 0; i < NODENUM*THREADNUM+1; i++)
       pthread_join(server_tid[i], &pstatus);
}

void* Respond(void *pargu)
{
    int conn;
    int index;
    server_arg * temp;
    temp = (server_arg *) pargu;
    conn = temp->conn;
    index = temp->index;
    command type;
    memset(srecv_buffer[index], 0, SRECV_BUFFER_MAXSIZE*sizeof(uint64_t));
    int ret;
    do
    {
           ret = recv(conn, srecv_buffer[index], SRECV_BUFFER_MAXSIZE*sizeof(uint64_t), 0);

           if (ret == -1)
           {
               printf("server receive error!\n");
           }

           type = (command)(*(srecv_buffer[index]));
           switch(type)
           {
              case cmd_insert:
                  ProcessInsert(srecv_buffer[index], conn, index);
                  break;
              case cmd_update:
                  ProcessUpdate(srecv_buffer[index], conn, index);
                  break;
              case cmd_read:
                  ProcessRead(srecv_buffer[index], conn, index);
                  break;
              case cmd_commit:
                  ProcessCommit(srecv_buffer[index], conn, index);
                  break;
              case cmd_prepare:
                  ProcessPrepare(srecv_buffer[index], conn, index);
                  break;
              case cmd_abort:
                  ProcessAbort(srecv_buffer[index], conn, index);
                  break;
              case cmd_release:
                  break;
              default:
                  printf("error route, never here!\n");
                  break;
           }
    } while (type != cmd_release);

       close(conn);
       pthread_exit(NULL);
       return (void*)NULL;
}

void InitNetworkParam(void)
{
   char buffer[5];

   ReadConfig("paramport", buffer);
   param_port = atoi(buffer);

   ReadConfig("masterip", master_ip);

   ReadConfig("localip", local_ip);

   fclose(conf_fp);
}

void GetParam(void)
{
    int i;
    int param_send_buffer[1];
    int param_recv_buffer[32+NODENUMMAX];

    // register local IP to master node && get by other nodes in the system 
    in_addr_t help = inet_addr(local_ip);

    if (help == INADDR_NONE)
    {
        printf("inet addr error\n");
        exit(-1);
    }

    param_send_buffer[0] = (uint32_t)help;

    if (send(param_socket, param_send_buffer, sizeof(param_send_buffer), 0) == -1)
        printf("get param send error\n");
    if (recv(param_socket, param_recv_buffer, sizeof(param_recv_buffer), 0) == -1)
        printf("get param recv error\n");

    nodenum = param_recv_buffer[0];
    threadnum = param_recv_buffer[1];
    port_base = param_recv_buffer[2];
    message_port = param_recv_buffer[3];
    record_port = param_recv_buffer[4];
    nodeid = param_recv_buffer[5];
    time_skew_max = param_recv_buffer[6];

    oneNodeWeight = param_recv_buffer[7];
    twoNodeWeight = param_recv_buffer[8];
    redo_limit = param_recv_buffer[9];
       
    // hotspot control
    HOTSPOT_PERCENTAGE = param_recv_buffer[10];
    HOTSPOT_FIXED_SIZE = param_recv_buffer[11];

    // duration control
    extension_limit = param_recv_buffer[12];
       
    // random read control
    random_read_limit = param_recv_buffer[13];

    benchmarkType = (BENCHMARK)param_recv_buffer[14];

    if (benchmarkType == TPCC)
    {
        TABLENUM = TPCC_TABLENUM;
    }
    else if (benchmarkType == SMALLBANK)
    {
        TABLENUM = SMALLBANK_TABLENUM;
    }
    else
    {
        printf("read benchmark type error!\n");
        exit(-1);
    }
    
    transactionsPerTerminal = param_recv_buffer[15];
    paymentWeightValue = param_recv_buffer[16];
    orderStatusWeightValue = param_recv_buffer[17];
    deliveryWeightValue = param_recv_buffer[18];
    stockLevelWeightValue = param_recv_buffer[19];
    limPerMin_Terminal = param_recv_buffer[20];
    configWhseCount = param_recv_buffer[21];
    configCommitCount = param_recv_buffer[22];
    OrderMaxNum = param_recv_buffer[23];
    MaxDataLockNum = param_recv_buffer[24];
    scaleFactor = param_recv_buffer[25] * 0.01;
    FREQUENCY_AMALGAMATE = param_recv_buffer[26];
    FREQUENCY_BALANCE = param_recv_buffer[27];
    FREQUENCY_DEPOSIT_CHECKING = param_recv_buffer[28];
    FREQUENCY_SEND_PAYMENT = param_recv_buffer[29];
    FREQUENCY_TRANSACT_SAVINGS = param_recv_buffer[30];
    FREQUENCY_WRITE_CHECK = param_recv_buffer[31];

    for (i = 0; i < nodenum; i++)
    {
        struct in_addr help; 
        help.s_addr = param_recv_buffer[32+i];
        char * result = inet_ntoa(help);
        int k;
        for (k = 0; result[k] != '\0'; k++)
        {
            node_ip[i][k] = result[k];
        }
        node_ip[i][k] = '\0';
    }
}

void WaitDataReady(void)
{
    int wait_buffer[1];
    wait_buffer[0] = 999;

    if (send(message_socket, wait_buffer, sizeof(wait_buffer), 0) == -1)
        printf("wait data send error\n");
    if (recv(message_socket, wait_buffer, sizeof(wait_buffer), 0) == -1)
        printf("wait data recv error\n");
}
