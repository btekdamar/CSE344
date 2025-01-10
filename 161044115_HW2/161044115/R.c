#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>

extern char** __environ;

int fd;
char const ENV_NAME[] = "COORDINATE";
int const COOR_NUM = 10;

void
SIGINThandler(int sigNo) {
    close(fd);
    _exit(EXIT_SUCCESS);
}

void findCovMatrix(int x[], int y[], int z[]);
double findVar(int arr[], double avg);
double findCov(int arr1[], int arr2[], double avg1, double avg2);
int findSum(int arr[]);

int 
main(int argc, char *const argv[]) {
    char* outputFileName;
    int index;
    int option;
    int X[COOR_NUM];
    int Y[COOR_NUM];
    int Z[COOR_NUM];
    int xIndex = 0, yIndex = 0, zIndex = 0;
    while ((option = getopt(argc, argv, "o:n:")) != -1) {
        switch (option) {
        case 'o':
            outputFileName = optarg;
            break;
        case 'n':
            index = atoi(optarg);
            break;
        default:
            break;
        }
    }

    fd = open(outputFileName, O_WRONLY);
    if (fd == -1) {
        perror("Open Error");
        _exit(EXIT_FAILURE);
    }

    struct sigaction sac;
    sac.sa_flags = 0;
    sac.sa_handler = SIGINThandler;

    if(sigemptyset(&sac.sa_mask) == -1 || sigaction(SIGINT, &sac, NULL) == -1) {
        perror("Sigaction error");
        close(fd);
        return -1;
    }
    

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Lock Error");
        _exit(EXIT_FAILURE);
    }
    
    lseek(fd, 0L, SEEK_END);
    fflush(stdout);
    char* my_env_var = getenv(ENV_NAME);
    fprintf(stderr, "Created R_%d: (", index);
    for (int i = 0; i < 30; i++) {
        if (i == 29) {
            Z[zIndex] = (int) my_env_var[i];
            fprintf(stderr, "%d)\n", my_env_var[i]);
        } else if((i+1) % 3 == 1) {
            fprintf(stderr, "%d, ", my_env_var[i]);
            X[xIndex++] = (int) my_env_var[i];
        } else if ((i + 1) % 3 == 0) {
            fprintf(stderr, "%d),(", my_env_var[i]);
            Z[zIndex++] = (int) my_env_var[i];
        } else {
            fprintf(stderr, "%d, ", my_env_var[i]);
            Y[yIndex++] = (int) my_env_var[i];
        }
    }

    findCovMatrix(X, Y, Z);
    
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Unlock Error");
        _exit(EXIT_FAILURE);
    }
    
    close(fd);
    return 0;
}

void
findCovMatrix(int x[], int y[], int z[]) {
    int sumX = findSum(x);
    int avgX = sumX / COOR_NUM;
    int sumY = findSum(y);
    int avgY = sumY / COOR_NUM;
    int sumZ = findSum(z);
    int avgZ = sumZ / COOR_NUM;
    double covMatrix[3][3];
    covMatrix[0][0] = findVar(x, avgX);
    covMatrix[1][1] = findVar(y, avgY);
    covMatrix[2][2] = findVar(z, avgZ);
    covMatrix[0][1] = covMatrix[1][0] = findCov(x, y, avgX, avgY);
    covMatrix[1][2] = covMatrix[2][1] = findCov(y, z, avgY, avgZ);
    covMatrix[0][2] = covMatrix[2][0] = findCov(x, z, avgX, avgZ);
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            char buff[32];
            int bum = sprintf(buff, "%lf ", covMatrix[i][j]);
            write(fd, buff, bum);
        }
    }

    write(fd, "\n", 1);
}

double
findVar(int arr[], double avg) {
    double sum = 0;
    for (size_t i = 0; i < COOR_NUM; i++)
        sum += pow((arr[i] - avg), 2);
    return (sum/COOR_NUM);
}

double
findCov(int arr1[], int arr2[], double avg1, double avg2) {
    double sum = 0;
    for (size_t i = 0; i < COOR_NUM; i++)
        sum += ((arr1[i] - avg1) * (arr2[i] - avg2));
    return (sum/COOR_NUM);
}

int
findSum(int arr[]) {
    int sum = 0;
    for (size_t i = 0; i < COOR_NUM; i++)
        sum += arr[i];
    return sum;
}