#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

struct arguments{
    char* str1;
    char* str2;
    char* str3;
    char* str4;
    int isCaseSensSingle;
    int isCaseSensMulti;
};

const char* path;
char temp_filename[] = "/tmp/temp_file.XXXXXX";

int isArgumentsValid(const char* args);
int parseArguments(struct arguments* argList, const char* info);
int changeFile(int* fd, int fdTemp, struct arguments* argList);
void parsePattern(int fd, char* str1, char* str2, char* buffer, int isCaseSensitive);
void swapStrings(char* buffer, char* fromString, char* toString);
void writeFile(int fd, char* buffer);


int main(int argc, char const *argv[]) {
    int fd;
    struct flock lock;
    struct arguments argInfo;
    int fdTemp = mkstemp(temp_filename);
    unlink(temp_filename);
    memset(&argInfo, 0, sizeof(argInfo));

    if (argc != 3) {
        perror("Wrong input");
        return 1;
    }

    const char* strings = argv[1];

    if (isArgumentsValid(strings) == -1){
        return 1;
    }

    path = argv[2];

    fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("Failed to open input file");
        return 1;
    }

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    if ((fcntl(fd, F_SETLKW, &lock)) == -1) {
        perror("Failed to lock");
        return -1;
    }
    
    parseArguments(&argInfo, strings);
    changeFile(&fd, fdTemp, &argInfo);
  
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Failed to unlock");
        return -1;
    }

    free(argInfo.str1);
    free(argInfo.str2);
    free(argInfo.str3);
    free(argInfo.str4);
    close(fdTemp);
    close(fd);
    
    return 0;
}

int isArgumentsValid(const char* args) {
    int i = 0;
    int t = 0;
    while (i < strlen(args)) {
        if (i == 0 && args[i] != '/') {
            perror("./main '/str1/str2/' filename");
            return -1;
        }
        if (args[i] == '^' 
            && args[i-1] != '/' 
            && (args[i+1] != '[' 
            || (args[i+1] >= 65 && args[i+1] <= 90)
            || (args[i+1] >= 97 && args[i+1] <= 122)
            || (args[i+1] >= 48 && args[i+1] <= 57))) {
            perror("./main '/^str1/str2/' filename");
            return -1;
        } 
        if (args[i] == '$' && (args[i-1] == '/' || args[i+1] != '/')) {
            perror("./main '/str1$/str2/' filename");
            return -1;
        }
        if (args[i] == '['){
            int j = i + 1;
            while (args[j] != ']') {
                if (args[j] == '/') {
                    perror("./main '/s[tr]1/str2/' filename");
                    return -1;
                }
                j++;
            }
        }
        if(args[i] == '/') {
            t++;
        }

        if (args[i] == 'i' && t == 3 && args[i-1] != '/' && (args[i+1] != ';' || args[i+1] != '\0')) {
                perror("./main '/str1/str2/i' filename");
                perror("./main '/str1/str2/i;/str3/str4' filename");
                return -1;
        }
        if (args[i] == 'i' && ((t == 6 
            && args[i-1] != '/' && args[i+1] != '\0') 
            || (t == 5 && args[i-1] != '/' && args[i+1] != '\0'))) {
                perror("./main '/str1/str2/i;/str3/str4/i' filename");
                return -1;
        }
        i++; 
    }
    return 1;
}

