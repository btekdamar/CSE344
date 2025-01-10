#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define FALSE 0
#define TRUE 1
#define CHEF_NUM 6
#define ING_NUM 4

enum Ing{MILK, FLOUR, WALNUTS, SUGAR};
static char *name[ING_NUM] = {"Milk", "Flour", "Walnuts", "Sugar"};

typedef struct {
    int id;
    pid_t pid;
    enum Ing ing1;
    enum Ing ing2;
    int chefDessert;
    sem_t *semChef;
} Chef;

sem_t *semGullac,
      *semWS, 
      *semFW, 
      *semSF, 
      *semMF, 
      *semMW, 
      *semSM;
sem_t *semOrganizer;
char *sharedArr;
char *inputFile;
char *semName;
char *chefsSemName;
char *wholeSalerSemName;
static int *controlFlag = FALSE;

int fd;
int shmid;
pid_t childPid[CHEF_NUM + 1];
Chef chefs[CHEF_NUM];

int runChef(Chef chefInfo);
int initChef();
void initPusher();
void runTheWholeSaler();
void closeResources();
enum Ing getIng(char ch);

void
SIGINThandler(int sigNo) {
    closeResources();
    _exit(EXIT_SUCCESS);
}

int main(int argc, char *const argv[]) {
    int opt;

    if (argc != 7) {
        fprintf(stdout, "USAGE: ./hw3named -i inputFilePath -n chefsname -w wholesalername");
        return -1;
    }
    
    while ((opt = getopt(argc, argv, "i:n:w:")) != -1) {
        switch (opt) {
        case 'i':
            inputFile = optarg;
            break;
        case 'n':
            chefsSemName = optarg;
            break;
        case 'w':
            wholeSalerSemName = optarg;
            break;
        default:
            fprintf(stdout, "USAGE: ./hw3named -i inputFilePath -n chefsname -w wholesalername\n");
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

    controlFlag = mmap(NULL, sizeof *controlFlag, PROT_READ | PROT_WRITE, 
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    shmid = shmget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    sharedArr = (char*) shmat(shmid, NULL, 0);
    semGullac = sem_open(wholeSalerSemName, O_CREAT | O_EXCL, 0644, 0);
    semOrganizer = sem_open("/semOrganizer", O_CREAT | O_EXCL, 0644, 0);
    sem_unlink(wholeSalerSemName);
    sem_unlink("/semOrganizer");


    fd = open(inputFile, O_RDONLY);
    if (fd == -1) {
        perror("file error");
        return -1;
    }

    int result = 0;
    result = initChef();
    if (*controlFlag == FALSE) {
        *controlFlag = TRUE;
        initPusher();
        runTheWholeSaler();
        closeResources();
        return 0;
    }

    closeResources();
    return result;
}

void runTheWholeSaler() {
    enum Ing ing1, ing2;
    char buf[3];
    int dessertNum = 0;
    while (read(fd, buf, 3)) {
        ing1 = getIng(buf[0]);
        ing2 = getIng(buf[1]);
        fprintf(stdout, "the wholesaler (pid %d) delivers %s and %s\n", getpid(), name[ing1], name[ing2]);
        sharedArr[0] = buf[0];
        sharedArr[1] = buf[1];
        sem_post(semOrganizer);
        fprintf(stdout, "the wholesaler (pid %d) is waiting for the dessert\n", getpid());
        sem_wait(semGullac);
        fprintf(stdout, "the wholesaler (pid %d) has obtained the dessert and left\n", getpid());
        
    }
    sharedArr[0] = 'N';
    sharedArr[1] = 'N';
    sem_post(semOrganizer);
    int status;
    for (int i = 0; i < CHEF_NUM + 1; i++) {
        if (waitpid(childPid[i], &status, 0) < 0) {
            perror("Wait error");
            exit(EXIT_FAILURE);
        }
        dessertNum += WEXITSTATUS(status);
    }

    fprintf(stdout, "the wholesaler (pid %d) is done (total deserts: %d)\n", getpid(), dessertNum);
}


int runChef(Chef chefInfo) {
    pid_t pid = chefInfo.pid;
    int id = chefInfo.id;
    enum Ing ing1 = chefInfo.ing1;
    enum Ing ing2 = chefInfo.ing2;
    fprintf(stdout, "chef%d (pid %d) is waiting for %s and %s\n", id, pid, name[ing1], name[ing2]);
    while (1) {
        sem_wait(chefInfo.semChef);
        if (sharedArr[0] == 'N' && sharedArr[1] == 'N') {
            fprintf(stdout, "chef%d (pid %d) is exiting\n", id, pid);
            return chefInfo.chefDessert;
        }
        fprintf(stdout, "chef%d (pid %d) has taken the %s [%c%c]\n", id, pid, name[ing1], sharedArr[0], sharedArr[1]);
        fprintf(stdout, "chef%d (pid %d) has taken the %s [%c%c]\n", id, pid, name[ing2], sharedArr[0], sharedArr[1]);
        fprintf(stdout, "chef%d (pid %d) is preparing the dessert [%c%c]\n", id, pid, sharedArr[0], sharedArr[1]);
        chefInfo.chefDessert++;
        fprintf(stdout, "chef%d (pid %d) has delivered the dessert [%c%c]\n", id, pid, sharedArr[0], sharedArr[1]);
        sem_post(semGullac);
        fprintf(stdout, "chef%d (pid %d) is waiting for %s and %s [%c%c]\n", id, pid, name[ing1], name[ing2], sharedArr[0], sharedArr[1]);
    }
}

int initChef() {
    int i = 0;
    char semName[strlen(chefsSemName + 1)];
    while (i < CHEF_NUM) {
        switch (i) {
        case 0:
            chefs[i].id = i;
            chefs[i].ing1 = WALNUTS;
            chefs[i].ing2 = SUGAR;
            sprintf(semName, "%s%d", chefsSemName, i);
            semWS = sem_open(semName, O_CREAT | O_EXCL, 0644, 0);
            sem_unlink(semName);
            chefs[i].semChef = semWS;
            chefs[i].chefDessert = 0;
            break;
        case 1:
            chefs[i].id = i;
            chefs[i].ing1 = FLOUR;
            chefs[i].ing2 = WALNUTS;
            sprintf(semName, "%s%d", chefsSemName, i);
            semFW = sem_open(semName, O_CREAT | O_EXCL, 0644, 0);
            sem_unlink(semName);
            chefs[i].semChef = semFW;
            chefs[i].chefDessert = 0;
            break;
        case 2:
            chefs[i].id = i;
            chefs[i].ing1 = SUGAR;
            chefs[i].ing2 = FLOUR;
            sprintf(semName, "%s%d", chefsSemName, i);
            semSF = sem_open(semName, O_CREAT | O_EXCL, 0644, 0);
            sem_unlink(semName);
            chefs[i].semChef = semSF;
            chefs[i].chefDessert = 0;
            break;
        case 3:
            chefs[i].id = i;
            chefs[i].ing1 = MILK;
            chefs[i].ing2 = FLOUR;
            sprintf(semName, "%s%d", chefsSemName, i);
            semMF = sem_open(semName, O_CREAT | O_EXCL, 0644, 0);
            sem_unlink(semName);
            chefs[i].semChef = semMF;
            chefs[i].chefDessert = 0;
            break;
        case 4:
            chefs[i].id = i;
            chefs[i].ing1 = MILK;
            chefs[i].ing2 = WALNUTS;
            sprintf(semName, "%s%d", chefsSemName, i);
            semMW = sem_open(semName, O_CREAT | O_EXCL, 0644, 0);
            sem_unlink(semName);
            chefs[i].semChef = semMW;
            chefs[i].chefDessert = 0;
            break;
        case 5:
            chefs[i].id = i;
            chefs[i].ing1 = SUGAR;
            chefs[i].ing2 = MILK;
            sprintf(semName, "%s%d", chefsSemName, i);
            semSM = sem_open(semName, O_CREAT | O_EXCL, 0644, 0);
            sem_unlink(semName);
            chefs[i].semChef = semSM;
            chefs[i].chefDessert = 0;
            break;
        default:
            break;
        }
        if (fork() == 0) {
            chefs[i].pid = getpid();
            childPid[i] = chefs[i].pid;
            int result = runChef(chefs[i]);
            return result;
        }
        i++;
    }
    return 0;
}

enum Ing getIng(char ch) {
    enum Ing ing;
    switch (ch) {
    case 'M':
        ing = MILK;
        break;
    case 'F':
        ing = FLOUR;
        break;
    case 'W':
        ing = WALNUTS;
        break;
    case 'S':
        ing = SUGAR;
        break;
    default:
        perror("Ingredient Error");
        exit(EXIT_FAILURE);
        break;
    }
    return ing;
}

void initPusher() {
    if (fork() == 0) {
        childPid[CHEF_NUM] = getpid();
        while(1) {
            sem_wait(semOrganizer);
            if ((sharedArr[0] == 'W' && sharedArr[1] == 'S') || (sharedArr[1] == 'W' && sharedArr[0] == 'S'))
                sem_post(semWS);
            else if ((sharedArr[0] == 'F' && sharedArr[1] == 'W') || (sharedArr[1] == 'F' && sharedArr[0] == 'W'))
                sem_post(semFW);
            else if ((sharedArr[0] == 'S' && sharedArr[1] == 'F') || (sharedArr[1] == 'S' && sharedArr[0] == 'F'))
                sem_post(semSF);
            else if ((sharedArr[0] == 'M' && sharedArr[1] == 'F') || (sharedArr[1] == 'M' && sharedArr[0] == 'F'))
                sem_post(semMF);
            else if ((sharedArr[0] == 'M' && sharedArr[1] == 'W') || (sharedArr[1] == 'M' && sharedArr[0] == 'W')) 
                sem_post(semMW);
            else if ((sharedArr[0] == 'S' && sharedArr[1] == 'M') || (sharedArr[1] == 'S' && sharedArr[0] == 'M'))
                sem_post(semSM);
            else {
                sem_post(semWS);
                sem_post(semFW);
                sem_post(semSF);
                sem_post(semMF);
                sem_post(semMW);
                sem_post(semSM);
                break;
            }
        }
        closeResources();
        _exit(EXIT_SUCCESS);
    }
}

void closeResources() {
    sem_close(semFW);
    sem_close(semMF);
    sem_close(semSF);
    sem_close(semMW);
    sem_close(semSM);
    sem_close(semWS);
    sem_close(semGullac);
    sem_close(semOrganizer);
    shmdt(sharedArr);
    shmctl(shmid, IPC_RMID, 0);
    munmap(controlFlag, sizeof *controlFlag);
    close(fd);
}