#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
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

char *inputFile1,
     *inputFile2,
     *outputFile;
int nValue = 0;
int mValue = 0;
int queue = 0;
int fdInput1;
int fdInput2;
int fdOutput;
int size = 0;
int count = 0;
int arrived = 0;
int arrived2 = 0;
int arrived3 = 0;
double totalTime = 0.0;
pthread_t *threads;
pthread_mutex_t mutex;
pthread_cond_t condVar;

struct arrayBlock {
    int *colArr;
    int **aMat;
    int **cMat;
    int ***bMat;
    double **real;
    double **imag;
} block;

void readFiles();
void writeFile();
void dft(int id, int col);
void *operations(void *data);
void closeResources();

void
SIGINThandler(int sigNo) {
    closeResources();
    exit(EXIT_FAILURE);
}

int main(int argc, char *const argv[]) {
    int opt; 
    if (argc != 11) {
        fprintf(stderr, "USAGE: ./hw5 -i filePath1 -j filePath2 -o output -n charecterNum -m threadNum\n");
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "i:j:o:n:m:")) != -1) {
        switch (opt) {
        case 'i':
            inputFile1 = optarg;
            break;
        case 'j':
            inputFile2 = optarg;
            break;
        case 'o':
            outputFile = optarg;
            break;
        case 'n':
            nValue = atoi(optarg);
            break;
        case 'm':
            mValue = atoi(optarg);
            break;
        default:
            fprintf(stderr, "USAGE: ./hw5 -i filePath1 -j filePath2 -o output -n charecterNum -m threadNum\n");
            exit(EXIT_FAILURE);
        }
    }

    if (mValue % 2 != 0) {
        fprintf(stderr, "m value must be equalt to the 2*k");
        exit(EXIT_FAILURE);
    }
    

    if (nValue <= 2) {
        fprintf(stderr, "n value must be greater than 2");
        exit(EXIT_FAILURE);
    }

    struct sigaction sac;
    sac.sa_flags = 0;
    sac.sa_handler = SIGINThandler;

    if(sigemptyset(&sac.sa_mask) == -1 || sigaction(SIGINT, &sac, NULL) == -1) {
        perror("Sigaction Error");
        return -1;
    }
    
    block.aMat = NULL;
    block.bMat = NULL;
    block.cMat = NULL;
    block.colArr = NULL;
    block.real = NULL;
    block.imag = NULL;
    threads = NULL;
    fdOutput = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    fdInput1 = open(inputFile1, O_RDONLY);
    fdInput2 = open(inputFile2, O_RDONLY);
    if (fdInput1 == -1 || fdInput2 == -1 || fdOutput == -1) {
        perror("Open file error");
        exit(EXIT_FAILURE);
    }

    size = pow(2.0, (double)nValue);

    if (mValue > size) mValue = size;
    
    threads = (pthread_t*) malloc(mValue * sizeof(pthread_t));
    pthread_cond_init(&condVar, NULL);
    pthread_mutex_init(&mutex, NULL);
    readFiles();
    int threadNo[mValue];
    for (int i = 0; i < mValue; i++) {
        threadNo[i] = i;
        if (pthread_create(&threads[i], NULL, operations, (void *) &threadNo[i]) != 0) {
            perror("pthread_create error");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < mValue; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join error");
            exit(EXIT_FAILURE);
        }
    }


    writeFile();
    
    closeResources();
    return 0;
}