int parseArguments(struct arguments* argList, const char* info) {
    size_t lenStr1 = 0;
    size_t lenStr2 = 0;
    size_t lenStr3 = 0;
    size_t lenStr4 = 0;
    int j = 0;
    int i = 1;
    argList->isCaseSensSingle = 0;
    argList->isCaseSensMulti = 0;
    while (i < strlen(info)){
        if (info[i] == '/') {
            j++;
            if (j == 2  && info[i+1] == 'i') {
                argList->isCaseSensSingle = 1;
                i += 3;
            }
            if (j == 4 && info[i+1] == 'i')
                argList->isCaseSensMulti = 1;
        } else {
            switch (j){
            case 0:
                lenStr1++;
                break;
            case 1:
                lenStr2++;
                break;
            case 2:
                lenStr3++;
                break;
            case 3:
                lenStr4++;
                break;
            default:
                break;
            }
        }
        i++; 
    }
    if (lenStr1 != 0){
        argList->str1 = (char*) malloc(lenStr1);
    }
    if (lenStr2 != 0){
        argList->str2 = (char*) malloc(lenStr2);
    }
    if (lenStr3 != 0){
        argList->str3 = (char*) malloc(lenStr3);
    } else {
        argList->str3 = NULL;
    }
    if (lenStr4 != 0){
        argList->str4 = (char*) malloc(lenStr4);
    } else {
        argList->str4 = NULL;
    }
    int index1 = 0, index2 = 0, index3 = 0, index4 = 0;
    i = 1;
    j = 0;
    while (i < strlen(info)){
        if (info[i] == '/') {
            j++;
            if (j == 2  && info[i+1] == 'i') {
                i += 3;
            }
        } else {
            switch (j){
            case 0:
                argList->str1[index1] = info[i];
                index1++;
                break;
            case 1:
                argList->str2[index2] = info[i];
                index2++;
                break;
            case 2:
                argList->str3[index3] = info[i];
                index3++;
                break;
            case 3:
                argList->str4[index4] = info[i];
                index4++;
                break;
            default:
                break;
            }
        }
        i++; 
    }
    return 1;
}

