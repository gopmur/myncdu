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

#include "darray.h"

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

typedef struct _Args {
    char *path;
} Args;

sem_t sem;
Result *result;

// string should be copied
void *search_and_calculate(void *args) {

    // init args
    char *path = ((Args *)args)->path;

    DIR *dir = opendir(path);
    struct dirent *item;

    ThreadHandles children;
    thread_handles_init(&children);

    while ((item = readdir(dir)) != NULL) {
        switch (item->d_type) {
            case DT_REG:

                char *filePath = dir_cat(path, item->d_name);
                long fileSize = fsize(filePath);
                printf("%s: %d\n", filePath, fileSize);

                sem_wait(&sem); // * critical section start
                {
                    if (result->minSize > fileSize) {
                        result->minSize = fileSize;
                    }
                    if (result->maxSize < fileSize) result->maxSize = fileSize;
                    result->totalSize += fileSize;
                }
                sem_post(&sem); // ! critical section end
                free(filePath);
                break;

            case DT_DIR:
                if (strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                    pthread_t ptid;
                    Args *args = (Args *)malloc(sizeof(Args));
                    args->path = dir_cat(path, item->d_name);
                    pthread_create(&ptid, NULL, &search_and_calculate, (void *)args);
                    thread_handles_add(&children, ptid);
                }
                break;
        }
    }

    for (int i = 0; i < children.n; i++) {
        pthread_join(children.ids[i], NULL);
    }

    thread_handles_delete(&children);
    free(path);
    closedir(dir);
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
    result = (Result *)shmat(shmid, NULL, 0);

    result->totalSize = 0;
    result->minSize = LONG_MAX;
    result->maxSize = 0;

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
                {
                    if (result->minSize > fileSize) {
                        result->minSize = fileSize;
                    }
                    if (result->maxSize < fileSize) result->maxSize = fileSize;
                    result->totalSize += fileSize;
                }
                sem_post(&sem); // ! critical section end

                free(filePath);
            }
        }
    }

    // child proc
    if (pid == 0) {

        // create new dir addr in path var
        char *currentFolderName = item->d_name;

        char *newPath = dir_cat(currentPath, currentFolderName);
        free(currentPath);
        currentPath = newPath;

        // open new dir
        closedir(dir);
        dir = opendir(currentPath);

        ThreadHandles children;
        thread_handles_init(&children);

        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_REG) {

                char *filePath = dir_cat(currentPath, item->d_name);

                long fileSize = fsize(filePath);
                printf("%s : %ld\n", filePath, fileSize);

                sem_wait(&sem); // * critical section start
                {
                    if (result->minSize > fileSize) result->minSize = fileSize;
                    if (result->maxSize < fileSize) result->maxSize = fileSize;
                    result->totalSize += fileSize;
                }
                sem_post(&sem); // ! critical section end

                free(filePath);
            }
            else if (item->d_type == DT_DIR && strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                pthread_t ptid;
                Args *args = (Args *)malloc(sizeof(Args));
                args->path = dir_cat(currentPath, item->d_name);
                pthread_create(&ptid, NULL, &search_and_calculate, args);
                thread_handles_add(&children, ptid);
            }
        }

        for (int i = 0; i < children.n; i++) {
            pthread_join(children.ids[i], NULL);
        }

        thread_handles_delete(&children);

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