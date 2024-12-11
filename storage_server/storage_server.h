#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H
#include"../color.h"
#include"stdbool.h"
#include"./network.h"
#include"./file.h"
// Structure to represent a file request
typedef struct
{
    char opType[20];         // e.g., "READ", "WRITE", "CREATE", "DELETE", "COPY"
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

// Structure to hold thread arguments


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

// Define the WriteBuffer Node structure
typedef struct WriteBuffer {
    char path[256];            // Path to the file
    char *data;                // Pointer to the data to be written
    int dataSize;           // Size of the data
    int clientSocket;          // Client socket for communication
    struct WriteBuffer *next;  // Pointer to the next node in the list
} WriteBuffer;

typedef struct {
    char filePath[256];   // File path to write to
    WriteBuffer *head;    // Linked list head of the write buffer
} ThreadArgs;


// USED FOR GET_DETAILS
// Structure to store file information


#define __STRING__ 0
#define __FILE_REQUEST__ 1
#define __SERVER_INIT_INFO__ 2
#define __SERVER_INFO__ 3
extern int _CLIENT_PORT_ ;
#define BACKLOG 5

#define STORAGE_DIR "storage"
#define NM_PORT 8080 
extern int NM_SS_PORT;
extern char* NM_SERVER_IP ;


// error codes
#define INVALID_PATH 50
#define WRITE_ERR 51
#define DELETE_ERR 52
#define CREATE_ERR 53
#define STREAM_ERR 54
#define INVALID_OP 55
#define READ_ERR   56
#define ALREADY_EXIST 57
#define SUCCESS 0
#define FAILURE 1

void *client_listen_thread_function();
void *handle_client(void *args);

typedef struct PACKET{
    char data[8192];
    int length;
    int stop;
}PACKET;

void *recieve_by_ss(int clientSocket, int dataStruct);

#endif
