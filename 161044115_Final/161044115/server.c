#include "server.h"

void
SIGINThandler(int sigNo) {
    raiseSignal = TRUE;
    fprintf(stderr, "SIGINT has been received. I handled a total of %d requests. Goodbye.\n", totalRequest);
    pthread_cond_broadcast(&condRequest);
    closeResources();
    exit(EXIT_SUCCESS);
}

/*
I parsed the user-entered arguments. 
I created a shared memory for the initial value of the servants' ports and wrote the port number there.
*/
int main(int argc, char *const argv[]) {
    int opt;

    if (argc != 5) {
        fprintf(stderr, "USAGE: ./server -p PORT -t numberOfThreads\n");
        return -1;
    }
    
    while ((opt = getopt(argc, argv, "p:t:")) != -1) {
        switch (opt) {
        case 'p':
            portValue = atoi(optarg);
            break;
        case 't':
            threadNum = atoi(optarg);
            break;
        default:
            fprintf(stderr, "USAGE: ./server -p PORT -t numberOfThreads\n");
            return -1;
        }
    }

    if (threadNum < 5) {
        fprintf(stderr, "Thread number has to greater than 5\n");
        exit(EXIT_FAILURE);
    }
    

    struct sigaction sac;
    sac.sa_flags = 0;
    sac.sa_handler = SIGINThandler;

    if(sigemptyset(&sac.sa_mask) == -1 || sigaction(SIGINT, &sac, NULL) == -1) {
        perror("Sigaction Error");
        return -1;
    }

    sem_t *semMem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 1);
    int shmFd = shm_open(NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shmFd < 0) {
        perror("shm_open() error");
        exit(EXIT_FAILURE);
    }
    ftruncate(shmFd, sizeof(int));
    int *data = (int *)mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    data[0] = portValue;
    munmap(data, sizeof(int));
    close(shmFd);
    sem_close(semMem);

    queue = createQueue();
    createPool();
    makeConnection();
    shm_unlink(NAME);
    sem_unlink(SEM_NAME);
    free(queue);
    free(threadPool);
    return 0;
}

/*
I used this function to communicate with the servant and client.
I used the letters S and C to distinguish messages from servants and clients.
The presence of S at the beginning of a message indicates that it came from the servant, and the presence of C indicates that it came from the client.
Information from the servant is added to the servant list.
If a request is received by the client, it is added to the queue to be answered.
*/
int makeConnection() {
    char requestType;
    char request[ITEM_LEN];
    char servantRequest[ITEM_LEN];
    int fd = 0;
    struct sockaddr_in serverAdd, clientAdd;
    socklen_t clientSize = sizeof(clientAdd);

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Socket Error");
        return -1;
    }

    bzero(&serverAdd, sizeof(serverAdd));
    serverAdd.sin_family = AF_INET;
    serverAdd.sin_addr.s_addr = inet_addr(IP);
    serverAdd.sin_port = htons(portValue);

    if (bind(fd, (struct sockaddr *)&serverAdd, sizeof(serverAdd)) == -1) {
        perror("Bind Error");
        close(fd);
        return -1;
    }
    
    if (listen(fd, MAX_SIZE) == -1) {
        perror("Listen Error");
        close(fd);
        return -1;
    }
    
    while (raiseSignal == FALSE) {
        pthread_mutex_lock(&mutConnection);
        connectionFd = accept(fd, (struct sockaddr *)&clientAdd, &clientSize);
        if (connectionFd == -1) {
            perror("Accept Error");
            close(fd);
            return -1;
        }
        
        if (raiseSignal == TRUE) {
            break;
        }
        if (connectionFd == -1) {
            perror("Accept Error");
            close(fd);
            return -1;
        }
        

        if (read(connectionFd, &requestType, sizeof(char)) == -1) {
            perror("Read Error");
            close(fd);
            return -1;
        }

        if (requestType == 'C') {
            memset(request, 0, ITEM_LEN);

            if (read(connectionFd, request, ITEM_LEN) == -1) {
                perror("Read Error");
                close(fd);
                return -1;
            }
            enqueu(queue, request);
        } else {
            struct ServantNode* link = (struct ServantNode*) malloc(sizeof(struct ServantNode));
            struct ServantNode* last = servantHead;
            int tempPort = 0;
            int tempPid = 0;
            if (read(connectionFd, &tempPort, sizeof(int)) == -1) {
                perror("Read Error");
                close(fd);
                return -1;
            }

            link->portNumber = tempPort;

            if (read(connectionFd, &tempPid, sizeof(int)) == -1) {
                perror("Read Error");
                close(fd);
                return -1;
            }

            link->pid = tempPid;

            memset(servantRequest, 0, ITEM_LEN);
            if (read(connectionFd, servantRequest, ITEM_LEN) == -1) {
                perror("Read Error");
                close(fd);
                return -1;
            }

            sscanf(servantRequest, "%s %s", link->firstCity, link->lastCity);

            fprintf(stdout, "Servant %d present at port %d handling cities %s-%s\n", 
                            link->pid, link->portNumber, link->firstCity, link->lastCity);

            link->next = NULL;

            if (servantHead == NULL) {
                servantHead = link;
            } else {
                while (last->next != NULL) {
                    last = last->next;
                }
                last->next = link;
            }             
        }

        if (activeThread == threadNum) {
            fprintf(stderr, "All threads is busy! Waiting for it...\n");
            pthread_cond_wait(&condThread, &mutConnection);
        }

        pthread_cond_signal(&condRequest);
    }

    close(fd);
    return 0;
}

