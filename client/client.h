#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include "../color.h"


/// FOR AUDIO 

#include <ao/ao.h>
#include <netinet/in.h>
/// FOR AUDIO 

#define BUFFER_SIZE_AUDIO 4096
#define SAMPLE_RATE 44100


// Structure to represent a file request

typedef struct
{
    char opType[20];         // e.g., "READ", "WRITE", "CREATE", "DELETE", "COPY", LIST"  (LIST ke case mein send it to naming server, expect a string back) 
    char path[256];             // File path
    char destination_path[256]; // USED ONLY FOR COPY
    bool isDirectory;           // FLAG USED ONLY FOR CREATING FILE/DIRECTORY
    bool sync;
} FILE_REQUEST_STRUCT;

// Structure to represent server information (IP address and port)
typedef struct
{
    char networkIP[16];
    int portNumber;
    int errorCode; // New field for error code
} SERVER_CONFIG;

typedef struct SERVER_CONFIG CLIENT_CONFIG;

typedef char **List; // Tries or Array
typedef struct
{
    char networkIP[16]; // IP address of the server
    int namingServerPort; // Port number of the naming server
    int clientPort; // Port number of the client
    List Paths; // List of paths stored in the server

    // FOR REDUNDANCY
    int isBackup;
} SERVER_SETUP_INFO;

// USED FOR GET_DETAILS
// Structure to store file information
typedef struct FILE_METADATA
{
    char filename[256];
    long long fileSize;
    char filePermissions[10]; // Assuming RWX format for simplicity
} FILE_METADATA;



typedef struct PACKET{
    char data[8192];
    int length;
    int stop;
}PACKET;


typedef struct STATUS_WRITE{
    int actual;
    int backup1;
    int backup2;
}STATUS_WRITE;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>



#define BUFFER_SIZE 8192
#define NAMING_SERVER_PORT 8081
// #define NAMING_SERVER_IP "192.168.16.138"

// Error Codes
#define INVALID_PATH 50       // Invalid file path provided
#define WRITE_ERR 51          // Failed to write to the file or stream
#define DELETE_ERR 52         // Could not delete the requested file
#define CREATE_ERR 53         // Failed to create the file or resource
#define STREAM_ERR 54         // Streaming operation failed
#define INVALID_OP 55         // Invalid operation requested
#define READ_ERR 56           // Failed to read from the file or stream
#define ALREADY_EXIST 57      // The resource already exists
#define SUCCESS 0             // Operation completed successfully
#define FAILURE 1             // Operation failed due to an unknown error
#define CONNECT_ERR 58

// Error Definitions
#define FIFO_WRITE_ERR 2
#define OPEN_FIFO_ERR 3
#define SEND_ERR 4
#define MPV_FIFO_ERR 5
#define MPV_INIT_ERR 6
#define MPV_CREATE_ERR 7
#define MKFIFO_ERR 8
#define UNLINK_FIFO_ERR 9
#define STORAGE_SERVER_ERR 10
#define NAMING_SERVER_ERR 11
#define INVALID_METADATA_ERR 12
#define INVALID_SERVER_INFO_ERR 13
#define UNKNOWN_ERR 14
#define PERMISSION_DENIED_ERR 15
#define PATH_NOT_FOUND_ERR 16
#define DATA_SEND_ERR 17
#define RECV_ERR 18
#define MEMORY_ALLOC_ERR 19
#define ERR_INVALID_FILE_FORMAT 101
#define ERR_SERVER_DOWN 102

// Error messages
#define ERR_PATH_NOT_EXIST 100

// Function prototypes
int connect_to_server(const char *ip, int port);
int connect_to_naming_server(const char *nm_ip, int nm_port, SERVER_CONFIG *storage_server_info, const char* filePath, const char* operation, int isDirectory,bool sync,int *nmPort);
int read_from_file(const char *fileName, char *buffer);
int list(char *responseBuffer, size_t bufferSize);
void handle_error(int errcode); 
void format_file_name(char *fileName);
int get(const char *fileName, FILE_METADATA *metadata);


#endif // CLIENT_H