int changeFile(int* fd, int fdTemp, struct arguments* argList) {
    char* buffer1;
    char* buffer2;
    char* buffer3;
    char temp_filename[] = "/tmp/temp_file.XXXXXX";
    if(lseek (*fd, 0, SEEK_SET) == -1 
        && lseek(fdTemp, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return -1;
    }
    int characterSize = 0;
    char ch;
    while (read (*fd, &ch, 1) == 1) {
        characterSize++;
    }

    if(lseek (*fd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return -1;
    }

    buffer1 = (char*) malloc(characterSize); 
    read (*fd, buffer1, characterSize);
    
    parsePattern(fdTemp, argList->str1, argList->str2, buffer1, argList->isCaseSensSingle);
    free(buffer1);
    if (argList->str3 != NULL && argList->str4 != NULL) {
        if(lseek (fdTemp, 0, SEEK_SET) == -1) {
            perror("lseek error");
            return -1;
        }

        characterSize = 0;
        while (read (fdTemp, &ch, 1) == 1) {
            characterSize++;
        }

        if(lseek (fdTemp, 0, SEEK_SET) == -1) {
            perror("lseek error");
            return -1;
        }

        buffer2 = (char*) malloc(characterSize); 
        read (fdTemp, buffer2, characterSize);
        fdTemp = mkstemp(temp_filename);
        unlink(temp_filename);
        parsePattern(fdTemp, argList->str3, argList->str4, buffer2, argList->isCaseSensMulti);
        free(buffer2);
    }
    

    if(lseek (fdTemp, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return -1;
    }
    characterSize = 0;
    while (read (fdTemp, &ch, 1) == 1) {
        characterSize++;
    }

    if(lseek (fdTemp, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return -1;
    }

    buffer3 = (char*) malloc(characterSize); 
    read (fdTemp, buffer3, characterSize);

    *fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (*fd == -1) {
        perror("Failed to open input file");
        return -1;
    }

    if(lseek (*fd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return -1;
    }
    write(*fd, buffer3, characterSize);
    free(buffer3);
    return 1;
}

void parsePattern(int fd, char* str1, char* str2, char* buffer, int isCaseSensitive) {  
    int MAX_LENGTH = 1024 * 1024;
    int charGroupSize = 10;
    int i = 0;
    int countIndex;
    int firstControl = 0;
    int lastControl = 0;
    int control = 0;

    size_t lenStr1 = strlen(str1);
    size_t lenStr2 = strlen(str2);

    char tempBuffer[MAX_LENGTH];
    strcpy(tempBuffer, buffer);

    if (str1[0] == '^'){
        firstControl = 1;
    }
    if (str1[lenStr1-1] == '$'){
        lastControl = 1;
    }

    while (i < strlen(tempBuffer)){
        int tempI = i;
        countIndex = 0;
        control = 0;
        int j = 0;
        if (firstControl == 1){
            j = 1;            
        }
        char bracketGroup[charGroupSize];
        while (j < lenStr1){
            char lowerBuffer = tempBuffer[tempI];
            char lowerStr = str1[j];

            if (lowerBuffer >= 65 && lowerBuffer <= 90) 
                lowerBuffer += 32;
            if (lowerStr >= 65 && lowerStr <= 90)
                lowerStr += 32;
                
            if (str1[j] == '[') {
                for (size_t index = 0; index < charGroupSize; index++)
                    bracketGroup[index] = '\0';
                int t = 0;
                int innerIndex = j+1;
                while(t < charGroupSize 
                        && str1[innerIndex] != '\0' 
                        && str1[innerIndex] != ']') { 
                    bracketGroup[t] = str1[innerIndex];
                    t++;
                    innerIndex++;
                }
                j = innerIndex;
                t = 0;
                while (t < charGroupSize){
                    if (isCaseSensitive == 0) {
                        char lowerBuffer = tempBuffer[tempI];
                        char lowerBracket = bracketGroup[t];
                        if (lowerBuffer >= 65 && lowerBuffer <= 90) 
                            lowerBuffer += 32;
                        if (lowerBracket >= 65 && lowerBracket <= 90)
                            lowerBracket += 32;
                        if (firstControl == 1 
                            && (lowerBuffer == lowerBracket) 
                            && (tempBuffer[tempI-1] == '\n' || tempI == 0)){
                            j++;
                            tempI++;
                            countIndex++;
                            control = 1;
                            break;
                        } else if(control == 1 
                                    && firstControl == 1 
                                    && (lowerBracket == lowerBuffer)) {
                            j++;
                            tempI++;
                            countIndex++;
                            break;
                        } else if (firstControl == 0 
                                    && (lowerBracket == lowerBuffer)){
                            countIndex++;
                            tempI++;
                            j++;
                            control = 1;
                            break;
                        }
                    } else {
                        if (firstControl == 1 
                            && (tempBuffer[tempI] == bracketGroup[t]) 
                            && (tempBuffer[tempI-1] == '\n' || tempI == 0)){
                            j++;
                            tempI++;
                            countIndex++;
                            control = 1;
                            break;
                        } else if(control == 1 
                                    && firstControl == 1 
                                    && (tempBuffer[tempI] == bracketGroup[t])) {
                            j++;
                            tempI++;
                            countIndex++;
                            break;
                        } else if (firstControl == 0 
                                    && tempBuffer[tempI] == bracketGroup[t]){
                            countIndex++;
                            tempI++;
                            j++;
                            control = 1;
                            break;
                        }
                    }
                    t++;
                }
                if (t == charGroupSize || control == 0){
                    break;
                }
            } else if(str1[j] == '*') {
                if (isCaseSensitive == 0) {
                    char lowerBuffer = tempBuffer[tempI];
                    char lowerStr = str1[j-1];
                    char lowerStr2 = str1[j+1];
                    if (lowerBuffer >= 65 && lowerBuffer <= 90) 
                        lowerBuffer += 32;
                    if (lowerStr >= 65 && lowerStr <= 90)
                        lowerStr += 32;
                    if (lowerStr2 >= 65 && lowerStr2 <= 90)
                        lowerStr2 += 32;
                    if (str1[j-1] == ']') {
                        int t = 0;
                        lowerStr = bracketGroup[t];
                        if (lowerStr >= 65 && lowerStr <= 90)
                            lowerStr += 32;
                        while(t < charGroupSize) {
                            if (lowerBuffer != tempBuffer[tempI]) {
                                if((tempBuffer[tempI] + 32) == lowerStr){
                                    while((tempBuffer[tempI] + 32) == lowerStr) {
                                        countIndex++;
                                        tempI++;
                                    }
                                    j++;
                                    break;
                                } else {
                                    t++;
                                    lowerStr = bracketGroup[t];
                                    if (lowerStr >= 65 && lowerStr <= 90)
                                        lowerStr += 32;
                                }
                            } else {
                                if(tempBuffer[tempI] == lowerStr){
                                    while(tempBuffer[tempI] == lowerStr) {
                                        countIndex++;
                                        tempI++;
                                    }
                                    j++;
                                    break;
                                } else {
                                    t++;
                                    lowerStr = bracketGroup[t];
                                    if (lowerStr >= 65 && lowerStr <= 90)
                                        lowerStr += 32;
                                }
                            }   
                        }
                    } else if (lowerBuffer == lowerStr){
                        while(lowerBuffer == lowerStr) {
                            countIndex++;
                            tempI++;
                            lowerBuffer = tempBuffer[tempI];
                            if (lowerBuffer >= 65 && lowerBuffer <= 90) 
                                lowerBuffer += 32;
                        }
                        j++;
                    } else if(lowerBuffer == lowerStr2){
                        countIndex++;
                        tempI++;
                        j += 2;
                    } else{
                        break;
                    }
                } else {
                    if (str1[j-1] == ']') {
                        int t = 0;
                        while(t < charGroupSize && bracketGroup[t] != '\0') { 
                            if(tempBuffer[tempI] == bracketGroup[t]){
                                while(tempBuffer[tempI] == bracketGroup[t]) {
                                    countIndex++;
                                    tempI++;
                                }
                                j++;
                                break;
                            } else
                                t++; 
                        }
                    } else if (tempBuffer[tempI] == str1[j-1]){
                        while(tempBuffer[tempI] == str1[j-1]) {
                            countIndex++;
                            tempI++;
                        }
                        j++;
                    } else if(tempBuffer[tempI] == str1[j+1]){
                        countIndex++;
                        tempI++;
                        j += 2;
                    } else{
                        break;
                    }
                }
            } else if (isCaseSensitive == 1 && firstControl == 1 
                    && (tempBuffer[tempI] == str1[j]) 
                    && (tempBuffer[tempI-1] == '\n' || tempI == 0)) {
                    tempI++;
                    j++;
                    countIndex++;
                    control = 1;
            } else if (isCaseSensitive == 1 && control == 1 
                            && firstControl == 1 
                            && (tempBuffer[tempI] == str1[j])) {
                    tempI++;
                    j++;
                    countIndex++;
                    control = 1;
            } else if (isCaseSensitive == 1 && firstControl == 0 
                            && tempBuffer[tempI] == str1[j]){
                    tempI++;
                    j++;
                    countIndex++;
                    control = 1;
            } else if (isCaseSensitive == 0 && firstControl == 1 
                    && (lowerBuffer == lowerStr) 
                    && (tempBuffer[tempI-1] == '\n' || tempI == 0)) {
                    tempI++;
                    j++;
                    countIndex++;
                    control = 1;
            } else if (isCaseSensitive == 0 && control == 1 
                            && firstControl == 1 
                            && (lowerBuffer == lowerStr)) {
                    tempI++;
                    j++;
                    countIndex++;
                    control = 1;
            } else if (isCaseSensitive == 0 && firstControl == 0 
                        && lowerBuffer == lowerStr){
                    tempI++;
                    j++;
                    countIndex++;
                    control = 1;
            } else {
                break;
            }
        }
        if (lastControl == 1) {
            j++;
        }
        if (j == (lenStr1)){
            int tempControl = 0;
            if (tempBuffer[tempI] != '\n' && lastControl == 1){
                tempControl = 1;
            }
            if (tempControl == 0){
                char* temp;
                temp = (char*)malloc(countIndex);
                temp = strncpy(temp, (tempBuffer + tempI - countIndex), countIndex);
                swapStrings(tempBuffer+tempI-countIndex, temp, str2);
                tempI += (lenStr2 - strlen(temp));
                free(temp);
            }         
        }
        if (tempI != i){
            i = tempI;
        } else {
            i++;
        }
    }
    writeFile(fd, tempBuffer);
}

void swapStrings(char* buffer, char* fromString, char* toString) {
    char* substringBuffer = strstr(buffer, fromString);
    if (substringBuffer == NULL){
        return;
    }
    memmove(substringBuffer + strlen(toString), 
            substringBuffer + strlen(fromString), 
            strlen(substringBuffer) - strlen(fromString) + 1);

    memcpy(substringBuffer, toString, strlen(toString));
}

void writeFile(int fd, char* buffer){
    if(lseek (fd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return;
    }
    write(fd, buffer, strlen(buffer));
}