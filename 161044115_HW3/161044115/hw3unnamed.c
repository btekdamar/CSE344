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
    sem_t semGullac,
          semWS, 
          semFW, 
          semSF, 
          semMF, 
          semMW, 
          semSM;
    sem_t semOrganizer;
    int dessertNum;
    int controlFlag;
    char sharedArr[2];
} SharedBlock;

typedef struct {
    enum Ing ing1;
    enum Ing ing2;
    int id;
    int chefDessert;
    pid_t pid;
    sem_t *semChef;
} Chef;

const char *MEM_NAME = "/shmING";
char *inputFile;
int fd;
pid_t childPid[CHEF_NUM + 1];
Chef chefs[CHEF_NUM];
SharedBlock *sems;

int runChef(Chef chefInfo);
int initChef();
void initPusher();
void runTheWholeSaler();
enum Ing getIng(char ch);

void
SIGINThandler(int sigNo) {
    sem_destroy(&sems->semFW);
    sem_destroy(&sems->semMF);
    sem_destroy(&sems->semSF);
    sem_destroy(&sems->semMW);
    sem_destroy(&sems->semSM);
    sem_destroy(&sems->semWS);
    sem_destroy(&sems->semGullac);
    munmap(sems, sizeof(sems));
    shm_unlink(MEM_NAME);
    close(fd);
    for (int i = 0; i < CHEF_NUM + 1; i++) {
        kill(childPid[i], SIGINT);
    }

    exit(EXIT_SUCCESS);
}

