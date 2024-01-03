typedef struct _Result {
    unsigned long totalSize;
    unsigned long maxSize;
    unsigned long minSize;
    int minFilePathShmid;
    int maxFilePathShmid;
    int fileTypesShmid;
    int numberOfFiles;
} Result;