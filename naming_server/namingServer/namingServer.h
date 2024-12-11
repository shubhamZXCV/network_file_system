#ifndef NAMING_SERVER_H
#define NAMING_SERVER_H

int numberOfStorageServers;
int numberOfClients;
typedef struct storageServerArgs{
    int index;
    int socketFD;
}STORAGE_SERVER_ARGS;

#define ERR_DIRECTORY_MISMATCH 99
#define ERR_PATH_NOT_EXIST 100
#define ERR_INVALID_FILE_FORMAT 101
#define ERR_SERVER_DOWN 102
#endif