/*
This function creates as many threads as the number entered by the user.
*/
void createPool() {
    threadPool = (pthread_t *)malloc(threadNum * sizeof(pthread_t));
    int threadID[threadNum];
    for (size_t i = 0; i < threadNum; i++) {
        threadID[i] = i;
    }
    
    for (int i = 0; i < threadNum; i++) {
        if (pthread_create(&threadPool[i], NULL, worker, (void *) &threadID[i]) != 0) {
            free(threadPool);
            perror("pthread_create error");
            exit(EXIT_FAILURE);
        }
    }
}


/*
Thread function. 
It reads the request added to the queue and sends it to the relevant servant.
It sends the reply from the servant back to the client.
A value of -1 is returned to the client if a servant that was opened at the beginning is closed, 
or if there is no servant that can respond to the request from the client.
*/
void *worker(void * id) {
    char transactionCount[ITEM_LEN];
    char type[ITEM_LEN];
    char startDate[ITEM_LEN], endDate[ITEM_LEN];
    char city[ITEM_LEN];
    int workerConnFd;
    char request[ITEM_LEN];
    while (raiseSignal == FALSE) {
        pthread_cond_wait(&condRequest, &mutRequest);
        if (raiseSignal == TRUE) {
            close(connectionFd);
            break;
        }
        
        pthread_mutex_lock(&mutRead);
        ++activeThread;
        pthread_mutex_unlock(&mutRead);
        workerConnFd = connectionFd;
        pthread_mutex_unlock(&mutConnection);
        pthread_mutex_lock(&mutRead);
        if (queue->front != NULL) {
            memset(request, 0, ITEM_LEN);
            strcpy(request, queue->front->data);
            dequeue(queue);
            pthread_mutex_unlock(&mutRead);
            fprintf(stdout, "Request arrived \"%s\"\n", request);
            memset(transactionCount, 0, ITEM_LEN);
            memset(type, 0, ITEM_LEN);
            memset(startDate, 0, ITEM_LEN);
            memset(endDate, 0, ITEM_LEN);
            memset(city, 0, ITEM_LEN);
            sscanf(request, "%s %s %s %s %s", 
                        transactionCount, type, startDate, endDate, city);
            struct ServantNode* current = servantHead;
            int flag = FALSE;
            if (strlen(city) == 0) {
                int answer = 0;
                fprintf(stdout, "Contacting ALL servants\n");
                while (current != NULL) {
                    int servantConnFd;
                    struct sockaddr_in workerAdd;
                    servantConnFd = socket(PF_INET, SOCK_STREAM, 0);
                    workerAdd.sin_family = AF_INET;
                    workerAdd.sin_addr.s_addr = inet_addr(IP);
                    workerAdd.sin_port = htons(current->portNumber);
                    if(connect(servantConnFd, (struct sockaddr *)&workerAdd, sizeof(workerAdd)) == -1) {
                        close(servantConnFd);
                        current = current->next;
                        continue;
                    }
                    int len = strlen(request);
                    if (write(servantConnFd, request, len) == -1) {
                        perror("Write Error");
                        close(workerConnFd);
                        close(servantConnFd);
                        exit(EXIT_FAILURE);
                    }
                    flag = TRUE;
                    int ans = 0;

                    if (read(servantConnFd, &ans, sizeof(int)) == -1) {
                        perror("Read Error");
                        close(workerConnFd);
                        close(servantConnFd);
                        exit(EXIT_FAILURE);
                    }
                    answer += ans;

                    close(servantConnFd);
                    current = current->next;
                }
                if (flag == TRUE) {
                    totalRequest++;
                    fprintf(stdout, "Response received: %d, forwarded to client\n", answer);
                    if (write(workerConnFd, &answer, sizeof(int)) == -1) {
                        perror("Write Error");
                        close(workerConnFd);
                        exit(EXIT_FAILURE);
                    } 
                } else {
                    answer = -1;
                    totalRequest++;
                    fprintf(stderr, "There is no available servant\n");
                    if (write(workerConnFd, &answer, sizeof(int)) == -1) {
                        perror("Write Error");
                        close(workerConnFd);
                        exit(EXIT_FAILURE);
                    }
                }
            } else {  
                int ans = 0;
                while (current != NULL) {
                    if (strcmp(city, current->firstCity) >= 0 && strcmp(city, current->lastCity) <= 0) {
                        fprintf(stdout, "Contacting servant %d\n", current->pid);
                        int servantConnFd;
                        struct sockaddr_in workerAdd;
                        servantConnFd = socket(PF_INET, SOCK_STREAM, 0);
                        workerAdd.sin_family = AF_INET;
                        workerAdd.sin_addr.s_addr = inet_addr(IP);
                        workerAdd.sin_port = htons(current->portNumber);
                        if(connect(servantConnFd, (struct sockaddr *)&workerAdd, sizeof(workerAdd)) == -1) {
                            flag = FALSE;
                            break;
                        }
                        int len = strlen(request);
                        if (write(servantConnFd, request, len) == -1) {
                            perror("Write Error");
                            close(workerConnFd);
                            close(servantConnFd);
                            exit(EXIT_FAILURE);
                        }

                        if (read(servantConnFd, &ans, sizeof(int)) == -1) {
                            perror("Read Error");
                            close(workerConnFd);
                            close(servantConnFd);
                            exit(EXIT_FAILURE);
                        }
                        
                        flag = TRUE;
                        close(servantConnFd);
                        break;
                    }
                    current = current->next;
                }
                if (flag == TRUE) {
                    totalRequest++;
                    fprintf(stdout, "Response received: %d, forward to client\n", ans);
                    if (write(workerConnFd, &ans, sizeof(int)) == -1) {
                        perror("Write Error");
                        close(workerConnFd);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    ans = -1;
                    totalRequest++;
                    fprintf(stderr, "There is no servant responsible for %s\n", city);
                    if (write(workerConnFd, &ans, sizeof(int)) == -1) {
                        perror("Write Error");
                        close(workerConnFd);
                        exit(EXIT_FAILURE);
                    }
                }
            }
        } else {
            pthread_mutex_unlock(&mutRead);
        }
        
        pthread_mutex_lock(&mutRead);
        --activeThread;
        pthread_mutex_unlock(&mutRead);
        close(workerConnFd);
        pthread_cond_signal(&condThread);
    }
    return NULL;
}

/*
The function I use to create the queue.
*/
struct Queue* createQueue() {
    struct Queue* queue = (struct Queue *) malloc(sizeof(struct Queue));
    queue->front = NULL;
    queue->rear = NULL;
    return queue;
}

/*
The function I use to add data to the given queue.
*/
void enqueu(struct Queue* queue, char* item) {
    struct QNode* link = (struct QNode *)malloc(sizeof(struct QNode));
    strcpy(link->data, item);
    link->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = link;
        return;
    }
    queue->rear->next = link;
    queue->rear = link;
}

/*
Here is the function I use to remove items from the queue.
*/
void dequeue(struct Queue* queue) {
    if (queue->front == NULL) {
        return;
    }

    memset(queue->front->data, 0, 256);
    struct QNode* link = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    
    free(link);
}

/*
The function where I close all open sources and pointers in the program.
*/
void closeResources() {
    shm_unlink(NAME);
    sem_unlink(SEM_NAME);
    struct ServantNode* current = servantHead;
    while (current != NULL) {
        struct ServantNode* link = current->next;
        kill(current->pid, SIGINT);
        free(current);
        current = link;
    }
    while (queue->front != NULL) {
        dequeue(queue);
    }
    free(threadPool);
    free(queue);
    close(connectionFd);
    pthread_cond_destroy(&condThread);
    pthread_cond_destroy(&condRequest);
}