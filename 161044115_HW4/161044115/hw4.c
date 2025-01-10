#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FALSE 0
#define TRUE 1

int C, N;
char *inputFile;
int semConsumer;
int fd;
pthread_t supplierThread;
pthread_t *consumerThread;
sig_atomic_t control = TRUE;

union semUnion {
    int val;
    struct semid_ds *buf;
    unsigned short array[1];
    struct seminfo *__buf;
} semAtt;

void waitSem(int sem, unsigned short a, unsigned short b);
void postSem(int sem, unsigned short n);
void *consumer(void* id);
void *supplier();
void initSemaphore();
void printString(const char *str);
void printConsumer(int id, int c, int num);
void printProducer(int val, char c);

void
SIGINThandler(int sigNo) {
    control = FALSE;
    
    if (semctl(semConsumer, 0, IPC_RMID) == -1) {
        perror("semaphore close error");
        exit(EXIT_FAILURE);
    } 
    free(consumerThread); 
    close(fd);
    exit(EXIT_FAILURE);
}

int main(int argc, char *const argv[]) {
    int opt; 
    if (argc != 7) {
        fprintf(stderr, "USAGE: ./hw4 -C consumerNum -N iterationNum -F inputFilePath");
        return -1;
    }

    while ((opt = getopt(argc, argv, "C:N:F:")) != -1) {
        switch (opt) {
        case 'C':
            C = atoi(optarg);
            break;
        case 'N':
            N = atoi(optarg);
            break;
        case 'F':
            inputFile = optarg;
            break;
        default:
            fprintf(stderr, "USAGE: ./hw4 -C consumerNum -N iterationNum -F inputFilePath");
            return -1;
        }
    }

    struct sigaction sac;
    sac.sa_flags = 0;
    sac.sa_handler = SIGINThandler;

    if(sigemptyset(&sac.sa_mask) == -1 || sigaction(SIGINT, &sac, NULL) == -1) {
        perror("Sigaction Error");
        return -1;
    }

    if (C <= 4) {
        fprintf(stderr, "C value must be greater than 4");
        exit(EXIT_FAILURE);
    }

    if (N <= 1) {
        fprintf(stderr, "N value must be greater than 1");
        exit(EXIT_FAILURE);
    }

    fd = open(inputFile, O_RDONLY);
    if (fd == -1) {
        perror("open file error");
        return -1;
    }

    initSemaphore();

    consumerThread = (pthread_t*)malloc(C * sizeof(pthread_t));
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&supplierThread, &attr, supplier, NULL) != 0) {
        perror("pthread_create error");
        exit(EXIT_FAILURE);
    }

    int threadNo[C];
    for (int i = 0; i < C; i++) {
        threadNo[i] = i;
        if (pthread_create(&consumerThread[i], NULL, consumer, (void *) &threadNo[i]) != 0) {
            perror("pthread_create error");
            exit(EXIT_FAILURE);
        }
    }
    
    for (int i = 0; i < C; i++) {
        if (pthread_join(consumerThread[i], NULL) != 0) {
            perror("pthread_join error");
            exit(EXIT_FAILURE);
        }
    }

    control = FALSE;
    if (semctl(semConsumer, 0, IPC_RMID) == -1) {
        perror("semaphore close error");
        exit(EXIT_FAILURE);
    }

    free(consumerThread);
    close(fd);
    pthread_exit(NULL);
}

void waitSem(int sem, unsigned short a, unsigned short b) {
    struct sembuf sop[2];
    sop[0].sem_num = a;
    sop[0].sem_op = -1;
    sop[0].sem_flg = 0;
    sop[1].sem_num = b;
    sop[1].sem_op = -1;
    sop[1].sem_flg = 0;

    if(semop(sem, sop, 2) == -1) {
        fprintf(stderr, "waitSem semop error");
        exit(EXIT_FAILURE);
    }
}

void postSem(int sem, unsigned short n) {
    struct sembuf sop;
    sop.sem_num = n;
    sop.sem_op = 1;
    sop.sem_flg = 0;
    if(semop(sem, &sop, 1) == -1) {   
        fprintf(stderr, "postSem semop error");
        exit(EXIT_FAILURE);
    }
}

void *supplier() {
    char buf;
    int val1, val2;
    while (read(fd, &buf, 1) == 1 && control == TRUE) {
        if (buf == '1') {
            val1 = semctl(semConsumer, 0, GETVAL, semAtt);
            val2 = semctl(semConsumer, 1, GETVAL, semAtt);
            fprintf(stdout, "%d Supplier: read from input a '%c'. Current amounts: %d x '1', %d x '2'\n", 
                        (int)time(NULL), buf, val1, val2);
            postSem(semConsumer, 0);
            val1 = semctl(semConsumer, 0, GETVAL, semAtt);
            val2 = semctl(semConsumer, 1, GETVAL, semAtt);
            
            fprintf(stdout, "%d Supplier: delivered a '%c'. Post-delivery amounts: %d x '1', %d x '2'\n", 
                        (int)time(NULL), buf, val1, val2);
        } else if (buf == '2') {
            val1 = semctl(semConsumer, 0, GETVAL, semAtt);
            val2 = semctl(semConsumer, 1, GETVAL, semAtt);
            fprintf(stdout, "%d Supplier: read from input a '%c'. Current amounts: %d x '1', %d x '2'\n", 
                        (int)time(NULL), buf, val1, val2);
            postSem(semConsumer, 1);
            val1 = semctl(semConsumer, 0, GETVAL, semAtt);
            val2 = semctl(semConsumer, 1, GETVAL, semAtt);
            fprintf(stdout, "%d Supplier: delivered a '%c'. Post-delivery amounts: %d x '1', %d x '2'\n", 
                        (int)time(NULL), buf, val1, val2);
        }
    }
    
    pthread_exit(NULL);
}

void *consumer(void *id) {
    int j = 0;
    int val1, val2;
    while(j < N && control == TRUE) {
        val1 = semctl(semConsumer, 0, GETVAL, semAtt);
        val2 = semctl(semConsumer, 1, GETVAL, semAtt);
        
        fprintf(stdout, "%d Consumer-%d at iteration %d (waiting). Current amounts: %d x '1', %d x '2'\n", 
                        (int)time(NULL), *(int*)id, j, val1, val2);
        waitSem(semConsumer, 0, 1);        
        j++;
        val1 = semctl(semConsumer, 0, GETVAL, semAtt);
        val2 = semctl(semConsumer, 1, GETVAL, semAtt);
        fprintf(stdout, "%d Consumer-%d at iteration %d (consumed). Post-consumtion amounts: %d x '1', %d x '2'\n", 
                        (int)time(NULL), *(int*)id, j - 1, val1, val2);
    }
    fprintf(stdout, "%d Consumer-%d has left\n", 
                    (int)time(NULL), *(int*)id);
    pthread_exit(NULL);
}

void initSemaphore() {
    semConsumer = semget(IPC_PRIVATE, 2, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (semConsumer == -1) {
        perror("semget error");
        exit(EXIT_FAILURE);
    }

    semAtt.val = 0;
    for (int i = 0; i < 2; i++) {
        if (semctl(semConsumer, i, SETVAL, semAtt) == -1){
            perror("semctl error");
            exit(EXIT_FAILURE);
        }
    }
}