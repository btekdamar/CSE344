#include "servant.h"

void SIGINThandler(int sigNo) {
    raiseSignal = TRUE;
    fprintf(stderr, "Servant %d: termination message received, handled %d requests in total.\n", myPid, requestCount);
    closeResources();
    exit(EXIT_SUCCESS);
}

/*
I parsed the user-entered arguments. 
Then I read the port number from the shared memory I opened on the server, increase the value by 1 and check whether the port is empty. 
If the port is full, I increase the value and keep checking. 
After the appropriate port number is found, I write the port number of the servant to the shared memory again.
*/
int main(int argc, char *const argv[]) {
    int opt;
    char *tempDir;

    if (argc != 9) {
        fprintf(stderr, "USAGE: ./servant -d directoryPath -c 10-19 -r IP -p PORT\n");
        return -1;
    }
    
    while ((opt = getopt(argc, argv, "d:c:r:p:")) != -1) {
        switch (opt) {
        case 'd':
            tempDir = optarg;
            break;
        case 'c':
            cValue = optarg;
            break;
        case 'r':
            ipValue = optarg;
            break;
        case 'p':
            portValue = atoi(optarg);
            break;
        default:
            fprintf(stderr, "USAGE: ./servant -d directoryPath -c 10-19 -r IP -p PORT\n");
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

    if (tempDir[strlen(tempDir)-1] != '/') {
        sprintf(directoryPath, "%s/", tempDir);
    } else {
        sprintf(directoryPath, "%s", tempDir);
    }

    sscanf(cValue, "%d-%d", &startPoint, &endPoint);
    myPid = getPid();

    servantFd = socket(PF_INET, SOCK_STREAM, 0);
    if (servantFd == -1) {
        perror("Socket Error");
        return -1;
    }

    sem_t *semMem = sem_open(SEM_NAME, O_RDWR);
    sem_wait(semMem);
    int shmFd = shm_open(NAME, O_EXCL | O_RDWR, 0600);
    if (shmFd < 0) {
        perror("shm_open() error");
        exit(EXIT_FAILURE);
    }

    int *data = (int *)mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    myPort = data[0] + 1;
    if (myPort < 5000 || myPort > 60000) {
        myPort = 5000;
    }

    bzero(&servantAdd, sizeof(servantAdd));
    servantAdd.sin_family = AF_INET;
    servantAdd.sin_addr.s_addr = inet_addr(ipValue);
    servantAdd.sin_port = htons(myPort);
    
    while (bind(servantFd, (struct sockaddr *)&servantAdd, sizeof(servantAdd)) == -1) {
        myPort += 1;
        if (myPort < 5000 || myPort > 60000) {
            myPort = 5000;
        }
        
        servantAdd.sin_port = htons(myPort);
    }

    data[0] = myPort;
    munmap(data, sizeof(int));

    close(shmFd);
    sem_post(semMem);
    sem_close(semMem);
    scanDirectory();
    fprintf(stdout, "Servant %d: loaded dataset, cities %s-%s\nServant %d: listening at port %d\n", myPid, firstCity, lastCity, myPid, myPort);
    
    makeConnection();
    closeResources();
    return 0;
}

/*
This function scans all the folders in the file path entered by the user and keeps the data in the files in a link list.
I have nested the data as city, date and file content.
*/
void scanDirectory() {
    struct dirent **sdList;
    struct dirent **subSdList;
    int dirSize = 0;
    int subDirSize = 0;
    char subDirPath[1024];
    char subFileName[2048];
    char *line = NULL;
    ssize_t read = 0;
    size_t len = 0;

    dirSize = scandir(directoryPath, &sdList, NULL, alphasort);
    if (dirSize < 0) {
        perror("Scandir Error");
        exit(EXIT_FAILURE);
    } else {
        int index = 0;
        int count = 0;
        int subCount = 0;
        while(count < dirSize) {
            if (strcmp(sdList[count]->d_name, ".") == 0 || strcmp(sdList[count]->d_name, "..") == 0) {
                free(sdList[count]);
                count++;
                continue;
            } else {
                index++;
                if (index >= startPoint && index <= endPoint) {
                    if (index == startPoint) {
                        strcpy(firstCity, sdList[count]->d_name);
                    } else if (index == endPoint) {
                        strcpy(lastCity, sdList[count]->d_name);
                    }
                    struct CityNode *cityLink = (struct CityNode*) malloc(sizeof(struct CityNode));
                    struct CityNode *cityLast = cityHead;
                    cityLink->dateInfo = NULL;
                    sprintf(cityLink->cityName, "%s", sdList[count]->d_name);            
                    memset(subDirPath, 0, 1024);
                    sprintf(subDirPath, "%s%s", directoryPath, sdList[count]->d_name);
                    subDirSize = scandir(subDirPath, &subSdList, NULL, alphasort);
                    subCount = 0;
                    while (subCount < subDirSize) {
                        if (strcmp(subSdList[subCount]->d_name, ".") == 0 ||  strcmp(subSdList[subCount]->d_name, "..") == 0) {
                            free(subSdList[subCount]);
                            subCount++;
                            continue;
                        }
                        struct DateNode *dateLink = (struct DateNode*) malloc(sizeof(struct DateNode));
                        struct DateNode *dateLast = cityLink->dateInfo;
                        dateLink->dataInfo = NULL;
                        sprintf(dateLink->date, "%s", subSdList[subCount]->d_name);
                    
                        memset(subFileName, 0, 2048);
                        sprintf(subFileName, "%s/%s", subDirPath, subSdList[subCount]->d_name);
                        fd = fopen(subFileName, "r");
                        while ((read = getline(&line, &len, fd)) != -1) {
                            struct DataNode* dataLink = (struct DataNode*) malloc(sizeof(struct DataNode));
                            struct DataNode* dataLast = dateLink->dataInfo;
                            sscanf(line, "%d %s %s %d %lf", 
                                    &dataLink->transactionID, dataLink->type, 
                                    dataLink->streetName, &dataLink->surfaceSize, &dataLink->price);

                            dataLink->next = NULL;

                            if (dateLink->dataInfo == NULL) {
                                dateLink->dataInfo = dataLink;
                            } else {
                                while (dataLast->next != NULL) {
                                    dataLast = dataLast->next;
                                }
                                dataLast->next = dataLink;
                            }             
                        }
                        dateLink->next = NULL;
                        if (cityLink->dateInfo == NULL) {
                                cityLink->dateInfo = dateLink;
                        } else {
                            while (dateLast->next != NULL) {
                                dateLast = dateLast->next;
                            }
                            dateLast->next = dateLink;
                        }          
                        fclose(fd);
                        fd = NULL;
                        free(subSdList[subCount]);
                        subCount++;
                    }
                    cityLink->next = NULL;
                    if (cityHead == NULL) {
                            cityHead = cityLink;
                    } else {
                        while (cityLast->next != NULL) {
                            cityLast = cityLast->next;
                        }
                        cityLast->next = cityLink;
                    } 
                    free(subSdList);
                }
            }
            free(sdList[count]);
            count++;
        }
        free(sdList);
    }
    free(line);
}

/*
The function where I close all open sources and pointers in the program.
*/
void closeResources() {
    struct CityNode* current = cityHead;
    while (current != NULL) {
        struct DateNode* dateCurrent = current->dateInfo;
        while (dateCurrent != NULL) {
            struct DataNode* dataCurrent = dateCurrent->dataInfo;
            while (dataCurrent != NULL) {
                struct DataNode* linkData = dataCurrent->next;
                free(dataCurrent);
                dataCurrent = linkData;
            }
            struct DateNode* linkDate = dateCurrent->next;
            free(dateCurrent);
            dateCurrent = linkDate;
        }
        struct CityNode* link = current->next;
        free(current);
        current = link;
    }
    if (fd != NULL) {
        fclose(fd);
    }
    close(connectionFd);
    
}

/*
I used this function to communicate with the server. 
When a request comes from the server, it creates a thread and sends the response to the server through this thread.
*/
int makeConnection() {
    int socketFd;
    char cities[256];
    char request[ITEM_LEN];

    sprintf(cities, "%s %s", firstCity, lastCity);
    socketFd = socket(PF_INET, SOCK_STREAM, 0);
    servantAdd.sin_family = AF_INET;
    servantAdd.sin_addr.s_addr = inet_addr(ipValue);
    servantAdd.sin_port = htons(portValue);
    connect(socketFd, (struct sockaddr *)&servantAdd, sizeof(servantAdd));

    if (write(socketFd, "S", sizeof(char)) == -1 ||
        write(socketFd, &myPort, sizeof(int)) == -1 ||
        write(socketFd, &myPid, sizeof(int)) == -1 ||
        write(socketFd, &cities, strlen(cities)) == -1) {
        perror("Write Error");
        close(socketFd);
        return -1;
    }
    close(socketFd);
    
    if (listen(servantFd, MAX_SIZE) == -1) {
        perror("Listen Error");
        close(servantFd);
        return -1;
    }

    while (raiseSignal == FALSE) {
        pthread_mutex_lock(&mutex);
        connectionFd = accept(servantFd, (struct sockaddr *)&clientAdd, &clientSize);
        if (raiseSignal == TRUE) {
            break;
        }
        
        if (connectionFd == -1) {
            perror("Accept Error");
            close(servantFd);
            return -1;
        }

        memset(request, 0, sizeof(request));
        if (read(connectionFd, request, sizeof(request)) == -1) {
            perror("Read Error");
            close(servantFd);
            return -1;
        }
        if (raiseSignal == TRUE) {
            break;
        }
        
        struct requestInfo req;
        strcpy(req.message, request);
        req.requestFd = connectionFd;
        pthread_t newThread;
        if (pthread_create(&newThread, NULL, answer, (void *) &req) != 0) {
            perror("pthread_create error");
            return -1;
        }
    }
    close(servantFd);
    return 0;
}

/*
I use this function to get pid number of process.
It read /proc/self path and it read pid number
*/
pid_t getPid() {
    char info[32];
    memset(info, 0, sizeof(info));
    int pid = 0;
    readlink("/proc/self", info, sizeof(info));
    sscanf(info, "%d", &pid);
    return (pid_t) pid;
}

/*
Thread function.
The request is searched here and the answer is sent to the server from here.
*/
void *answer(void *info) {
    struct requestInfo workerInfo = *(struct requestInfo *)info;
    int ans = searchRequest(workerInfo.message);
    if (write(workerInfo.requestFd, &ans, sizeof(int)) == -1) {
        close(workerInfo.requestFd);
        perror("Write Error");
        exit(EXIT_FAILURE);
    }
    ++requestCount;
    pthread_mutex_unlock(&mutex);
    close(workerInfo.requestFd);
    return NULL;
}

/*
This function parses the incoming request and checks whether this servant is in the data list. 
It returns the number of data suitable for the given request.
*/
int searchRequest(char* request) {
    int count = 0;
    char transactionCount[ITEM_LEN];
    char type[ITEM_LEN];
    char startDate[ITEM_LEN], endDate[ITEM_LEN];
    char city[ITEM_LEN];
    memset(transactionCount, 0, ITEM_LEN);
    memset(type, 0, ITEM_LEN);
    memset(startDate, 0, ITEM_LEN);
    memset(endDate, 0, ITEM_LEN);
    memset(city, 0, ITEM_LEN);
    sscanf(request, "%s %s %s %s %s", 
                        transactionCount, type, startDate, endDate, city);
    int startDay, startMonth, startYear;
    int endDay, endMonth, endYear;
    sscanf(startDate, "%d-%d-%d", &startDay, &startMonth, &startYear);
    sscanf(endDate, "%d-%d-%d", &endDay, &endMonth, &endYear);
    struct CityNode* cityCurrent = cityHead;
    while (cityCurrent != NULL) {
        if (strlen(city) == 0) {
            struct DateNode* dateCurrent = cityCurrent->dateInfo;
            while (dateCurrent != NULL) {
                int dataDay = 0, dataMonth = 0, dataYear = 0;
                sscanf(dateCurrent->date, "%d-%d-%d", 
                        &dataDay, &dataMonth, &dataYear);
                struct DataNode* dataCurrent = dateCurrent->dataInfo;
                while (dataCurrent != NULL) {
                    count += searchHelper(dataCurrent->type, type, dataYear, dataMonth, dataDay, 
                                            startYear, startMonth, startDay, endYear, endMonth, endDay);
                    dataCurrent = dataCurrent->next;
                }
                dateCurrent = dateCurrent->next;
            }
        } else {
            if (strcmp(cityCurrent->cityName, city) == 0) {
                struct DateNode* dateCurrent = cityCurrent->dateInfo;
                while (dateCurrent != NULL) {
                    int dataDay = 0, dataMonth = 0, dataYear = 0;
                    sscanf(dateCurrent->date, "%d-%d-%d", 
                            &dataDay, &dataMonth, &dataYear);
                    struct DataNode* dataCurrent = dateCurrent->dataInfo;
                    while (dataCurrent != NULL) {
                        count += searchHelper(dataCurrent->type, type, dataYear, dataMonth, dataDay, 
                                            startYear, startMonth, startDay, endYear, endMonth, endDay);
                        dataCurrent = dataCurrent->next;
                    }
                    dateCurrent = dateCurrent->next;
                }
            }
        }
        cityCurrent = cityCurrent->next;
    }

    return count;
}

/*
I used this function for searching between dates.
*/
int searchHelper(char *dataType, char *type, int dataYear, int dataMonth, int dataDay, 
            int startYear, int startMonth, int startDay, int endYear, int endMonth, int endDay) {

    if ((dataYear < endYear && dataYear > startYear)) {
        if (strcmp(dataType, type) == 0) {
            return 1;
        }
    } else if (dataYear == endYear && dataYear == startYear) {
        if (dataMonth < endMonth && dataMonth > startMonth) {
            if (strcmp(dataType, type) == 0) {
                return 1;
            }
        } else if (dataMonth >= startMonth && dataMonth < endMonth) {
            if (dataDay >= startDay) {
                if (strcmp(dataType, type) == 0) {
                    return 1;
                }
            }
        } else if (dataMonth > startMonth && dataMonth <= endMonth) {
            if (dataDay <= endDay) {
                if (strcmp(dataType, type) == 0) {
                    return 1;
                }
            }
        } else if (dataMonth == endMonth && dataMonth == startMonth) {
            if (dataDay <= endDay && dataDay >= startDay) {
                if (strcmp(dataType, type) == 0) {
                    return 1;
                }
            }
        }   
    } else if (dataYear == endYear && dataYear > startYear) {
        if (dataMonth < endMonth) {
            if (strcmp(dataType, type) == 0) {
                return 1;
            }
        } else if (dataMonth == endMonth) {
            if (dataDay <= endDay) {
                if (strcmp(dataType, type) == 0) {
                    return 1;
                }
            }
        }  
    } else if (dataYear < endYear && dataYear == startYear) {
        if (dataMonth > startMonth) {
            if (strcmp(dataType, type) == 0) {
                return 1;
            }
        } else if (dataMonth == startMonth) {
            if (dataDay >= startDay) {
                if (strcmp(dataType, type) == 0) {
                    return 1;
                }
            }
        }   
    }
    return 0;
}