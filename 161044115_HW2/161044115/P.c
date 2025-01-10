#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>

int PROCESS_NUM = 0;
char* inputFileName; 
char* outputFileName;
int input_fd;
FILE* output_fd;
int *childPID = NULL;
char* line = NULL;

void
SIGINThandler(int sigNo) {
    char errMsg[] = "\nSomeone pressed CTRL+C so the program terminates.\n";
    write(fileno(stdout), errMsg, strlen(errMsg));
    
    fclose(output_fd);
    remove(outputFileName);
    close(input_fd);
    if (childPID != NULL) {
        for (size_t i = 0; i < PROCESS_NUM; i++)
            kill(childPID[i], SIGINT);
        free(childPID);
    }
    if (line != NULL) 
        free(line);
    _exit(EXIT_SUCCESS);
}

double findFrobenius(double arr[], int arrLen);
void findClosest(double arr[]);

int 
main(int argc, char *const argv[]) {
    int status;
    if (argc != 5) {
        perror("USAGE: ./P -i inputFilePath -o outputFilePath");
        return -1;
    }

    char envVariable[30];
    int option;
    while ((option = getopt(argc, argv, "i:o:")) != -1) {
        switch (option) {
        case 'i':
            inputFileName = optarg;
            break;
        case 'o':
            outputFileName = optarg;
            break;
        default:
            break;
        }
    }

    input_fd = open(inputFileName, O_RDONLY);
    if (input_fd == -1) {
        perror("Failed to open input file");
        return 1;
    }

    output_fd = fopen(outputFileName, "wb+");
    if (output_fd == NULL) {
        perror("Failed to open input file");
        return 1;
    }


    struct sigaction sac;
    sac.sa_flags = 0;
    sac.sa_handler = SIGINThandler;

    if(sigemptyset(&sac.sa_mask) == -1 || sigaction(SIGINT, &sac, NULL) == -1) {
        perror("Sigaction error");
        return -1;
    }

    fprintf(stderr, "Process P reading %s\n", inputFileName);
    
    unsigned char ch;
    int temp = 0;
    while(read(input_fd, &ch, sizeof(unsigned char)) != '\0') {
        if (ch == '\n') {
            continue;
        }
        if (temp == 29) {
            PROCESS_NUM++;
            temp = 0;
        } else {
            temp++;
        }
    }

    int count = 0;
    int childIndex = 0;
    childPID = (int*) malloc(PROCESS_NUM * sizeof(int));
    
    fflush(stdout);
    if (lseek(input_fd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return -1;
    }
    
    while (read(input_fd, &ch, sizeof(unsigned char)) != '\0') {
        if (ch == '\n') {
            continue;
        }
        envVariable[count] = ch;
        if (count != 0 && count % 29 == 0) {
            char envVar[41] = "COORDINATE=";
            strcat(envVar, envVariable);
            char *env[] = {envVar};
            childIndex++;
            char str[10];
            sprintf(str, "%d", childIndex);
            char *args[] = {"./R", "-o", outputFileName, "-n", str, NULL};
            switch (childPID[childIndex-1] = fork()) {
                case 0:
                    execve("./R", args, env);
                    fprintf (stderr, "An error occurred in execvp\n");
                    _exit(EXIT_FAILURE);
                    break;
                case -1:
                    break;
                default:
                    waitpid(childPID[childIndex-1], &status, 0);
                    break;
            }
            
        }
        if (count == 29) {
            count = 0;
        } else {
            count++;
        }
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();

    if (fcntl(fileno(output_fd), F_SETLKW, &lock) == -1) {
        perror("Lock Error");
        _exit(EXIT_FAILURE);
    }

    fflush(stdout);
    fprintf(stderr, "Reached EOF, collecting outputs from %s\n", outputFileName);
    rewind(output_fd);
    ssize_t read;
    size_t len = 0;
    int lineCount = 0;
    double childFrobNorm[PROCESS_NUM];

    while ((read = getline(&line, &len, output_fd)) != -1) {
        double matrix[9];
        sscanf(line, "%lf %lf %lf %lf %lf %lf %lf %lf %lf",
        &matrix[0], &matrix[1], &matrix[2],
        &matrix[3], &matrix[4], &matrix[5],
        &matrix[6], &matrix[7], &matrix[8]);
        childFrobNorm[lineCount] = findFrobenius(matrix, 9);
        lineCount++;
    }

    findClosest(childFrobNorm);

    lock.l_type = F_UNLCK;
    if (fcntl(fileno(output_fd), F_SETLKW, &lock) == -1) {
        perror("Unlock Error");
        _exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < PROCESS_NUM; i++)
        kill(childPID[i], SIGINT);

    free(childPID);
    close(input_fd);
    fclose(output_fd);
    free(line);

    return 0;
}

double
findFrobenius(double arr[], int arrLen) {
    double sum = 0;
    for (size_t i = 0; i < arrLen; i++) {
        sum += (pow(arr[i], 2));
    }
    return sqrt(sum);
}

void
findClosest(double arr[]) {
    double min = 99999999999;
    int closestProcess[2];

    for (size_t i = 0; i < PROCESS_NUM - 1; i++) {
        for (size_t j = i + 1; j < PROCESS_NUM; j++) {
            double distance = arr[i] - arr[j];
            if (distance < 0)
                distance *= -1;
            
            if (distance < min) {
                closestProcess[0] = i;
                closestProcess[1] = j;
                min = distance;
            }
        }
    }

    fprintf(stderr, "The closest 2 matrices are ");
    rewind(output_fd);
    ssize_t read;
    size_t len = 0;
    int lineCount = 0;
    char* tempLine;

    while ((read = getline(&tempLine, &len, output_fd)) != -1) {
        if (lineCount == closestProcess[0]) {
            tempLine[strlen(tempLine) - 1] = '\0';
            fprintf(stderr, "%sand ", tempLine);
        }
        
        if (lineCount == closestProcess[1]) {
            tempLine[strlen(tempLine) - 1] = '\0';
            fprintf(stderr, "%s", tempLine);
            break;
        }
        lineCount++;
    }

    fprintf(stderr, "and their distance is %.4lf\n", min);
    free(tempLine);
}