#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define ITEM_LEN 128

char *requestFileName,
     *ipValue;
int portValue;
int requestCount = 0;
int arrived = 0;
FILE *fd;
/*
The condition variable and mutex I use to wait for all threads to create.
*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condVar = PTHREAD_COND_INITIALIZER;

/*
The struct I use to hold threads and requests that threads will send.
*/
struct requestInfo {
    int id;
    char city[ITEM_LEN];
    char *message;
    pthread_t requestThread; 
};

/*
I kept the requests to be sent as a linked list. 
A struct for the nodes of this list.
*/
struct Node {
    struct requestInfo info;
    struct Node* next;
};

struct Node* head = NULL;

void readFile();
void createThreads();
void *client(void *info);
void closeResources();