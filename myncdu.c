#define _DEFAULT_SOURCE

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>

int main(int argc, char **argv)
{
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
        }
    }
     
    // child
    if (pid == 0) {

        // create new dir addr in path var
        int path_str_len = strlen(argv[1]) + strlen(item->d_name) + 1;
        char *path = (char *) malloc(path_str_len + 1);
        strcpy(path, argv[1]);
        strcat(path, "/");
        strcat(path, item->d_name);

        // open new addr
        // closedir(dir);
        dir = opendir(path);
        printf("%s START\n", path);
        while ((item = readdir(dir)) != NULL) {
            if (item->d_type == DT_REG) {
                printf("%s\n", item->d_name);
            }
        }
        printf("%s END\n", path);
    }

    // parent
    else {
        while(wait(NULL) > 0);
        closedir(dir);
    }

    return 0;

}