void readFiles() {
    char bf1, bf2;
    int i = 0, j = 0;
    int charCount = 0;
    int colSize = size / mValue;

    block.colArr = (int*)malloc(mValue * sizeof(int));
    block.aMat = (int**)malloc(size * sizeof(int *));
    block.bMat = (int***)malloc(mValue * sizeof(int **));
    block.cMat = (int**)malloc(size * sizeof(int *));
    block.real = (double**)malloc(size * sizeof(double *));
    block.imag = (double**)malloc(size * sizeof(double *));

    for (int i = 0; i < size; i++) {
        block.aMat[i] = (int*)malloc(size * sizeof(int));
        block.cMat[i] = (int*)malloc(size * sizeof(int));
        block.real[i] = (double*)malloc(size * sizeof(double));
        block.imag[i] = (double*)malloc(size * sizeof(double));
    }

    while(read(fdInput1, &bf1, 1) == 1) {
        if (++charCount > (size*size)) {
            break;
        }
        
        if (j == size) {
            i++;
            j = 0;
        }
        block.aMat[i][j] = bf1;
        j++;
    }

    if (charCount < (size*size)) {
        fprintf(stderr, "Wrong input file\n");
        exit(EXIT_FAILURE);
    }
    

    if ((size % mValue) == 0) {
        int threadNo = 0;
        for (int i = 0; i < mValue; i++) {
            block.colArr[i] = colSize;
            block.bMat[i] = (int **)malloc(size * sizeof(int *));
            for (int j = 0; j < size; j++) {
                block.bMat[i][j] = (int *)malloc(colSize * sizeof(int));
            }
        }

        i = j = charCount = 0 ;
        while (read(fdInput2, &bf2, 1) == 1) {
            if (++charCount > (size*size)) {
                break;
            }
            block.bMat[threadNo][i][j++] = bf2;

            if (j == colSize) {
                threadNo++;
                j = 0;
            }

            if (threadNo == mValue) {
                threadNo = 0;
                j = 0;
                i++;
            }
        }
        if (charCount < (size*size)) {
            fprintf(stderr, "Wrong input file\n");
            exit(EXIT_FAILURE);
        }
    } else {
        int lessSize = size / mValue;
        int highSize = size - (lessSize * (mValue - 1));
        int colCount = 0;
        int threadNo = 0;

        for (int i = 0; i < mValue; i++) {
            block.bMat[i] = (int **)malloc(size * sizeof(int *));
            if (colCount < mValue - 1) {
                colCount++;
                block.colArr[i] = lessSize;
                for (int j = 0; j < size; j++) {
                    block.bMat[i][j] = (int *)malloc(lessSize * sizeof(int));
                }
            } else {
                block.colArr[i] = highSize;
                for (int j = 0; j < size; j++) {
                    block.bMat[i][j] = (int *)malloc(highSize * sizeof(int));
                }
            }
        }
        
        threadNo = i = j = charCount = 0;
        colCount = 0;
        while (read(fdInput2, &bf2, 1) == 1) {
            if (++charCount > (size*size)) {
                break;
            }
            block.bMat[threadNo][i][j++] = bf2;

            if (colCount < (mValue - 1) && j == lessSize) {
                colCount++;
                threadNo++;
                j = 0;
            } else if(j == highSize){
                threadNo++;
                j = 0;
            }

            if (threadNo == mValue) {
                threadNo = 0;
                colCount = 0;
                j = 0;
                i++;
            }
        }
        if (charCount < (size*size)) {
            perror("Hatalisin gardas");
            exit(EXIT_FAILURE);
        }
    }
    fprintf(stdout, "[%d] Two matrices of size %dx%d have been read. The number of threads is %d\n", (int)time(NULL), size, size, mValue);
}

void writeFile() {
    char buffer[1024];
    double timeDifference = 0.0;
    struct timeval start, end;
    gettimeofday(&start, NULL);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            memset(buffer, '\0', sizeof(buffer));
            if (j == size - 1) {
                sprintf(buffer, "%.3lf  %.3lei\n", block.real[i][j], block.imag[i][j]);
            } else {
                sprintf(buffer, "%.3lf  %.3lei,", block.real[i][j], block.imag[i][j]);
            }
            write(fdOutput, buffer, sizeof(buffer));
        }
    }
    gettimeofday(&end, NULL);
    timeDifference = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;
    totalTime += timeDifference;
    fprintf(stdout, "[%d] The process has written the output file. The total time spent is %lf seconds.\n", (int)time(NULL), totalTime);
}

