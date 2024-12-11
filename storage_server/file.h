#ifndef FILE_H
#define FILE_H
#include"storage_server.h"
#include"../color.h"
typedef struct FILE_METADATA
{
    char filename[256];
    long long fileSize;
    char filePermissions[10]; // Assuming RWX format for simplicity
} FILE_METADATA;
int delete_file_dir(const char *path, bool isDir);
int delete_directory(const char *dirpath);
int create_file_dir(const char *path, bool isDir);
int write_file(int clientSocket,const char *path,bool sync);
int read_file(int clientSocket, const char *path);
int get_file(const char * path,FILE_METADATA *fileMetadata);
int stream_audio(int clientSocket,const char * path);
void seek_recursive_concatenate(const char *path, const char *current_path, char *result);

#endif