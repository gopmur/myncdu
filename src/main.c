#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>

#include <pthread.h>

#include "darray.h"
#include "main.h"

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
    key_t minFilePathKey;
    key_t maxFilePathKey;
    key_t fileTypesKey;
} Args;

typedef struct _FileTypes {
    int *counts;
    char *types;
    int n;
} FileTypes;

sem_t *sem;
Result *result;

// string should be copied
void *search_and_calculate(void *args) {

    // init args
    char *path = ((Args *)args)->path;
    int minFilePathKey = ((Args *)args)->minFilePathKey;
    int maxFilePathKey = ((Args *)args)->maxFilePathKey;

    DIR *dir = opendir(path);
    struct dirent *item;

    ThreadHandles children;
    thread_handles_init(&children);

    while ((item = readdir(dir)) != NULL) {
        switch (item->d_type) {
            case DT_REG:

                char *filePath = dir_cat(path, item->d_name);
                long fileSize = fsize(filePath);

                sem_wait(sem); // * critical section start
                {
                    if (result->minSize > fileSize) {
                        result->minSize = fileSize;
                        if (result->minFilePathShmid != -1) {
                            shmctl(result->minFilePathShmid, IPC_RMID, NULL);
                        }
                        result->minFilePathShmid = shmget(minFilePathKey, strlen(filePath) + 1, 0666 | IPC_CREAT);
                        char *minFilePath = shmat(result->minFilePathShmid, NULL, 0);
                        strcpy(minFilePath, filePath);
                    }
                    if (result->maxSize < fileSize) {
                        result->maxSize = fileSize;
                        if (result->maxFilePathShmid != -1) {
                            shmctl(result->maxFilePathShmid, IPC_RMID, NULL);
                        }
                        result->maxFilePathShmid = shmget(maxFilePathKey, strlen(filePath) + 1, 0666 | IPC_CREAT);
                        char *maxFilePath = shmat(result->maxFilePathShmid, NULL, 0);
                        strcpy(maxFilePath, filePath);
                    }
                    result->totalSize += fileSize;
                    result->numberOfFiles++;
                }
                sem_post(sem); // ! critical section end
                free(filePath);
                break;

            case DT_DIR:
                if (strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                    pthread_t ptid;
                    Args *args = (Args *)malloc(sizeof(Args));
                    args->path = dir_cat(path, item->d_name);
                    args->minFilePathKey = minFilePathKey;
                    args->maxFilePathKey = maxFilePathKey;
                    pthread_create(&ptid, NULL, &search_and_calculate, (void *)args);
                    thread_handles_add(&children, ptid);
                }
                break;
        }
    }

    for (int i = 0; i < children.len; i++) {
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
    if (currentPath == NULL) {
        printf("No such directory: %s", argv[1]);
        return -1;
    }
    dir = opendir(currentPath);

    key_t resultKey = ftok("myncdu", 'a');
    key_t minFilePathKey = ftok("myncdu", 'b');
    key_t maxFilePathKey = ftok("myncdu", 'c');
    key_t semKey = ftok("myncdu", 'd');
    key_t fileTypesKey = ftok("myncdu", 'x');

    int sempShmid = shmget(semKey, sizeof(sem_t), 0666 | IPC_CREAT);
    int resultShmid = shmget(resultKey, sizeof(Result), 0666 | IPC_CREAT);

    result = (Result *)shmat(resultShmid, NULL, 0);

    result->totalSize = 0;
    result->minSize = LONG_MAX;
    result->maxSize = 0;
    result->minFilePathShmid = -1;
    result->maxFilePathShmid = -1;
    result->fileTypesShmid = -1;

    sem = (sem_t *)shmat(sempShmid, NULL, 0);
    sem_init(sem, 1, 1);


    if (dir) {
        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_DIR && strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                pid = fork();
                if (pid == 0) break;
            }

            // parent
            else if (item->d_type == DT_REG) {

                char *fileName = item->d_name;
                char *filePath = dir_cat(currentPath, fileName);
                long fileSize = fsize(filePath);

                sem_wait(sem); // * critical section start
                {
                    if (result->minSize > fileSize) {
                        result->minSize = fileSize;
                        if (result->minFilePathShmid != -1) {
                            shmctl(result->minFilePathShmid, IPC_RMID, NULL);
                        }
                        result->minFilePathShmid = shmget(minFilePathKey, strlen(filePath) + 1, 0666 | IPC_CREAT);
                        char *minFilePath = shmat(result->minFilePathShmid, NULL, 0);
                        strcpy(minFilePath, filePath);
                    }
                    if (result->maxSize < fileSize) {
                        result->maxSize = fileSize;
                        if (result->maxFilePathShmid != -1) {
                            shmctl(result->maxFilePathShmid, IPC_RMID, NULL);
                        }
                        result->maxFilePathShmid = shmget(maxFilePathKey, strlen(filePath) + 1, 0666 | IPC_CREAT);
                        char *maxFilePath = shmat(result->maxFilePathShmid, NULL, 0);
                        strcpy(maxFilePath, filePath);
                    }
                    char *fileType = strrchr(fileName, '.');
                    if (fileType == NULL) {
                        fileType = "no extension";
                    }
                    int fileTypeLen = strlen(fileType);
                    bool found = false;

                    if (result->fileTypesShmid == -1) {
                        result->fileTypesShmid = shmget(fileTypesKey, sizeof(FileTypes) + sizeof(int) + (fileTypeLen + 1), 0666 | IPC_CREAT);
                        FileTypes *fileTypes = (FileTypes *)shmat(result->fileTypesShmid, NULL, 0);
                        fileTypes->n = 1;
                        fileTypes->counts = (int *)((void *)fileTypes + sizeof(FileTypes));
                        fileTypes->types = (char *)((void *)fileTypes + sizeof(FileTypes) + sizeof(int) * fileTypes->n);
                        strcpy(&fileTypes->types[0], fileType);
                        fileTypes->counts[0] = 1;
                    }
                    else {
                        FileTypes *fileTypes = (FileTypes *)shmat(result->fileTypesShmid, NULL, 0);
                        int i = 0;
                        int j = 0;
                        while (j < fileTypes->n) {
                            if (!strcmp(&fileTypes->types[i], fileType)) {
                                fileTypes->counts[j]++;
                                found = true;
                                break;
                            }
                            i += strlen(&fileTypes->types[i]) + 1;
                            j++;
                        }
                    }

                    if (!found && result->fileTypesShmid != -1) {




                    }
                    result->totalSize += fileSize;
                    result->numberOfFiles++;
                }
                sem_post(sem); // ! critical section end

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

                sem_wait(sem); // * critical section start
                {
                    if (result->minSize > fileSize) {
                        result->minSize = fileSize;
                        if (result->minFilePathShmid != -1) {
                            shmctl(result->minFilePathShmid, IPC_RMID, NULL);
                        }
                        result->minFilePathShmid = shmget(minFilePathKey, strlen(filePath) + 1, 0666 | IPC_CREAT);
                        char *minFilePath = shmat(result->minFilePathShmid, NULL, 0);
                        strcpy(minFilePath, filePath);
                    }
                    if (result->maxSize < fileSize) {
                        result->maxSize = fileSize;
                        if (result->maxFilePathShmid != -1) {
                            shmctl(result->maxFilePathShmid, IPC_RMID, NULL);
                        }
                        result->maxFilePathShmid = shmget(maxFilePathKey, strlen(filePath) + 1, 0666 | IPC_CREAT);
                        char *maxFilePath = shmat(result->maxFilePathShmid, NULL, 0);
                        strcpy(maxFilePath, filePath);
                    }
                    result->totalSize += fileSize;
                    result->numberOfFiles++;
                }
                sem_post(sem); // ! critical section end

                free(filePath);
            }
            else if (item->d_type == DT_DIR && strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                pthread_t ptid;
                Args *args = (Args *)malloc(sizeof(Args));
                args->path = dir_cat(currentPath, item->d_name);
                args->minFilePathKey = minFilePathKey;
                args->maxFilePathKey = maxFilePathKey;
                pthread_create(&ptid, NULL, &search_and_calculate, args);
                thread_handles_add(&children, ptid);
            }
        }

        for (int i = 0; i < children.len; i++) {
            pthread_join(children.ids[i], NULL);
        }

        thread_handles_delete(&children);

    }

    // parent
    else {

        // wait for all children to finish
        while (wait(NULL) > 0);

        printf("total number of files: %d\n", result->numberOfFiles);

        if (result->totalSize >= (1 << 30)) printf("total size: %.2f GiB\n", ((double)result->totalSize) / (1 << 30));
        else if (result->totalSize >= (1 << 20)) printf("total size: %.2f MiB\n", ((double)result->totalSize) / (1 << 20));
        else if (result->totalSize >= (1 << 10)) printf("total size: %.2f KiB\n", ((double)result->totalSize) / (1 << 10));
        else printf("total size: %d B\n", result->totalSize);

        if (result->maxSize >= (1 << 30)) printf("max size: %.2f GiB\n", ((double)result->maxSize) / (1 << 30));
        else if (result->maxSize >= (1 << 20)) printf("max size: %.2f MiB\n", ((double)result->maxSize) / (1 << 20));
        else if (result->maxSize >= (1 << 10)) printf("max size: %.2f KiB\n", ((double)result->maxSize) / (1 << 10));
        else printf("max size: %d B\n", result->maxSize);

        if (result->minSize >= (1 << 30)) printf("min size: %.2f GiB\n", ((double)result->minSize) / (1 << 30));
        else if (result->minSize >= (1 << 20)) printf("min size: %.2f MiB\n", ((double)result->minSize) / (1 << 20));
        else if (result->minSize >= (1 << 10)) printf("min size: %.2f KiB\n", ((double)result->minSize) / (1 << 10));
        else printf("min size: %d B\n", result->minSize);


        char *minFilePath = NULL;
        char *maxFilePath = NULL;
        if (result->maxFilePathShmid != -1) maxFilePath = shmat(result->maxFilePathShmid, NULL, 0);
        if (result->minFilePathShmid != -1) minFilePath = shmat(result->minFilePathShmid, NULL, 0);
        printf("max file path: %s\n", maxFilePath);
        printf("min file path: %s\n", minFilePath);

        FileTypes *fileTypes = (FileTypes *)shmat(result->fileTypesShmid, NULL, 0);

        int j = 0;
        int i = 0;

        while (j < fileTypes->n) {
            printf("%s: %d\n", &fileTypes->types[i], fileTypes->counts[j]);
            i += strlen(&fileTypes->types[i]) + 1;
            j++;
        }

        shmctl(resultShmid, IPC_RMID, NULL);
        shmctl(sempShmid, IPC_RMID, NULL);
        shmctl(result->minFilePathShmid, IPC_RMID, NULL);
        shmctl(result->maxFilePathShmid, IPC_RMID, NULL);
        shmctl(result->fileTypesShmid, IPC_RMID, NULL);
    }

    closedir(dir);
    free(currentPath);

    return 0;

}