void *operations(void *data) {
    int id = *(int*)data;
    int colSize = block.colArr[id];
    int cMat[size][colSize];
    double timeDifference = 0.0;
    struct timeval start, end;

    pthread_mutex_lock(&mutex);
    if (++arrived < mValue) {
        pthread_cond_wait(&condVar, &mutex);
    } else {
        pthread_cond_broadcast(&condVar);
    }

    gettimeofday(&start, NULL);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < colSize; j++) {
            cMat[i][j] = 0;
            for (int k = 0; k < size; k++) {
                cMat[i][j] += block.aMat[i][k] * block.bMat[id][k][j];
            }
        }
    }

    int index = 0;
    if (id == mValue - 1) {
        index = block.colArr[0] * id;
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < colSize; j++) {
                block.cMat[i][index] = cMat[i][j];  
                index++;   
            }
            index = block.colArr[0] * id;
            
        }
    } else {
        index = id * colSize;
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < colSize; j++) {
                block.cMat[i][index] = cMat[i][j];   
                index++;  
            }
            index = id * colSize;
        }
    }


    gettimeofday(&end, NULL);

    timeDifference = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;
    totalTime += timeDifference;
    fprintf(stdout, "[%d] Thread %d has reached the rendezvous point in %lf seconds.\n", (int)time(NULL), id+1, timeDifference);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    if (++arrived2 < mValue) {
        pthread_cond_wait(&condVar, &mutex);
    } else {
        pthread_cond_broadcast(&condVar);
    }

    fprintf(stdout, "[%d] Thread %d is advancing to the second part\n", (int)time(NULL), id+1);

    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);

    if (++arrived3 < mValue) {
        pthread_cond_wait(&condVar, &mutex);
    } else {
        pthread_cond_broadcast(&condVar);
    }

    gettimeofday(&start, NULL);
    dft(id, colSize);
    gettimeofday(&end, NULL);
    timeDifference = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;
    totalTime += timeDifference;
    fprintf(stdout, "[%d] Thread %d has has finished the second part in %lf seconds.\n", (int)time(NULL), id+1, timeDifference);
    pthread_mutex_unlock(&mutex);
    
    return NULL;
}

void dft(int id, int col) {
    int index = 0;
    if (id == mValue - 1) {
        index = block.colArr[0] * id;
    } else {
        index = id * col;
    }

    for (int y = 0; y < size; y++) {
        for (int x = index; x < index + col; x++) {
            double v1 = 0, v2 = 0; 
            for (int y1 = 0; y1 < size; y1++) {
                for (int x1 = 0; x1 < size; x1++) {
                    double xIndex = -2.0 * M_PI * y * y1 / (double)size;
                    double yIndex = -2.0 * M_PI * x * x1 / (double)size;
                    v1 += (block.cMat[y1][x1] * cos(xIndex+yIndex));
                    v2 += (block.cMat[y1][x1] * 1.0 * sin(xIndex+yIndex));
                }
            }
            block.real[y][x] = v1;
            block.imag[y][x] = v2; 
        }  
    }
}

void closeResources() {
    if (block.aMat != NULL) {
        for (int i = 0; i < size; i++) {
            free(block.aMat[i]);
        }
        free(block.aMat);
    }

    if (block.bMat != NULL) {
        for (int i = 0; i < mValue; i++) {
            for (int j = 0; j < size; j++) {
                free(block.bMat[i][j]);
            }
            free(block.bMat[i]);
        }
        free(block.bMat);
    }
    

    if (block.cMat != NULL) {
        for (int i = 0; i < size; i++) {
            free(block.cMat[i]);
        }
        free(block.cMat);
    }

    if (block.colArr != NULL) {
       free(block.colArr);
    }

    if (block.real != NULL) {
        for (int i = 0; i < size; i++) {
            free(block.real[i]);
        }
        free(block.real);
    }

    if (block.imag != NULL) {
        for (int i = 0; i < size; i++) {
            free(block.imag[i]);
        }
        free(block.imag);
    }
    
    if (threads != NULL) {
        free(threads);
    }
    
    close(fdInput1);
    close(fdInput2);
    close(fdOutput);
}
