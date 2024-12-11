#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include "errors.h"
#include "caching/caching.h"
#include "path/path.h"
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#define RED       "\x1b[31m"
#define GREEN     "\x1b[32m"
#define YELLOW    "\x1b[33m"
#define BLUE      "\x1b[34m"
#define MAGENTA   "\x1b[35m"
#define CYAN      "\x1b[36m"
#define WHITE     "\x1b[37m"
#define RESET     "\x1b[0m"

// Bright colors
#define BRIGHT_RED     "\x1b[91m"
#define BRIGHT_GREEN   "\x1b[92m"
#define BRIGHT_YELLOW  "\x1b[93m"
#define BRIGHT_BLUE    "\x1b[94m"
#define BRIGHT_MAGENTA "\x1b[95m"
#define BRIGHT_CYAN    "\x1b[96m"
#define BRIGHT_WHITE   "\x1b[97m"
#define PINK           "\x1b[95m"

// Background colors
#define BG_RED       "\x1b[41m"
#define BG_GREEN     "\x1b[42m"
#define BG_YELLOW    "\x1b[43m"
#define BG_BLUE      "\x1b[44m"
#define BG_MAGENTA   "\x1b[45m"
#define BG_CYAN      "\x1b[46m"
#define BG_WHITE     "\x1b[47m"

// Bright background colors
#define BG_BRIGHT_RED     "\x1b[101m"
#define BG_BRIGHT_GREEN   "\x1b[102m"
#define BG_BRIGHT_YELLOW  "\x1b[103m"
#define BG_BRIGHT_BLUE    "\x1b[104m"
#define BG_BRIGHT_MAGENTA "\x1b[105m"
#define BG_BRIGHT_CYAN    "\x1b[106m"
#define BG_BRIGHT_WHITE   "\x1b[107m"


#define __NAMING_SS_PORT__ 8080
#define __NAMING_CLIENT_PORT__ 8081
#define __NAMING_SERVER_IP__ "192.168.16.138"
#define __MAX_STORAGE_SERVERS__ 100
#define __MAX_CLIENTS__ 100
#define __BUFFER__ 5
#define __BUFFER_SIZE__ 4096
#define __MAX_LRU_CACHE_SIZE__ 10
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


// Define the WriteBuffer Node structure
typedef struct WriteBuffer {
    char path[256];            // Path to the file
    char *data;                // Pointer to the data to be written
    int dataSize;           // Size of the data
    int clientSocket;          // Client socket for communication
    struct WriteBuffer *next;  // Pointer to the next node in the list
} WriteBuffer;

#define __STRING__ 0
#define __FILE_REQUEST__ 1
#define __SERVER_INIT_INFO__ 2
#define __SERVER_INFO__ 3
#define __STATUS__ 4

#endif /* HEADERS_H */