int main(int argc, char *const argv[]) {
    int opt;

    if (argc != 3) {
        fprintf(stderr, "USAGE: ./hw3unnamed -i inputFilePath\n");
        return -1;
    }
    
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
        case 'i':
            inputFile = optarg;
            break;
        default:
            fprintf(stderr, "USAGE: ./hw3unnamed -i inputFilePath\n");
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

    int shm_fd = shm_open(MEM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    ftruncate(shm_fd, sizeof(SharedBlock));
    sems = mmap(NULL, sizeof(SharedBlock), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (sem_init(&sems->semOrganizer, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semWS, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semFW, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semSF, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semMF, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semMW, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semSM, 1, 0) == -1) { perror("sem_init error"); return -1;}
    if (sem_init(&sems->semGullac, 1, 0) == -1) { perror("sem_init error"); return -1;}
    sems->dessertNum = 0;
    sems->controlFlag = FALSE;

    fd = open(inputFile, O_RDONLY);
    if (fd == -1) {
        perror("file error");
        return -1;
    }

    int result = 0;
    result = initChef();

    if (sems->controlFlag == FALSE) {
        sems->controlFlag = TRUE;
        initPusher();
        runTheWholeSaler();
        return 0;
    }

    sem_destroy(&sems->semFW);
    sem_destroy(&sems->semMF);
    sem_destroy(&sems->semSF);
    sem_destroy(&sems->semMW);
    sem_destroy(&sems->semSM);
    sem_destroy(&sems->semWS);
    sem_destroy(&sems->semGullac);
    munmap(sems, sizeof(sems));
    shm_unlink(MEM_NAME);
    close(fd);
    return result;
}

void runTheWholeSaler() {
    enum Ing ing1, ing2;
    char buf[3];
    while (read(fd, buf, 3)) {
        ing1 = getIng(buf[0]);
        ing2 = getIng(buf[1]);
        fprintf(stdout, "the wholesaler (pid %d) delivers %s and %s\n", getpid(), name[ing1], name[ing2]);
        sems->sharedArr[0] = buf[0];
        sems->sharedArr[1] = buf[1];
        sem_post(&sems->semOrganizer);
        fprintf(stdout, "the wholesaler (pid %d) is waiting for the dessert\n", getpid());
        sem_wait(&sems->semGullac);
        fprintf(stdout, "the wholesaler (pid %d) has obtained the dessert and left\n", getpid());
    }
    sems->sharedArr[0] = 'N';
    sems->sharedArr[1] = 'N';
    sem_post(&sems->semOrganizer);
    int status;
    for (int i = 0; i < CHEF_NUM + 1; i++) {
        if (waitpid(childPid[i], &status, 0) < 0) {
            perror("Wait error");
            exit(EXIT_FAILURE);
        }
        sems->dessertNum += WEXITSTATUS(status);
    }

    fprintf(stdout, "the wholesaler (pid %d) is done (total deserts: %d)\n", getpid(), sems->dessertNum);
}


int runChef(Chef chefInfo) {
    pid_t pid = chefInfo.pid;
    int id = chefInfo.id;
    enum Ing ing1 = chefInfo.ing1;
    enum Ing ing2 = chefInfo.ing2;
    fprintf(stdout, "chef%d (pid %d) is waiting for %s and %s\n", id, pid, name[ing1], name[ing2]);
    while (1) {
        sem_wait(chefInfo.semChef);
        if (sems->sharedArr[0] == 'N' && sems->sharedArr[1] == 'N') {
            fprintf(stdout, "chef%d (pid %d) is exiting\n", id, pid);
            return chefInfo.chefDessert;
        }
        fprintf(stdout, "chef%d (pid %d) has taken the %s [%c%c]\n", id, pid, name[ing1], sems->sharedArr[0], sems->sharedArr[1]);
        fprintf(stdout, "chef%d (pid %d) has taken the %s [%c%c]\n", id, pid, name[ing2], sems->sharedArr[0], sems->sharedArr[1]);
        fprintf(stdout, "chef%d (pid %d) is preparing the dessert [%c%c]\n", id, pid, sems->sharedArr[0], sems->sharedArr[1]);
        chefInfo.chefDessert++;
        fprintf(stdout, "chef%d (pid %d) has delivered the dessert [%c%c]\n", id, pid, sems->sharedArr[0], sems->sharedArr[1]);
        sem_post(&sems->semGullac);
        fprintf(stdout, "chef%d (pid %d) is waiting for %s and %s [%c%c]\n", id, pid, name[ing1], name[ing2], sems->sharedArr[0], sems->sharedArr[1]);
    }
}

int initChef() {
    int i = 0;
    while (i < CHEF_NUM) {
        switch (i) {
        case 0:
            chefs[i].id = i;
            chefs[i].ing1 = WALNUTS;
            chefs[i].ing2 = SUGAR;
            chefs[i].semChef = &sems->semWS;
            chefs[i].chefDessert = 0;
            break;
        case 1:
            chefs[i].id = i;
            chefs[i].ing1 = FLOUR;
            chefs[i].ing2 = WALNUTS;
            chefs[i].semChef = &sems->semFW;
            chefs[i].chefDessert = 0;
            break;
        case 2:
            chefs[i].id = i;
            chefs[i].ing1 = SUGAR;
            chefs[i].ing2 = FLOUR;
            chefs[i].semChef = &sems->semSF;
            chefs[i].chefDessert = 0;
            break;
        case 3:
            chefs[i].id = i;
            chefs[i].ing1 = MILK;
            chefs[i].ing2 = FLOUR;
            chefs[i].semChef = &sems->semMF;
            chefs[i].chefDessert = 0;
            break;
        case 4:
            chefs[i].id = i;
            chefs[i].ing1 = MILK;
            chefs[i].ing2 = WALNUTS;
            chefs[i].semChef = &sems->semMW;
            chefs[i].chefDessert = 0;
            break;
        case 5:
            chefs[i].id = i;
            chefs[i].ing1 = SUGAR;
            chefs[i].ing2 = MILK;
            chefs[i].semChef = &sems->semSM;
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
            sem_wait(&sems->semOrganizer);
            if ((sems->sharedArr[0] == 'W' && sems->sharedArr[1] == 'S') || (sems->sharedArr[1] == 'W' && sems->sharedArr[0] == 'S'))
                sem_post(&sems->semWS);
            else if ((sems->sharedArr[0] == 'F' && sems->sharedArr[1] == 'W') || (sems->sharedArr[1] == 'F' && sems->sharedArr[0] == 'W'))
                sem_post(&sems->semFW);
            else if ((sems->sharedArr[0] == 'S' && sems->sharedArr[1] == 'F') || (sems->sharedArr[1] == 'S' && sems->sharedArr[0] == 'F'))
                sem_post(&sems->semSF);
            else if ((sems->sharedArr[0] == 'M' && sems->sharedArr[1] == 'F') || (sems->sharedArr[1] == 'M' && sems->sharedArr[0] == 'F'))
                sem_post(&sems->semMF);
            else if ((sems->sharedArr[0] == 'M' && sems->sharedArr[1] == 'W') || (sems->sharedArr[1] == 'M' && sems->sharedArr[0] == 'W')) 
                sem_post(&sems->semMW);
            else if ((sems->sharedArr[0] == 'S' && sems->sharedArr[1] == 'M') || (sems->sharedArr[1] == 'S' && sems->sharedArr[0] == 'M'))
                sem_post(&sems->semSM);
            else {
                sem_post(&sems->semWS);
                sem_post(&sems->semFW);
                sem_post(&sems->semSF);
                sem_post(&sems->semMF);
                sem_post(&sems->semMW);
                sem_post(&sems->semSM);
                break;
            }
        }
        _exit(EXIT_SUCCESS);
    }
}