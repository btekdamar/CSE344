#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>

#define FALSE 0
#define TRUE 1
#define NAME "/shm_name"
#define SEM_NAME "/semaphore_shm"
#define MAX_SIZE 100000
#define ITEM_LEN 128
/*
Global variables
*/
char *cValue,
     *ipValue;
char directoryPath[512];
char firstCity[ITEM_LEN];
char lastCity[ITEM_LEN];
int portValue;
int startPoint;
int endPoint;
int connectionFd;
int requestCount = 0;
int myPid = 0;
int myPort = 0;
int servantFd = 0;
struct sockaddr_in servantAdd, clientAdd;
socklen_t clientSize = sizeof(clientAdd);
sig_atomic_t raiseSignal = FALSE;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *fd = NULL;

/*
This struct for the request information.
requestFd holds the fd of the socket from which the message came.
*/
struct requestInfo {
    char message[ITEM_LEN];
    int requestFd;
};

/*
I used this struct to keep the information in the files in order.
*/
struct DataNode {
    int transactionID;
    char type[ITEM_LEN];
    char streetName[ITEM_LEN];
    int surfaceSize;
    double price;
    struct DataNode *next;
};

/*
I kept the date information of the data as a separate struct.
*/
struct DateNode {
    char date[ITEM_LEN];
    struct DataNode *dataInfo;
    struct DateNode *next;
};

/*
I used this struct to hold the cities the servant is responsible for and the dates under those cities.
*/
struct CityNode {
    char cityName[256];
    struct DateNode *dateInfo;
    struct CityNode *next;
};

struct CityNode *cityHead = NULL;

/*
Functions prototypes
*/
void scanDirectory();
void closeResources();
int searchRequest(char* request);
int searchHelper(char *dataType, char *type, int dataYear, int dataMonth, int dataDay, 
            int startYear, int startMonth, int startDay, int endYear, int endMonth, int endDay);
int makeConnection();
void *answer(void *info);
pid_t getPid();