#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>

#include <pthread.h>

#include "stack.h"

typedef struct _StrIntPair {
    char *str;
    int i;
} StrIntPair;

typedef struct _Result {
    int numberOfFiles;
    StrIntPair *fileTypes;
    char *maxFilePath;
    long maxSize;
    char *minFilePath;
    long minSize;
    long totalSize;
} Result;

long fsize(char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fclose(file);
        return size;
    }
    else {
        printf("Error\n");
        return -1;
    }

}

char *dir_cat(char *currentDirectory, char *fileName) {
    char *filePath = (char *)malloc(strlen(currentDirectory) + 2 + strlen(fileName));
    strcpy(filePath, currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileName);
    return filePath;
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printf("No target directory specified");
        return 0;
    }

    pid_t pid;
    DIR *dir;
    struct dirent *item;
    char *currentPath = realpath(argv[1], NULL);
    dir = opendir(currentPath);

    key_t resultKey = ("myncdu", 'a');
    key_t minFilePathKey = ("myncdu", 'b');
    key_t maxFilePathKey = ("myncdu", 'c');
    int shmid = shmget(resultKey, sizeof(Result), 0666 | IPC_CREAT);
    Result *result = (Result *)shmat(shmid, NULL, 0);

    result->totalSize = 0;
    result->minSize = LONG_MAX;
    result->maxSize = 0;

    sem_t sem;
    sem_init(&sem, 0, 1);

    if (dir) {
        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_DIR && strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                pid = fork();
                if (pid == 0) break;
            }

            // parent
            else if (item->d_type == DT_REG) {

                char *filePath = dir_cat(currentPath, item->d_name);
                long fileSize = fsize(filePath);
                printf("%s: %d\n", filePath, fileSize);

                sem_wait(&sem); // * critical section start

                if (result->minSize > fileSize) {
                    result->minSize = fileSize;
                }
                if (result->maxSize < fileSize) result->maxSize = fileSize;
                result->totalSize += fileSize;

                sem_post(&sem); // ! critical section end

                free(filePath);
            }
        }
    }

    // child
    if (pid == 0) {

        // create new dir addr in path var
        char *currentFolderName = item->d_name;

        char *newPath = dir_cat(currentPath, currentFolderName);
        free(currentPath);
        currentPath = newPath;

        // open new dir
        closedir(dir);
        dir = opendir(currentPath);


        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_REG) {

                char *filePath = dir_cat(currentPath, item->d_name);

                long fileSize = fsize(filePath);
                printf("%s : %ld\n", filePath, fileSize);

                sem_wait(&sem); // * critical section start

                if (result->minSize > fileSize) result->minSize = fileSize;
                if (result->maxSize < fileSize) result->maxSize = fileSize;
                result->totalSize += fileSize;

                sem_post(&sem); // ! critical section end

                free(filePath);
            }
        }

    }

    // parent
    else {

        // wait for all children to finish

        while (wait(NULL) > 0);
        printf("total size: %d\n", result->totalSize);
        printf("max size: %d\n", result->maxSize);
        printf("min size: %d\n", result->minSize);
        shmctl(shmid, IPC_RMID, NULL);
    }

    closedir(dir);
    free(currentPath);

    return 0;

}
