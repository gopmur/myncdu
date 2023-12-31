#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>

#include <pthread.h>

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

char *dircat(char *currentDirectory, char *fileName) {
    char *filePath = (char *) malloc(strlen(currentDirectory) + 2 + strlen(fileName));
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
    dir = opendir(argv[1]);

    if (dir) {
        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_DIR && strcmp(item->d_name, ".") && strcmp(item->d_name, "..")) {
                pid = fork();
                if (pid == 0) break;
            }
            else if (item->d_type == DT_REG) {
                char *filePath = getFilePath(argv[1], item->d_name);
                long size = fsize(filePath);

                printf("%s: %d\n", filePath, size);

                free(filePath);
            }
        }
    }

    // child
    if (pid == 0) {

        // create new dir addr in path var
        char *currentFolder = item->d_name;
        int currentPathLen = strlen(argv[1]) + strlen(item->d_name) + 1;
        char *currentPath = (char *)malloc(currentPathLen + 1);
        strcpy(currentPath, argv[1]);
        strcat(currentPath, "/");
        strcat(currentPath, currentFolder);

        // open new dir
        closedir(dir);
        dir = opendir(currentPath);


        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_REG) {

                int filePathLen = strlen(currentPath) + 1 + strlen(item->d_name);
                char *filePath = (char *)malloc(filePathLen + 1);

                strcpy(filePath, currentPath);
                strcat(filePath, "/");
                strcat(filePath, item->d_name);
                long fileSize = fsize(filePath);
                printf("%s : %ld\n", filePath, fileSize);

                free(filePath);
            }
        }

        // close dir
        free(currentPath);
        closedir(dir);
    }

    // parent
    else {

        // wait for all children to finish


        while ((item = readdir(dir) != NULL)) {



        }

        closedir(dir);
        while (wait(NULL) > 0);

    }

    return 0;

}
