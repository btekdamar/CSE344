#include "client.h"

void
SIGINThandler(int sigNo) {
    closeResources();
    exit(EXIT_SUCCESS);
}

/*
I parsed the user-entered arguments.
I read the file containing the requests to be sent line by line and kept all the requests in a list.
Then I created threads that will send requests and receive responses.
*/
int main(int argc, char *const argv[]) {
    int opt;
    if (argc != 7) {
        fprintf(stderr, "USAGE: ./client -r requestFile -q PORT -s IP\n");
        return -1;
    }
    
    while ((opt = getopt(argc, argv, "r:q:s:")) != -1) {
        switch (opt) {
        case 'r':
            requestFileName = optarg;
            break;
        case 'q':
            portValue = atoi(optarg);
            break;
        case 's':
            ipValue = optarg;
            break;
        default:
            fprintf(stderr, "USAGE: ./client -r requestFile -q PORT -s IP\n");
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
    readFile();
    fprintf(stdout, "Client: I have loaded %d requests and I'm creating %d threads.\n", requestCount, requestCount);
    createThreads();

    fprintf(stdout, "Client: All threads have terminated, goodbye.\n");

    closeResources();
    return 0;
}

/*
Function where I read the file and create the list.
*/
void readFile() {
    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;
    char transactionCount[ITEM_LEN];
    char type[ITEM_LEN];
    char startDate[ITEM_LEN], endDate[ITEM_LEN];
    char city[ITEM_LEN];
    fd = fopen(requestFileName, "r");

    if (fd == NULL) {
        perror("Open File Error");
        exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &len, fd)) != -1) {
        if (read > 1) {
            struct Node* link = (struct Node*) malloc(sizeof(struct Node));
            struct Node* last = head;
            link->next = NULL;
            requestCount++;
            link->info.id = requestCount;
            link->info.message = (char*) malloc(read * sizeof(char));
            line[strcspn(line, "\n")] = 0;
            memset(transactionCount, 0, ITEM_LEN);
            memset(type, 0, ITEM_LEN);
            memset(startDate, 0, ITEM_LEN);
            memset(endDate, 0, ITEM_LEN);
            memset(city, 0, ITEM_LEN);
            sscanf(line, "%s %s %s %s %s", 
                        transactionCount, type, startDate, endDate, city);
            strcpy(link->info.city, city);
            strcpy(link->info.message, line);
            if (head == NULL) {
                head = link;
            } else {
                while (last->next != NULL) {
                    last = last->next;
                }
                last->next = link;
            }             
        }
    }
    free(line);
    fclose(fd);
}

/*
The function where I create the threads.
*/
void createThreads() {
    struct Node* current = head;
    while (current != NULL) {
        if (pthread_create(&current->info.requestThread, NULL, client, (void *) &current->info) != 0) {
            perror("pthread_create error");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    current = head;
    while (current != NULL) {
        if (pthread_join(current->info.requestThread, NULL) != 0) {
            perror("pthread_join error");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }
    

}

/*
Thread function. 
It does not start sending messages until all threads are created.
*/
void *client(void *info) {
    struct requestInfo clientInfo = *(struct requestInfo *)info;
    fprintf(stdout, "Client-Thread-%d: Thread-%d has been created\n", clientInfo.id, clientInfo.id);
    pthread_mutex_lock(&mutex);
    if (++arrived < requestCount) {
        pthread_cond_wait(&condVar, &mutex);
    } else {
        pthread_cond_broadcast(&condVar);
    }

    int subFd;
    struct sockaddr_in workerAdd;
    subFd = socket(PF_INET, SOCK_STREAM, 0);
    workerAdd.sin_family = AF_INET;
    workerAdd.sin_addr.s_addr = inet_addr(ipValue);
    workerAdd.sin_port = htons(portValue);
    connect(subFd, (struct sockaddr *)&workerAdd, sizeof(workerAdd));
    
    fprintf(stdout, "Client-Thread-%d: I am requesting \"%s\"\n", clientInfo.id, clientInfo.message);
    int len = strlen(clientInfo.message);
    if (write(subFd, "C", sizeof(char)) == -1 || 
        write(subFd, clientInfo.message, len) == -1) {
        perror("Write Error");
        close(subFd);
        return NULL;
    }

    pthread_mutex_unlock(&mutex);
    
    int ans;
    if (read(subFd, &ans, sizeof(int)) == -1) {
        perror("Read Error val");
        return NULL;
    }

    if (ans == -1) {
        if (strlen(clientInfo.city) == 0) {
            fprintf(stdout, "Client-Thread-%d: There is no available servant\n", clientInfo.id);
        } else {
            fprintf(stdout, "Client-Thread-%d: There is no servant responsible for %s\n", clientInfo.id, clientInfo.city);
        }
    } else {
        fprintf(stdout, "Client-Thread-%d: The server's response to \"%s\" is %d\n", clientInfo.id, clientInfo.message, ans);
    }
    
    close(subFd);
    fprintf(stdout, "Client-Thread-%d: Terminating\n", clientInfo.id);
    return NULL;
}

/*
The function where I close all open sources and pointers in the program.
*/
void closeResources() {
    struct Node* current = head;
    
    while (current != NULL) {
        struct Node* link = current->next;
        free(current->info.message);
        free(current);
        current = link;
    }

    pthread_cond_destroy(&condVar);
}