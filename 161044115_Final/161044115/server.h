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
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>

#define FALSE 0
#define TRUE 1
#define IP "127.0.0.1"
#define MAX_SIZE 1000000
#define NAME "/shm_name"
#define SEM_NAME "/semaphore_shm"
#define ITEM_LEN 128

int portValue,
    threadNum,
    connectionFd;

int activeThread = 0;
int totalRequest = 0;

/*
I used this struct to keep the information of the servants connecting to the server.
*/
struct ServantNode {
    char firstCity[ITEM_LEN];
    char lastCity[ITEM_LEN];
    int portNumber;
    int pid;
    struct ServantNode* next;
};

/*
I used it for the nodes of the queue structure.
*/
struct QNode {
    char data[256];
    struct QNode *next;
};

/*
The struct I use for the queue structure.
*/
struct Queue {
    struct QNode *front, *rear;
};

struct Queue* queue;
struct ServantNode* servantHead = NULL;

sig_atomic_t raiseSignal = FALSE;
pthread_t *threadPool;

/*
Condition variables and mutexes that I use for synchronization.
*/
pthread_mutex_t mutConnection = PTHREAD_MUTEX_INITIALIZER,
                mutRequest = PTHREAD_MUTEX_INITIALIZER,
                mutRead = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t condRequest = PTHREAD_COND_INITIALIZER,
               condThread = PTHREAD_COND_INITIALIZER;


struct Queue* createQueue();
void enqueu(struct Queue* queue, char* item);
void dequeue(struct Queue* queue);
int makeConnection();
void createPool();
void closeResources();
void *worker(void *id);