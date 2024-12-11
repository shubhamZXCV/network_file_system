#include "../globals.h"
#include "namingServer.h"
#include "../log/log.h"
#include "../return_struct.h"




SERVER_SETUP_INFO listStorgeServers[__MAX_STORAGE_SERVERS__];
bool connected[__MAX_STORAGE_SERVERS__];
int storageServersFD[__MAX_STORAGE_SERVERS__];

// variable to help in redundancy
int storageServersCountTillNow;

// trie to store the paths
TrieNode* root;

pthread_mutex_t ssListLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clientLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ssConnectedLock = PTHREAD_MUTEX_INITIALIZER;

pthread_t listOfClientThreads[__MAX_CLIENTS__];
// pthread_t listOfStorageServerThreads[__MAX_STORAGE_SERVERS__];
pthread_t listOfFailureDetectionThreads[__MAX_STORAGE_SERVERS__];

LRUCache* cache;

void handle_error(int error_code, const char *opType) {
    switch (error_code) {
        case ERR_DIRECTORY_MISMATCH:
            printf("Directory/File Mismatch: DELETE Failed!");
            break;

        case ERR_PATH_NOT_EXIST:
            printf("Path does not exist in Storage Servers: %s Failed!", opType);
            break;

        default:
            printf("Unknown error code: %d\n", error_code);
            break;
    }
}

// initiate the storage server list
void initStorageServerList(){
    numberOfClients = 0;
    numberOfStorageServers = 0;
    for(int i=0;i<__MAX_STORAGE_SERVERS__;i++){
        listStorgeServers[i].namingServerPort = -1;
        connected[i] = false;
        listStorgeServers[i].isBackup = 0;
    }
    storageServersCountTillNow = 0;
}
// function to setup a socket 
int setupSocket(int* serverSocket, int port){
    *serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(*serverSocket == -1){
        perror("socket() failed");
        return -1;  
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);
    int opt = 1;
    if(setsockopt(*serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        perror("setsockopt() failed");
        return -1;
    }

    if(bind(*serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
        perror("bind() failed");
        return -1;
    }

    if(listen(*serverSocket, 10) == -1){
        perror("listen() failed");
        return -1;
    }
    
}
void* namingServerReceive(int socketFD, int dataType,bool fromClient){
    void* serverInfo;
    // printf("here");
    if(dataType == __STRING__){
        serverInfo = (char*)malloc(__BUFFER__+1);

        int bytesReceived = recv(socketFD, serverInfo, __BUFFER__, 0);
        if(bytesReceived <= 0){
            perror("recv() failed");
            return NULL;
        }
        ((char*)serverInfo)[bytesReceived] = '\0';
    }
    else if(dataType == __FILE_REQUEST__){
        serverInfo = (FILE_REQUEST_STRUCT*) malloc(sizeof(FILE_REQUEST_STRUCT));
        if(recv(socketFD, serverInfo, sizeof(FILE_REQUEST_STRUCT), 0) <= 0){
            // printf("Mehta\n");
            perror("recv() failed");
            return NULL;
        }
        // printf("here");

    }
    else if(dataType == __SERVER_INIT_INFO__){
        serverInfo = (SERVER_SETUP_INFO*) malloc(sizeof(SERVER_SETUP_INFO));
        if(recv(socketFD, serverInfo, sizeof(SERVER_SETUP_INFO), 0) <= 0){
            perror("recv() failed");
            return NULL;
        }
    }
    else if(dataType == __SERVER_INFO__){
        serverInfo = (SERVER_CONFIG*) malloc(sizeof(SERVER_CONFIG));
        if(recv(socketFD, serverInfo, sizeof(SERVER_CONFIG), 0) <= 0){
            perror("recv() failed");
            return NULL;
        }
    }
    logMessageRecieve(socketFD, dataType, &serverInfo, fromClient);
    return serverInfo;
}

void generateTotalPath(char* childDir, char* parentDir, char* recievedDirname){
    strcpy(childDir, parentDir);
    concatenateStrings(childDir, "/");
    concatenateStrings(childDir, recievedDirname);
}
void convertSS_NO_to_string(int storageServer, char* ssnString){
    sprintf(ssnString, "%d", storageServer);
}

void recieveDirPathTrie(int socketFD, char* parentDir, int storageServer) {
    char* recievedDirname = (char *)malloc(8192);
    memset(recievedDirname, 0, 8192);
    int stringLen;
    char fullPath[8192] = ""; // Buffer to accumulate all directories

    stringLen = recv(socketFD, recievedDirname, 8192, 0);
    if (stringLen <= 0) {
        perror("recv() failed");
        // break;
    }

    recievedDirname[stringLen] = '\0';
    logMessageRecieve(socketFD, __STRING__, (void**)&recievedDirname, false);
    printf("Received: %s\n", recievedDirname);

    // Accumulate the directory names into a full path string
    strcat(fullPath, recievedDirname);
    strcat(fullPath, ";"); // Use semicolon as a delimiter


    root = trieInsert(root, "./storage", "0", true);
    //
    // Tokenize the accumulated string using strtok_r
    char* saveptr;
    char* dirName = strtok_r(fullPath, ";", &saveptr);
    while (dirName != NULL) {
        if (strlen(dirName) > 0) {  // Ensure the directory is not empty
            // Generate the full path for the current directory
            char childDir[__BUFFER_SIZE__];
            bool isDirectory = false;
            if(dirName[0]=='1'){
                isDirectory = true;
            }
            generateTotalPath(childDir, parentDir, dirName+1);

            // Convert storage server number to string
            char ssnString[__BUFFER__];
            convertSS_NO_to_string(storageServer, ssnString);
            // printf("SSN: %s\n", ssnString);
            // Insert the directory path into the trie
            root = trieInsert(root, childDir, ssnString, isDirectory);
            // printf("Path: %s\n", childDir);
        }

        // Get the next directory
        dirName = strtok_r(NULL, ";", &saveptr); // Get the next token
    }

    printf("End of Transmission\n");
    free(recievedDirname);
    return;
}


bool refuseConnection_Max_limit(){
    return numberOfStorageServers == __MAX_STORAGE_SERVERS__;
}

void updateListStorageServers(int clientPort, int namingServerPort, char* networkIP, List Paths){
    listStorgeServers[numberOfStorageServers].clientPort = clientPort;
    // printf("Client Port: %d\n", listStorgeServers[numberOfStorageServers].clientPort);
    listStorgeServers[numberOfStorageServers].namingServerPort = namingServerPort;
    strcpy(listStorgeServers[numberOfStorageServers].networkIP, networkIP);
    listStorgeServers[numberOfStorageServers].Paths = Paths;
}



void* failureDetectionThread(void* args) {
    STORAGE_SERVER_ARGS arg = *(STORAGE_SERVER_ARGS*)args;  // Copy arguments
    int sockfd = arg.socketFD;
    int index = arg.index;         // Server index
    char buffer[8192];
    const char* PING = "PING";
    const int PING_LEN = 4;

    // Set a 5-second timeout for the recv function


    while (true) {
        // Send a "PING" message to the server
        // printf("Sending PING to server %d\n", index + 1);
        // if (send(sockfd, PING, PING_LEN, 0) <= 0) {
        //     pthread_mutex_lock(&ssConnectedLock);
        //     fprintf(stderr, RED "[+] Server %d disconnected (send failed: %s)\n" RESET, 
        //             index + 1, strerror(errno));
        //     connected[index] = 0;
        //     pthread_mutex_unlock(&ssConnectedLock);

        //     // Handle failure
        //     pthread_exit(NULL);
        // }
        if (send(sockfd, PING, PING_LEN, MSG_NOSIGNAL) <= 0) {
            pthread_mutex_lock(&ssConnectedLock);
            fprintf(stderr, RED "[+] Server %d disconnected (connection closed by peer)\n" RESET, 
                    index + 1);
            connected[index] = 0;
            pthread_mutex_unlock(&ssConnectedLock);
            pthread_exit(NULL);
        }

        // Wait for "PONG" acknowledgment
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            pthread_mutex_lock(&ssConnectedLock);
            if (bytes_received == 0) {
                fprintf(stderr, RED "[-] Server %d disconnected (connection closed by peer)\n" RESET, index + 1);
            } else {
                fprintf(stderr, RED "[-] Server %d disconnected (connection closed by peer)\n" RESET, index + 1);
            }
            connected[index] = 0;
            pthread_mutex_unlock(&ssConnectedLock);
            pthread_exit(NULL);
        }

        // printf("PONG received from server %d\n", index + 1);
        sleep(2);  // Wait before the next heartbeat
    }

    return NULL;
}


int makeCopyRequest(FILE_REQUEST_STRUCT* request, int storageServerName, int destStorageServer, bool isBackup, int originalStorageServer);

void removeANSISequences(char* str);
int createSocketAndConnect(int storageServerName);
int sendToSSForDeleteAndCreate(FILE_REQUEST_STRUCT* request, int storageServerName, bool isBackup,int originalStorageServer);

int directoryChecker(char* finalList, char* baseDir, int storageServerName, bool isBackup, int destStorageServer, int originalStorageServer){
    FILE_REQUEST_STRUCT request;
    // first delete in backup folder
    strcpy(request.opType, "DELETE");
    strcpy(request.path, finalList);
    strcpy(request.destination_path, baseDir);
    request.isDirectory = true;
    printf("Deleting: %s\n",request.path);
    int status = sendToSSForDeleteAndCreate(&request, destStorageServer, isBackup,originalStorageServer);

    // strcpy(request.opType, "CREATE");
    // strcpy(request.path, baseDir);
    // strcpy(request.destination_path, baseDir);
    // request.isDirectory = true;

    // status = sendToSSForDeleteAndCreate(&request, storageServerName, isBackup);

}

int fileChecker(char* finalList, char* baseDir, int storageServerName, bool isBackup, int destStorageServer,int originalStorageServer){
    FILE_REQUEST_STRUCT request;
    // first delete in backup folder
    printf("Deleting: %s\n",finalList);
    strcpy(request.opType, "DELETE");
    strcpy(request.path, finalList);
    strcpy(request.destination_path, baseDir);
    request.isDirectory = false;    

    sendToSSForDeleteAndCreate(&request, destStorageServer, isBackup,originalStorageServer);
}

int redundancyCheck(char* finalList, char* baseDir, int storageServerName, int destStorageServer, bool isBackup,int originalStorageServer) {
    char* saveptr;            // Save pointer for strtok_r
    char* token = strtok_r(finalList, "\n", &saveptr); // Use '\n' as the delimiter

    while (token != NULL) {
        // check if token contains BRIGHT_CYAN then its a directory
        if(strstr(token, "\033[0;36m") != NULL){
            removeANSISequences(token);
            if(strcmp(token,"./storage")==0){
                token = strtok_r(NULL, "\n", &saveptr); // Continue tokenizing
                continue;
            }
            printf(YELLOW"----- Directory: %s -----\n"RESET, token);
            directoryChecker(token, baseDir, storageServerName, isBackup, destStorageServer, originalStorageServer);

        }
        else{
            removeANSISequences(token);
            printf(YELLOW"----- File: %s -----\n"RESET, token);
            fileChecker(token, baseDir, storageServerName, isBackup, destStorageServer, originalStorageServer);
        }
        token = strtok_r(NULL, "\n", &saveptr); // Continue tokenizing
    }
    return __OK__;
}
int tokenizeAndPrintPaths(char* finalList,char* baseDir,FILE_REQUEST_STRUCT* request,int storageServerName,bool isBackup,int destStorageServer,int originalStorageServer);

int makeCopy(FILE_REQUEST_STRUCT* request, int storageServerName,int destStorageServer,bool isBackup,int originalStorageServer){
    printf("----------------- SRC INFO -----------------\n");
    printf("Storage Server Name: %d\n", storageServerName);
    printf("Storage Server IP: %s\n", listStorgeServers[storageServerName].networkIP);
    printf("Storage Server Port: %d\n", listStorgeServers[storageServerName].namingServerPort);
    printf("----------------- DEST INFO -----------------\n");
    printf("Storage Server Name: %d\n", destStorageServer);
    printf("Storage Server IP: %s\n", listStorgeServers[destStorageServer].networkIP);
    printf("Storage Server Port: %d\n", listStorgeServers[destStorageServer].namingServerPort);


    char path[__BUFFER_SIZE__] = "";
    printf("Request Path: %s\n", request->path);
    char* finalList = listAllDirectoriesForCopy1(request->path, root, path,originalStorageServer);
    
    // THE GIVEN PATH CONTAINS CONCACTENATED PATHS PRINT THEM CORRECTLY
    printf("Path: %s",finalList);
    tokenizeAndPrintPaths(finalList, request->path,request,storageServerName,isBackup,destStorageServer,originalStorageServer);

    return __OK__;
}




void redundancy(int storageServer, int storageServerSocket){
    // creat all possible paths with value storageServer-1 and storageServer-2
    // first delete everything and then write everything to the file
    char path[8192];
    
    if(storageServerSocket != -1){
        recv(storageServerSocket, path, 8192, 0);
    }
    sleep(5);
    char backup[8192]; 
    strcpy(path,"");

    if(storageServer-1>=0){
        char str[10];
        sprintf(str, "%d", storageServer-1);
        createStringinListAllPathsForValue(backup,root,path,str);
        printf("Backup1: %s\n",backup);

        redundancyCheck(backup,"",storageServer-1,storageServer,true,storageServer-1); // check if backup1 is updated or not for index - 1

        FILE_REQUEST_STRUCT request;
        strcpy(request.opType, "COPY");
        strcpy(request.path, "./storage");
        strcpy(request.destination_path, "./storage");
        request.isDirectory = true;

        makeCopy(&request,storageServer-1,storageServer,true,storageServer-1);
    }
    if(storageServer-2>=0){
        char str[10];
        sprintf(str, "%d", storageServer-2);
        createStringinListAllPathsForValue(backup,root,path,str);
        printf("Backup2: %s\n",backup);

        redundancyCheck(backup,"",storageServer-2,storageServer,true,storageServer-1);


        FILE_REQUEST_STRUCT request;
        strcpy(request.opType, "COPY");
        strcpy(request.path, "./storage");
        strcpy(request.destination_path, "./storage");
        request.isDirectory = true;
        makeCopy(&request,storageServer-2,storageServer,true,storageServer-2);
    }
    ///////////// make sure this storage Server is up to date //////////////////////
    char str[10];
    sprintf(str, "%d", storageServer);
    createStringinListAllPathsForValue(backup,root,path,str);
    printf("Backup: %s\n",backup);
    int latest = storageServer+2;
    if(latest==-1 || connected[latest] == false){
        // printf(RED"Data is not updated failed to sync the changes\n");
        return;
    }
    redundancyCheck(backup,"",latest,storageServer,true,storageServer); //// check if storageServer backup is updated or not
    FILE_REQUEST_STRUCT request;
    strcpy(request.opType, "COPY");
    strcpy(request.path, "./storage");
    strcpy(request.destination_path, "./storage");
    request.isDirectory = true;
    makeCopy(&request,latest,storageServer,true,storageServer);

}

void* listenStorageServerThread(){
    int storageServerSocket;
    if(setupSocket(&storageServerSocket, __NAMING_SS_PORT__) == -1){
        return NULL;
    }
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Naming Server is Ready to Connect! \n"PINK"---------------------------------------\n"YELLOW"📡 Listening for Storage Servers on: Port %d\n"CYAN"=======================================\n\n"RESET,__NAMING_SS_PORT__);
    
    pthread_t threadforStorageServer;
    while(true){
        int storageServerSocketFD;
        struct sockaddr_in storageServerAddress;
        socklen_t storageServerAddressLength = sizeof(storageServerAddress);
        if(refuseConnection_Max_limit()){
            continue;
        }
        if((storageServerSocketFD = accept(storageServerSocket, (struct sockaddr*)&storageServerAddress, &storageServerAddressLength)) == -1){
            perror("accept() failed");
            continue;
        }
        SERVER_SETUP_INFO* storageServerInfo = (SERVER_SETUP_INFO*)namingServerReceive(storageServerSocketFD, __SERVER_INIT_INFO__, false);
        ///////////////////////////////////////////////////////////////////////////////
        ////////////////////////// Failure detection //////////////////////////////////
        //////////////////////////// Thread ///////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////////
        // printf("Network IP: %s\n", storageServerInfo->networkIP);
        STORAGE_SERVER_ARGS* args = (STORAGE_SERVER_ARGS*)malloc(sizeof(STORAGE_SERVER_ARGS));
        pthread_mutex_lock(&ssListLock);
        bool flag = 0;
        for(int i=0;i<numberOfStorageServers;i++){
            if(listStorgeServers[i].namingServerPort == storageServerInfo->namingServerPort && listStorgeServers[i].clientPort == storageServerInfo->clientPort && strcmp(listStorgeServers[i].networkIP, storageServerInfo->networkIP) == 0){
                pthread_mutex_lock(&ssConnectedLock);
                connected[i] = true;
                pthread_mutex_unlock(&ssConnectedLock);
                printf(BRIGHT_GREEN"[+] Server %d back online\n"RESET, i+1);
                redundancy(i,storageServerSocketFD);
                args->index = i;
                args->socketFD = storageServerSocketFD;
                pthread_create(&threadforStorageServer, NULL, failureDetectionThread, (void*)args);
                flag = 1;
                break;
            }
        }
        pthread_mutex_unlock(&ssListLock);

        if(flag == 1){
            continue;
        }
        storageServersCountTillNow++;
        printf(
            BRIGHT_GREEN "[+] " 
            YELLOW "Storage Server #%d successfully connected\n" 
            RESET, 
            storageServersCountTillNow
        );
        printf(
            BRIGHT_CYAN "[Server Info] " 
            YELLOW "IP: %s | Naming Server Port: %d | Client Port: %d\n" 
            RESET, 
            storageServerInfo->networkIP, 
            storageServerInfo->namingServerPort, 
            storageServerInfo->clientPort
        );


        connected[numberOfStorageServers] = true;
        int index = numberOfStorageServers;
        args->index = index;
        args->socketFD = storageServerSocketFD;
        
        pthread_mutex_lock(&ssListLock);

        updateListStorageServers(storageServerInfo->clientPort, storageServerInfo->namingServerPort, storageServerInfo->networkIP, storageServerInfo->Paths);

        char dirPath[__BUFFER_SIZE__];
        // strcpy(dirPath,"storage");


        if(index==2){
            FILE_REQUEST_STRUCT request;
            strcpy(request.opType, "COPY");
            strcpy(request.path, "./storage");
            strcpy(request.destination_path, "./storage");
            request.isDirectory = true;
            
            // makeCopyRequest(&request,0,1,true, 0);
            makeCopy(&request,0,1,true, 0);
            printf("Storage Server 0 backed up in storage 1\n");
        }
        if(index>=2){
            FILE_REQUEST_STRUCT request;
            strcpy(request.opType, "COPY");
            strcpy(request.path, "./storage");
            strcpy(request.destination_path, "./storage");
 
            request.isDirectory = true;

            makeCopy(&request,index-1,index,true,index-1);
            strcpy(request.destination_path, "./storage");

            request.isDirectory = true;
            makeCopy(&request,index-2,index,true,index-2);
        }
        strcpy(dirPath,".");
        printf("Storage Server %d\n", index);
        recieveDirPathTrie(storageServerSocketFD, dirPath, numberOfStorageServers);
        strcpy(dirPath,"");
        listAllPathsinTries(root, dirPath);
        pthread_create(&threadforStorageServer, NULL, failureDetectionThread, (void*)args);
        numberOfStorageServers++;
        pthread_mutex_unlock(&ssListLock);
    }
    close(storageServerSocket);
    return NULL;
}



char* getParentPath(char* path){
    char* parentPath = addPrefix(path); 
    int len = strlen(parentPath);
    for(int i=len-1;i>=0;i--){
        if(parentPath[i] == '/'){
            parentPath[i] = '\0';
            return parentPath;
        }
    }
    return parentPath;
}
RETURN_DATA* getStorageServerDirectlyFromFile(TrieNode* root, char* path){
    return fetchValue(root, addPrefix(path),cache);
}

RETURN_DATA* getStorageServerName(TrieNode* root, char* path){
    char* parentPath = getParentPath(path);
    // char* storageServerName = fetchValue(root, addPrefix(parentPath),cache);
    RETURN_DATA* storageServerName = fetchValue(root, addPrefix(parentPath),cache);
    // return storageServerName->value;
    printf("Storage Server Name: %s\n", storageServerName->value);
    return storageServerName;
}

void namingServerSend(int socketFD, int dataType, void* messageStruct){
    if(dataType == __STRING__){
        if(send(socketFD, messageStruct, strlen((char*)messageStruct)+1, 0)==-1){
            perror("send() failed");
        }
    }
    else if(dataType == __FILE_REQUEST__){
        if(send(socketFD, messageStruct, sizeof(FILE_REQUEST_STRUCT), 0)==-1){
            perror("send() failed");
        }
    }
    if((FILE_REQUEST_STRUCT*)messageStruct != NULL && strcmp(((FILE_REQUEST_STRUCT*)messageStruct)->opType, "CREATE") == 0){
        logMessageSent(socketFD, dataType, &messageStruct, false, "CREATE", false);
    }
    else{
        logMessageSent(socketFD, dataType, &messageStruct, false, "SEND", false);
    }
    // logMessageSent(socketFD, dataType, &messageStruct, false, "SEND", false);
}
// int createBackup(int socketServer){
//     int status;
//     if(recv(socketServer, &status, sizeof(int), 0) < 0){
//         perror("Could not receive the response from SS for CREATE or DELETE");
//         return __SERVER_DOWN__;
//     }
//     if(status != __OK__){
//         perror("Could not CREATE RECOVERY FOLDER");
//         return status;
//     }
//     close(socketServer);
//     return status;
// }

void printCreateDeleteRequest(FILE_REQUEST_STRUCT* request, int status,bool isClient);
// function to send the request to the storage server directly for CREATE or DELETE
int sendToSSForDeleteAndCreate(FILE_REQUEST_STRUCT* request, int storageServerName, bool backup,int originalStorageServer){
    int socketServer = socket(AF_INET, SOCK_STREAM, 0);
    if(socketServer == -1){
        perror("socket() failed");
        return -1;
    }
    struct sockaddr_in serverAddress;
    memset(&serverAddress, '\0', sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(listStorgeServers[storageServerName].namingServerPort);
    serverAddress.sin_addr.s_addr = inet_addr(listStorgeServers[storageServerName].networkIP);
    printf("----------------- INFO -----------------\n");
    printf("Storage Server Name: %d\n", storageServerName);
    printf("Storage Server IP: %s\n", listStorgeServers[storageServerName].networkIP);
    printf("Storage Server Port: %d\n", listStorgeServers[storageServerName].namingServerPort);
    printf("Request: %s\n", request->opType);
    printf("Path: %s\n", request->path);
    printf("Backup: %d\n", backup);
    printf("---------------------------------------\n");
    
    if(connect(socketServer, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        perror("Connection error");
        return -1;
    }
    
    namingServerSend(socketServer, __FILE_REQUEST__, request);


    if(backup){
        int status;
        if(recv(socketServer, &status, sizeof(int), 0) < 0){
            perror("Could not receive the response from SS for CREATE or DELETE");
            return __SERVER_DOWN__;
        }
        if(status != __OK__){
            perror("Could not CREATE RECOVERY FOLDER");
            return status;
        }
        close(socketServer);
        return status;
    }
    int status;
    if(recv(socketServer,&status,sizeof(int),0) < 0){
        perror("Could not receive the response from SS for CREATE or DELETE");
        return __SERVER_DOWN__;
    }
    logMessageRecieve(socketServer, __STATUS__, (void**)&status, false);

    printCreateDeleteRequest(request,status,false);    


    if(status != __OK__){
        perror("Could not CREATE or DELETE");
        return status;
    }
    if(status == __OK__ && strcmp(request->opType, "DELETE") == 0){
        if(request->isDirectory){
            removeKey(root, addPrefix(request->path),cache,true);
        }
        else{
            removeKey(root, addPrefix(request->path),cache,false);
        }
    }
    if(status == __OK__ && strcmp(request->opType, "CREATE") == 0){
        char str[__BUFFER_SIZE__];
        sprintf(str, "%d", originalStorageServer);
        printf("Inserting into SSN: %s\n", str);
        root = trieInsert(root, addPrefix(request->path), str, request->isDirectory);
    }

    close(socketServer);
    return status;
}

int sendToSSForCopy(FILE_REQUEST_STRUCT* request, int srcStorageServer, int destStorageServer){
    int srcSocketServer = socket(AF_INET, SOCK_STREAM, 0);
    if(srcSocketServer == -1){
        perror("socket() failed");
        return -1;
    }

    struct sockaddr_in srcServerAddress;
    memset(&srcServerAddress, '\0', sizeof(srcServerAddress));
    srcServerAddress.sin_family = AF_INET;
    srcServerAddress.sin_port = htons(listStorgeServers[srcStorageServer].namingServerPort);
    srcServerAddress.sin_addr.s_addr = inet_addr(listStorgeServers[srcStorageServer].networkIP);

    if(connect(srcSocketServer, (struct sockaddr*)&srcServerAddress, sizeof(srcServerAddress)) < 0){
        perror("Connection error");
        return -1;
    }

    int destSocketServer = socket(AF_INET, SOCK_STREAM, 0);
    if(destSocketServer == -1){
        perror("socket() failed");
        return -1;
    }
    
    struct sockaddr_in destServerAddress;
    memset(&destServerAddress, '\0', sizeof(destServerAddress));
    destServerAddress.sin_family = AF_INET;
    destServerAddress.sin_port = htons(listStorgeServers[destStorageServer].namingServerPort);
    destServerAddress.sin_addr.s_addr = inet_addr(listStorgeServers[destStorageServer].networkIP);

    if(connect(destSocketServer, (struct sockaddr*)&destServerAddress, sizeof(destServerAddress)) < 0){
        perror("Connection error");
        return -1;
    }

    send(srcSocketServer, request, sizeof(FILE_REQUEST_STRUCT), 0);
    send(destSocketServer, request, sizeof(FILE_REQUEST_STRUCT), 0);

    char message[__BUFFER_SIZE__];
    strcpy(message,"INIT.SEND");
    send(srcSocketServer, message, strlen(message)+1, 0);
    strcpy(message,"INIT.RECEIVE");
    send(destSocketServer,message,sizeof(FILE_REQUEST_STRUCT),0);

    int bytesRecv;
    while(true){
        char pipe[__BUFFER_SIZE__];
        bytesRecv = recv(srcSocketServer,pipe,__BUFFER_SIZE__,0);
        if(bytesRecv<=0) break;
        if(strcmp(pipe,"TRANSFER_DONE")==0){
            break;
        }
        send(destSocketServer,pipe,bytesRecv,0);
    }
    close(srcSocketServer);
    close(destSocketServer);
}
// function to send the request to the storage server directly for READ or WRITE or GET
void printStorageServers(){
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Storage Servers\n"PINK"---------------------------------------\n"RESET);
    for(int i=0;i<numberOfStorageServers;i++){
        printf(BRIGHT_GREEN"[+] Storage Server #%d\n"RESET, i+1);
        printf(BRIGHT_CYAN"[Server Info] " YELLOW"IP: %s | Naming Server Port: %d | Client Port: %d\n"RESET, listStorgeServers[i].networkIP, listStorgeServers[i].namingServerPort, listStorgeServers[i].clientPort);
    }
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}
// function to print the clients
void printClients(){
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Clients\n"PINK"---------------------------------------\n"RESET);
    for(int i=0;i<numberOfClients;i++){
        printf(BRIGHT_GREEN"[+] Client #%d\n"RESET, i+1);
    }
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}
// function to print the recieved request
void printRequest(FILE_REQUEST_STRUCT* request){
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Request Recieved\n"PINK"---------------------------------------\n"RESET);
    printf(BRIGHT_GREEN"[+] Operation: %s\n"RESET, request->opType);
    printf(BRIGHT_GREEN"[+] Path: %s\n"RESET, request->path);
    printf(BRIGHT_GREEN"[+] Destination Path: %s\n"RESET, request->destination_path);
    printf(BRIGHT_GREEN"[+] Directory: %d\n"RESET, request->isDirectory);
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}
// function to send the request to the storage server directly for READ or WRITE or GET
void printServerInfo(SERVER_CONFIG* server, char* opType,int storageServerName,bool isNull){
    printf(YELLOW"Served Request: %s\n"RESET, opType);
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Sent Server Info\n"PINK"---------------------------------------\n"RESET);
    if(!isNull) printf(BRIGHT_GREEN"[+] Storage Server Name: %d\n"RESET, storageServerName);
    else printf(BRIGHT_GREEN"[+] Storage Server Name: (NULL)\n"RESET);
    printf(BRIGHT_GREEN"[+] Server Port: %d\n"RESET, server->portNumber);
    printf(BRIGHT_GREEN"[+] Server IP: %s\n"RESET, server->networkIP);
    printf(BRIGHT_GREEN"[+] Error Code: %d\n"RESET, server->errorCode);
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}

void printServedListRequest(FILE_REQUEST_STRUCT* request, char* finalList){
    printf(YELLOW"Request Served: %s\n"RESET, request->opType);
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Sent List\n"PINK"---------------------------------------\n"RESET);
    printf(BRIGHT_GREEN"[+] List of all paths sent to the client\n"RESET);
    printf(BRIGHT_GREEN"[+] List: %s\n"RESET, finalList);
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}
void printCreateDeleteRequest(FILE_REQUEST_STRUCT* request, int status,bool isClient){
    if(isClient){
        printf(YELLOW"Request Served: %s\n"RESET, request->opType);
        printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Sent Status\n"PINK"---------------------------------------\n"RESET);
    }
    else{
        printf(YELLOW"Acknowledgement Recieved: %s\n"RESET, request->opType);
        printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Recieved Status\n"PINK"---------------------------------------\n"RESET);
    }
    if(status == __OK__) printf(BRIGHT_GREEN"[+] Status: OK\n"RESET);
    else printf(BRIGHT_GREEN"[+] Status: %d\n"RESET, status);
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}

// // Function to strip ANSI escape sequences from a string
// void removeANSISequences(char* str) {
//     char* src = str;
//     char* dst = str;

//     while (*src) {
//         // Check for the start of an ANSI escape sequence
//         if (*src == '\033' && *(src + 1) == '[') {
//             // Skip the escape sequence by moving src past it
//             src += 2; // Skip '\033['
//             while (*src && *src != 'm') {
//                 src++; // Skip characters until 'm'
//             }
//             if (*src == 'm') {
//                 src++; // Skip the 'm' character
//             }
//         } else {
//             *dst++ = *src++; // Copy valid characters to destination
//         }
//     }
//     *dst = '\0'; // Null-terminate the string
// }

#include <stdbool.h>
#include <string.h>

// Function to strip ANSI escape sequences from a string
void removeANSISequences(char* str) {
    char* src = str; // Pointer to traverse the original string
    char* dst = str; // Pointer to write the cleaned string

    while (*src) {
        // Check for the start of an ANSI escape sequence
        if (*src == '\033' && *(src + 1) == '[') {
            // Skip the escape sequence
            src += 2; // Skip '\033['
            while (*src && !(*src >= 'A' && *src <= 'Z') && !(*src >= 'a' && *src <= 'z')) {
                src++; // Skip characters until the final letter of the escape sequence
            }
            if (*src) {
                src++; // Skip the final letter (A-Z or a-z)
            }
        } else {
            // Copy valid characters to the destination
            *dst++ = *src++;
        }
    }
    *dst = '\0'; // Null-terminate the cleaned string
}

// function to send the request to the storage server directly for READ or WRITE or GET
int sendReadRequestToServer(int socketServer, FILE_REQUEST_STRUCT* request,char* buffer){
    namingServerSend(socketServer, __FILE_REQUEST__, request);
    // recieve the file content from the storage server in single go
    PACKET packet;
    int bytesReceived = recv(socketServer, &packet, sizeof(PACKET), 0);
    memcpy(buffer, packet.data, packet.length);
    PACKET stop;
    int status1 = recv(socketServer, &stop, sizeof(PACKET), 0);
    if(stop.stop==1){
        printf("Stop\n");
    }


    // int bytesReceived = recv(socketServer, buffer, __BUFFER_SIZE__, 0);
    if(bytesReceived <= 0){
        perror("recv() failed");
        return __SERVER_DOWN__;
    }
    int status;
    if(recv(socketServer, &status, sizeof(int), 0) < 0){
        perror("Could not receive the response from SS for READ or WRITE");
        return __SERVER_DOWN__;
    }
    if(status != __OK__){
        perror("Could not READ");
        return status;
    }

    memcpy(buffer, packet.data, packet.length);
    logMessageRecieve(socketServer, __STRING__, (void**)&buffer, false);
    // print on temrinal the request was served
    
    printf("Received: %s\n", buffer);
    return __OK__;
}

int sendWriteRequestToServer(int socketServer, FILE_REQUEST_STRUCT* request, char* buffer){
    namingServerSend(socketServer, __FILE_REQUEST__, request);
    PACKET packet;
    packet.length = strlen(buffer);
    memcpy(packet.data, buffer, strlen(buffer));
    send(socketServer, &packet, sizeof(PACKET), 0);

    PACKET stop;
    stop.stop = 1;
    stop.length = 0;
    send(socketServer, &stop, sizeof(PACKET), 0);
    
    // send(socketServer, buffer, strlen(buffer)+1, 0);
    int status;
    if(recv(socketServer, &status, sizeof(int), 0) < 0){
        perror("Could not receive the response from SS for WRITE");
        return __SERVER_DOWN__;
    }
    if(status != __OK__){
        perror("Could not WRITE");
        return status;
    }
    logMessageRecieve(socketServer, __STATUS__, (void**)&status, false);
    printf("Write Successful\n");
    return __OK__;
}

int createSocketAndConnect(int storageServerName);
void getFileNameFromPath(char* path, char* fileName);
int readPipeToWrite(int readSocket,int writeSocket,int backupSocket1,int backupSocket2);
////////////////////////////////////////////////////////////////////////////////////////
int directoryHelper(char* path, char* baseDir,FILE_REQUEST_STRUCT* request,int storageServerName,bool isBackup,int destStorageServer,int originalStorageServer){
    FILE_REQUEST_STRUCT createRequest = *request;
    strcpy(createRequest.opType, "CREATE");
    // path -> destination_path+(basedir-./storage)+path
    strcpy(createRequest.path, request->destination_path); // 
    removeANSISequences(path);
    if(strcmp(path,"")==0){
        return __OK__;
    }
    strcat(createRequest.path, path);
    printf("Path: %s\n", createRequest.path);
    createRequest.isDirectory = true;
    // send the request to the storage server in destStorageServer
    int status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer, isBackup,originalStorageServer);
    printCreateDeleteRequest(&createRequest, status,true);
    return status;
}

int fileHelper(char* path, char* baseDir,FILE_REQUEST_STRUCT* request,int storageServerName,bool isBackup,int destStorageServer,int originalStorageServer){
    FILE_REQUEST_STRUCT createRequest = *request;
    strcpy(createRequest.opType, "CREATE");
    strcpy(createRequest.path, request->destination_path); //

    removeANSISequences(path);
    strcat(createRequest.path, path);
    printf("Path: %s\n", createRequest.path);
    createRequest.isDirectory = false;
    // send the request to the storage server in destStorageServer
    int status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer, isBackup,originalStorageServer);
    printCreateDeleteRequest(&createRequest, status,true);
    if(status != __OK__){
        return status;
    }
    ////////////////////////////////////////////////////////////////
    // send the read request to the original storage server
    // int readSocketServer = createSocketAndConnect(storageServerName);
    // FILE_REQUEST_STRUCT readRequest = *request;
    // strcpy(readRequest.opType, "READ");
    // strcpy(readRequest.path, baseDir); // "./storage" + path
    // strcat(readRequest.path, path);
    // // strcpy(readRequest.path, );
    // char pipe[__BUFFER_SIZE__];
    // status = sendReadRequestToServer(readSocketServer, &readRequest, pipe);
    // if(status != __OK__){
    //     return status;
    // }
    // close(readSocketServer);
    // printf("Read Path: %s\n", readRequest.path);
    // ////////////////////////////////////////////////////////////////
    // // send the write request to the new storage server
    // int writeSocketServer = createSocketAndConnect(destStorageServer);
    // FILE_REQUEST_STRUCT writeRequest = *request;
    // strcpy(writeRequest.opType, "WRITE");
    // strcpy(writeRequest.path, createRequest.path);
    // printf("Write Path: %s\n", writeRequest.path);
    // status = sendWriteRequestToServer(writeSocketServer, &writeRequest, pipe);
    // if(status != __OK__){
    //     return status;
    // }
    // close(writeSocketServer);
    printf("Read Path: %s\n", createRequest.path);

    int readSocketServer = createSocketAndConnect(storageServerName);
    int writeSocketServer = createSocketAndConnect(destStorageServer);
    FILE_REQUEST_STRUCT readRequest = *request;
    FILE_REQUEST_STRUCT writeRequest = *request;
    strcpy(readRequest.opType, "READ");
    strcpy(readRequest.path, baseDir); // "./storage" + path
    strcat(readRequest.path, path);

    char pipe[__BUFFER_SIZE__];
    if(send(readSocketServer, &readRequest, sizeof(FILE_REQUEST_STRUCT), 0) == -1){
        perror("send() failed");
        return __FAILURE__;
    }
    printf("Read Request Sent\n");
    strcpy(writeRequest.opType, "WRITE");
    strcpy(writeRequest.path, createRequest.path);


    if(send(writeSocketServer, &writeRequest, sizeof(FILE_REQUEST_STRUCT), 0) == -1){
        perror("send() failed");
        return __FAILURE__;
    }
    printf("Write Request Sent\n");

    readPipeToWrite(readSocketServer,writeSocketServer,-1,-1);
    close(readSocketServer);
    close(writeSocketServer);
    return __OK__;
}

int tokenizeAndPrintPaths(char* finalList,char* baseDir,FILE_REQUEST_STRUCT* request,int storageServerName,bool isBackup,int destStorageServer,int originalStorageServer){
    char* saveptr;            // Save pointer for strtok_r
    char* token = strtok_r(finalList, "\n", &saveptr); // Use '\n' as the delimiter

    while (token != NULL) {
        // check if token contains BRIGHT_CYAN then its a directory
        if(strstr(token, "\033[0;36m") != NULL){
            printf(YELLOW"----- Directory: %s -----\n"RESET, token);

            int status = directoryHelper(token, baseDir,request,storageServerName,isBackup,destStorageServer,originalStorageServer);
            if(status != __OK__){
                return status;
            }
        }
        else{
            printf(YELLOW"----- File: %s -----\n"RESET, token);
            int status = fileHelper(token, baseDir,request,storageServerName,isBackup,destStorageServer,originalStorageServer);
            if(status != __OK__){
                return status;
            }
        }
        token = strtok_r(NULL, "\n", &saveptr); // Continue tokenizing
    }
    return __OK__;
}

int readPipeToWritePipe(int readSocket,int writeSocket,int backupSocket1,int backupSocket2,FILE_REQUEST_STRUCT* readRequest,FILE_REQUEST_STRUCT* writeRequest){
    if(readSocket==-1){
        return __FAILURE__;
    }
    send(readSocket, readRequest, sizeof(FILE_REQUEST_STRUCT), 0);
    // printf("Read Request Sent\n");
    
    // usleep(100000);
    // sleep(1);
    
    logMessageSent(readSocket, __FILE_REQUEST__,(void**) &readRequest, false, "READ", false);
    if(writeSocket!=-1){
        send(writeSocket, writeRequest, sizeof(FILE_REQUEST_STRUCT), 0);
        logMessageSent(writeSocket, __FILE_REQUEST__, (void**)&writeRequest, false, "WRITE", false);
        // printf("Write Request Sent\n");
    }
    if(backupSocket1!=-1){
        send(backupSocket1, readRequest, sizeof(FILE_REQUEST_STRUCT), 0);
        logMessageRecieve(backupSocket1, __FILE_REQUEST__,(void**) &readRequest, false);
    }
    if(backupSocket2!=-1){
        send(backupSocket2, readRequest, sizeof(FILE_REQUEST_STRUCT), 0);
        logMessageSent(backupSocket2, __FILE_REQUEST__, (void**)&readRequest, false, "WRITE", false);
    }
    if(writeSocket==-1 && backupSocket1==-1 && backupSocket2==-1){
        return __FAILURE__;
    }
    int status = readPipeToWrite(readSocket,writeSocket,backupSocket1,backupSocket2);
    return status;
}

// int readPipeToWrite(int readSocket, int writeSocket, int backupSocket1, int backupSocket2) {
//     const size_t BUFFER_SIZE = 8192;
//     char* pipe = (char*)malloc(BUFFER_SIZE);
//     if (!pipe) {
//         printf("[DEBUG] Memory allocation failed!\n");
//         return __FAILURE__;
//     }

//     // Debug counters
//     int totalBytesReceived = 0;
//     int totalBytesSent = 0;
//     int transferCount = 0;

//     printf("\n[DEBUG] Starting data transfer...\n");
//     printf("[DEBUG] Read Socket: %d\n", readSocket);
//     printf("[DEBUG] Write Socket: %d\n", writeSocket);
//     printf("[DEBUG] Backup Socket 1: %d\n", backupSocket1);
//     printf("[DEBUG] Backup Socket 2: %d\n", backupSocket2);
//     int bytesRecv;
//     while (true) {
//         // Clear buffer before receiving
//         memset(pipe, 0, BUFFER_SIZE);
        
//         // Read data with debug info
//         printf("\n[DEBUG] Waiting to receive data...\n");
//         bytesRecv = recv(readSocket, pipe, BUFFER_SIZE - 1, 0);
        
//         if (bytesRecv < 0) {
//             printf("[DEBUG] Error receiving data! errno: %d\n", errno);
//             perror("[DEBUG] Receive error");
//             free(pipe);
//             return __FAILURE__;
//         } else if (bytesRecv == 0) {
//             printf("[DEBUG] Connection closed by server\n");
//             free(pipe);
//             return __OK__;
//         }

//         pipe[bytesRecv] = '\0';
//         totalBytesReceived += bytesRecv;
//         transferCount++;

//         printf("\n[DEBUG] Transfer #%d\n", transferCount);
//         printf("[DEBUG] Bytes received: %d\n", bytesRecv);
//         printf("[DEBUG] First 50 bytes of data: %.50s\n", pipe);
//         printf("[DEBUG] Total bytes received so far: %d\n", totalBytesReceived);

//         logMessageRecieve(readSocket, __STRING__, (void**)&pipe, false);

//         // Send to write socket if available
//         if (writeSocket != -1) {
//             printf("[DEBUG] Sending to write socket %d\n", writeSocket);
//             int sentBytes = send(writeSocket, pipe, bytesRecv, 0);
//             if (sentBytes != bytesRecv) {
//                 printf("[DEBUG] Write socket send failed! Sent %d of %d bytes\n", 
//                        sentBytes, bytesRecv);
//                 perror("[DEBUG] Send error");
//                 free(pipe);
//                 return __FAILURE__;
//             }
//             totalBytesSent += sentBytes;
//             printf("[DEBUG] Successfully sent %d bytes to write socket\n", sentBytes);
//             logMessageSent(writeSocket, __STRING__, (void**)&pipe, false, "WRITE", false);
//         }

//         // Backup socket 1
//         if (backupSocket1 != -1) {
//             printf("[DEBUG] Sending to backup socket 1: %d\n", backupSocket1);
//             int sentBytes = send(backupSocket1, pipe, bytesRecv, 0);
//             if (sentBytes != bytesRecv) {
//                 printf("[DEBUG] Backup1 send failed! Sent %d of %d bytes\n", 
//                        sentBytes, bytesRecv);
//                 free(pipe);
//                 return __FAILURE__;
//             }
//             printf("[DEBUG] Successfully sent %d bytes to backup1\n", sentBytes);
//             logMessageSent(backupSocket1, __STRING__, (void**)&pipe, false, "WRITE", false);
//         }

//         // Backup socket 2
//         if (backupSocket2 != -1) {
//             printf("[DEBUG] Sending to backup socket 2: %d\n", backupSocket2);
//             int sentBytes = send(backupSocket2, pipe, bytesRecv, 0);
//             if (sentBytes != bytesRecv) {
//                 printf("[DEBUG] Backup2 send failed! Sent %d of %d bytes\n", 
//                        sentBytes, bytesRecv);
//                 free(pipe);
//                 return __FAILURE__;
//             }
//             printf("[DEBUG] Successfully sent %d bytes to backup2\n", sentBytes);
//             logMessageSent(backupSocket2, __STRING__, (void**)&pipe, false, "WRITE", false);
//         }

//         if (strstr(pipe, "__TRANSFER_DONE__") != NULL) {
//             printf("\n[DEBUG] Transfer complete!\n");
//             printf("[DEBUG] Total transfers: %d\n", transferCount);
//             printf("[DEBUG] Total bytes received: %d\n", totalBytesReceived);
//             printf("[DEBUG] Total bytes sent: %d\n", totalBytesSent);
//             free(pipe);
//             return __OK__;
//         }
//     }
// }

// int readPipeToWritePipe(int readSocket, int writeSocket, int backupSocket1, int backupSocket2,
//                        FILE_REQUEST_STRUCT* readRequest, FILE_REQUEST_STRUCT* writeRequest) {
//     printf("\n[DEBUG] Starting file transfer process...\n");
    
//     if (readSocket == -1) {
//         printf("[DEBUG] Invalid read socket!\n");
//         return __FAILURE__;
//     }

//     // Print request details
//     printf("\n[DEBUG] Read Request Details:\n");
//     printf("Operation Type: %s\n", readRequest->opType);
//     printf("Path: %s\n", readRequest->path);

//     // Send read request with debug info
//     printf("[DEBUG] Sending read request to socket %d\n", readSocket);
//     int sent = send(readSocket, readRequest, sizeof(FILE_REQUEST_STRUCT), 0);
//     if (sent != sizeof(FILE_REQUEST_STRUCT)) {
//         printf("[DEBUG] Failed to send read request! Sent %d of %lu bytes\n", 
//                sent, sizeof(FILE_REQUEST_STRUCT));
//         return __FAILURE__;
//     }
//     sleep(1);
//     printf("[DEBUG] Read request sent successfully\n");
//     logMessageSent(readSocket, __FILE_REQUEST__, (void**)&readRequest, false, "READ", false);

//     // Print write request details
//     printf("\n[DEBUG] Write Request Details:\n");
//     printf("Operation Type: %s\n", writeRequest->opType);
//     printf("Path: %s\n", writeRequest->path);

//     // Send write requests with debug info
//     if (writeSocket != -1) {
//         printf("[DEBUG] Sending write request to primary socket %d\n", writeSocket);
//         sent = send(writeSocket, writeRequest, sizeof(FILE_REQUEST_STRUCT), 0);
//         if (sent != sizeof(FILE_REQUEST_STRUCT)) {
//             printf("[DEBUG] Failed to send write request! Sent %d of %lu bytes\n", 
//                    sent, sizeof(FILE_REQUEST_STRUCT));
//             return __FAILURE__;
//         }
//         printf("[DEBUG] Write request sent to primary socket\n");
//         logMessageSent(writeSocket, __FILE_REQUEST__, (void**)&writeRequest, false, "WRITE", false);
//     }

//     // Similar debug for backup sockets...
//     if (backupSocket1 != -1) {
//         printf("[DEBUG] Sending write request to backup1 socket %d\n", backupSocket1);
//         // ... (similar debug code)
//     }
    
//     if (backupSocket2 != -1) {
//         printf("[DEBUG] Sending write request to backup2 socket %d\n", backupSocket2);
//         // ... (similar debug code)
//     }

//     // Check destination validity
//     if (writeSocket == -1 && backupSocket1 == -1 && backupSocket2 == -1) {
//         printf("[DEBUG] No valid destination sockets!\n");
//         return __FAILURE__;
//     }

//     printf("[DEBUG] Waiting for servers to process requests...\n");
//     usleep(100000);  // 100ms delay
    
//     printf("[DEBUG] Starting data transfer...\n");
//     int status = readPipeToWrite(readSocket, writeSocket, backupSocket1, backupSocket2);
    
//     printf("[DEBUG] Transfer completed with status: %d\n", status);
//     return status;
// }
int send_full(int socket_fd, const void *data, size_t size);
int recv_full(int sock_fd, void* buffer, size_t size);

int readPipeToWrite(int readSocket,int writeSocket,int backupSocket1,int backupSocket2){
    int bytesRecv;
    int status = __FAILURE__;
    PACKET packet;
    // printf("Hello\n");
    if(backupSocket1==-1 && backupSocket2 == -1){
        // printf("Hello\n");
        // char* pipe = (char*)malloc(8192);
        int bytesRecv;
        while (true){
            int bytesRecv = recv_full(readSocket, &packet, sizeof(PACKET));
            // logMessageRecieve(readSocket, __STRING__, (void**)&packet.data, false);
            if(bytesRecv <= 0) break;

            printf("Packet Length: %d\n", packet.length);
            send_full(writeSocket, &packet, sizeof(PACKET));
            // logMessageSent(writeSocket, __STRING__, (void**)&packet.data, false, "WRITE", false);
            if(packet.stop == 1){
                break;
            }

            // bytesRecv = recv(readSocket, pipe, sizeof(pipe) - 1, 0);
            // logMessageRecieve(readSocket, __STRING__, (void**)&pipe, false);
            // if (bytesRecv <= 0) break;
            // pipe[bytesRecv] = '\0';
            // send(writeSocket, pipe, bytesRecv, 0);
            // logMessageSent(writeSocket, __STRING__, (void**)&pipe, false, "WRITE", false);
            // printf("%s\n", pipe);
            // if (strstr(pipe, "__TRANSFER_DONE__") != NULL){
            //     break;
            // }
            // memset(pipe, 0, sizeof(pipe));
        }
        // free(pipe);
        
        // printf("Transfer Done\n");
        return __OK__;
    }
    else if(backupSocket1==-1){
        // char* pipe = (char*)malloc(8192);
        while(true){
            // bytesRecv = recv(readSocket,pipe,sizeof(pipe)-1,0);
            // logMessageRecieve(readSocket, __STRING__, (void**)&pipe, false);
            // if(bytesRecv<=0) break;
            // send(writeSocket,pipe,bytesRecv,0);
            // logMessageSent(writeSocket, __STRING__, (void**)&pipe, false, "WRITE", false);
            // send(backupSocket2,pipe,bytesRecv,0);
            // logMessageSent(backupSocket2, __STRING__, (void**)&pipe, false, "WRITE", false);
            // if(strcmp(pipe,"TRANSFER_DONE")==0){
            //     break;
            // }
            // memset(pipe,0,sizeof(pipe));
            int bytesRecv = recv(readSocket, &packet, sizeof(PACKET), 0);
            // logMessageRecieve(readSocket, __STRING__, (void**)&packet.data, false);
            if(bytesRecv <= 0) break;
            send(writeSocket, &packet, sizeof(PACKET), 0);
            // logMessageSent(writeSocket, __STRING__, (void**)&packet.data, false, "WRITE", false);
            send(backupSocket2, &packet, sizeof(PACKET), 0);
            // logMessageSent(backupSocket2, __STRING__, (void**)&packet.data, false, "WRITE", false);
            if(packet.stop == 1){
                break;
            }
        }
        // free(pipe);
        // printf("Transfer Done\n");
        return __OK__;
    }
    else if(backupSocket2==-1){
        // char* pipe = (char*)malloc(8192);
        while(true){
            // bytesRecv = recv(readSocket,pipe,sizeof(pipe)-1,0);
            // logMessageRecieve(readSocket, __STRING__, (void**)&pipe, false);
            // if(bytesRecv<=0) break;
            // send(writeSocket,pipe,bytesRecv,0);
            // logMessageSent(writeSocket, __STRING__, (void**)&pipe, false, "WRITE", false);
            // send(backupSocket1,pipe,bytesRecv,0);
            // logMessageSent(backupSocket1, __STRING__, (void**)&pipe, false, "WRITE", false);
            // if(strstr(pipe,"__TRANSFER_DONE__")!=NULL){
            //     break;
            // }
            // memset(pipe,0,sizeof(pipe));
            int bytesRecv = recv(readSocket, &packet, sizeof(PACKET), 0);
            // logMessageRecieve(readSocket, __STRING__, (void**)&packet.data, false);
            if(bytesRecv <= 0) break;
            send(writeSocket, &packet, sizeof(PACKET), 0);
            // logMessageSent(writeSocket, __STRING__, (void**)&packet.data, false, "WRITE", false);
            send(backupSocket1, &packet, sizeof(PACKET), 0);
            // logMessageSent(backupSocket1, __STRING__, (void**)&packet.data, false, "WRITE", false);
            if(packet.stop == 1){
                break;
            }
        }
        // free(pipe);
        // printf("Transfer Done\n");
        return __OK__;
    }
    // char* pipe = (char*)malloc(8192);
    while(true){
        // bytesRecv = recv(readSocket,pipe,sizeof(pipe)-1,0);
        // logMessageRecieve(readSocket, __STRING__, (void**)&pipe, false);
        // if(bytesRecv<=0) break;
        // send(writeSocket,pipe,bytesRecv,0);
        // logMessageSent(writeSocket, __STRING__, (void**)&pipe, false, "WRITE", false);
        // send(backupSocket1,pipe,bytesRecv,0);
        // logMessageSent(backupSocket1, __STRING__, (void**)&pipe, false, "WRITE", false);
        // send(backupSocket2,pipe,bytesRecv,0);
        // logMessageSent(backupSocket2, __STRING__, (void**)&pipe, false, "WRITE", false);
        // // printf("Pipe: %s\n", pipe);
        // if(strstr(pipe,"__TRANSFER_DONE__")!=NULL){
        //     break;
        // }
        // memset(pipe,0,sizeof(pipe));
        int bytesRecv = recv(readSocket, &packet, sizeof(PACKET), 0);
        // logMessageRecieve(readSocket, __STRING__, (void**)&packet.data, false);
        if(bytesRecv <= 0) break;
        send(writeSocket, &packet, sizeof(PACKET), 0);
        // logMessageSent(writeSocket, __STRING__, (void**)&packet.data, false, "WRITE", false);
        send(backupSocket1, &packet, sizeof(PACKET), 0);
        // logMessageSent(backupSocket1, __STRING__, (void**)&packet.data, false, "WRITE", false);
        send(backupSocket2, &packet, sizeof(PACKET), 0);
        // logMessageSent(backupSocket2, __STRING__, (void**)&packet.data, false, "WRITE", false);
        if(packet.stop == 1){
                break;
        }
    }
    // free(pipe);
    // printf("Transfer Done\n");
    return __OK__;
}

int makeCopyRequest(FILE_REQUEST_STRUCT* request, int storageServerName,int destStorageServer,bool isBackup,int originalStorageServer){
    printf("----------------- SRC INFO -----------------\n");
    printf("Storage Server Name: %d\n", storageServerName);
    printf("Storage Server IP: %s\n", listStorgeServers[storageServerName].networkIP);
    printf("Storage Server Port: %d\n", listStorgeServers[storageServerName].namingServerPort);
    printf("----------------- DEST INFO -----------------\n");
    printf("Storage Server Name: %d\n", destStorageServer);
    printf("Storage Server IP: %s\n", listStorgeServers[destStorageServer].networkIP);
    printf("Storage Server Port: %d\n", listStorgeServers[destStorageServer].namingServerPort);


    char path[__BUFFER_SIZE__] = "";
    printf("Request Path: %s\n", request->path);
    // char* finalList = listAllDirectoriesForCopy(request->path, root, path,storageServerName);
    char* finalList = listAllDirectoriesForCopy(request->path, root, path,originalStorageServer);
    
    // THE GIVEN PATH CONTAINS CONCACTENATED PATHS PRINT THEM CORRECTLY
    printf("Path: %s",finalList);
    tokenizeAndPrintPaths(finalList, request->path,request,storageServerName,isBackup,destStorageServer,originalStorageServer);

    return __OK__;
}

void getFileNameFromPath(char* path, char* fileName){
    int len = strlen(path);
    for(int i=len-1;i>=0;i--){
        if(path[i] == '/'){
            strcpy(fileName, path+i+1);
            return;
        }
    }
}

int createSocketForAck(int destStorageServer){
    int writeSocketServer = socket(AF_INET, SOCK_STREAM, 0);
    if(writeSocketServer == -1){
        perror("socket() failed");
        return -1;
    }
    struct sockaddr_in serverAddress2;
    memset(&serverAddress2, '\0', sizeof(serverAddress2));
    serverAddress2.sin_family = AF_INET;
    serverAddress2.sin_port = htons(listStorgeServers[destStorageServer].namingServerPort);

    serverAddress2.sin_addr.s_addr = inet_addr(listStorgeServers[destStorageServer].networkIP);

    if(connect(writeSocketServer, (struct sockaddr*)&serverAddress2, sizeof(serverAddress2)) < 0){
        perror("Connection error");
        return -1;
    }

    return writeSocketServer;
}


int createSocketAndConnect(int destStorageServer) {
    int writeSocketServer = socket(AF_INET, SOCK_STREAM, 0);
    if (writeSocketServer == -1) {
        perror("socket() failed");
        return -1;  // Return error code
    }

    struct sockaddr_in serverAddress2;
    memset(&serverAddress2, '\0', sizeof(serverAddress2));
    serverAddress2.sin_family = AF_INET;
    serverAddress2.sin_port = htons(listStorgeServers[destStorageServer].namingServerPort);
    
    // Convert the server IP address to network format
    serverAddress2.sin_addr.s_addr = inet_addr(listStorgeServers[destStorageServer].networkIP);

    // Connect to the server
    if (connect(writeSocketServer, (struct sockaddr*)&serverAddress2, sizeof(serverAddress2)) < 0) {
        perror("Connection error");
        return -1;  // Return error code
        // sleep(5);
    }

    return writeSocketServer;  // Return the connected socket
} 

void printbackupDetails(int backup1, int backup2,bool isRedirectedTo1,bool isRedirectedTo2) {
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Backup Details\n"PINK"---------------------------------------\n"RESET);
    if(isRedirectedTo1){
        printf(BRIGHT_GREEN"[+] Redirected to Backup 1\n"RESET);
    }
    if(isRedirectedTo2){
        printf(BRIGHT_GREEN"[+] Redirected to Backup 2\n"RESET);
    }
    printf(BRIGHT_GREEN"[+] Backup 1 Status: %d\n"RESET, backup1);
    printf(BRIGHT_GREEN"[+] Backup 2 Status: %d\n"RESET, backup2);
    printf(BRIGHT_CYAN"=======================================\n\n"RESET);
}
void sendCreateDeleteCopyStatus(int newClientSocket, int status, FILE_REQUEST_STRUCT* request) {
    if(send(newClientSocket, &status, sizeof(int), 0) <= 0){
        printf(BRIGHT_RED"[-] Client %d disconnected9\n"RESET, numberOfClients);
        return;
    }
    // printCreateDeleteRequest(request, status,true);
    // logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
    printf(BRIGHT_RED"[-] Client %d disconnected16\n"RESET, numberOfClients);
}

int send_full(int socket_fd, const void *data, size_t size) {
    size_t bytes_sent = 0;   // Track how many bytes have been sent
    const char *data_ptr = (const char *)data; // Pointer to the data

    while (bytes_sent < size) {
        ssize_t result = send(socket_fd, data_ptr + bytes_sent, size - bytes_sent, 0);
        if (result < 0) {
            perror("Error while sending data");
            return -1; // Return error if send fails
        }
        bytes_sent += result; // Update the count of bytes sent
    }
    printf("[+] Sent %ld bytes\n", bytes_sent);

    return 0; // Success
}
// Function to read exactly `size` bytes
int recv_full(int sock_fd, void* buffer, size_t size) {
    size_t total_received = 0;
    char* ptr = (char*)buffer;

    while (total_received < size) {
        int bytes_received = recv(sock_fd, ptr + total_received, size - total_received, MSG_WAITALL);
        if (bytes_received == -1) {
            return -1; // Error occurred
        } else if (bytes_received == 0) {
            // Connection closed by the server
            break;
        }
        total_received += bytes_received;
    }

    return total_received; // Return total bytes read
}

WriteBuffer *create_write_buffer_node(const char *data, size_t dataSize, const char *filePath, int clientSocket)
{
    WriteBuffer *newNode = (WriteBuffer *)malloc(sizeof(WriteBuffer));
    if (!newNode)
    {
        perror("Failed to allocate memory for WriteBuffer node");
        return NULL;
    }

    strncpy(newNode->path, filePath, sizeof(newNode->path) - 1);
    newNode->path[sizeof(newNode->path) - 1] = '\0';
    newNode->data = (char *)malloc(dataSize);
    if (!newNode->data)
    {
        perror("Failed to allocate memory for data");
        free(newNode);
        return NULL;
    }
    memcpy(newNode->data, data, dataSize);
    newNode->dataSize = dataSize;
    newNode->clientSocket = clientSocket;
    newNode->next = NULL;

    return newNode;
}

void free_write_buffer(WriteBuffer *head) {
    while (head) {
        WriteBuffer *temp = head;
        head = head->next;
        free(temp->data);
        free(temp);
    }
}


int write_file(int clientSocket, const char *filePath, int readSocket, int writeSocket, int backupSocket1, int backupSocket2,FILE_REQUEST_STRUCT* request) {
    PACKET packet;
    WriteBuffer *head = NULL;
    WriteBuffer *tail = NULL;
    bool isLarge = false;
    int totalByteRecv = 0;

    // Receive data from client in PACKETs and store it in linked list
    while (1) {
        // Receive the PACKET structure fully
        // printf("Chud gaye\n");
        ssize_t bytesReceived = recv_full(readSocket, &packet, sizeof(PACKET));
        if (bytesReceived <= 0) {
            perror("Error receiving data from client");
            return __FAILURE__;
        }
        totalByteRecv += packet.length;
        // printf("hello\n");

        // Create a new WriteBuffer node for each PACKET received
        WriteBuffer *newNode = create_write_buffer_node(packet.data, packet.length,filePath,readSocket);
        if (!newNode) {
            free_write_buffer(head);
            return 51;
        }

        // Append new node to the linked list
        if (tail) {
            tail->next = newNode;
        } else {
            head = newNode; // First node
        }
        tail = newNode; // Move the tail to the new node

        printf("[+] Received %d bytes\n", packet.length);


        // Stop receiving if the stop flag is set
        printf("STOPPED : %d\n",packet.stop);
        if (packet.stop == 1) {
            printf("STOPPED\n");
            break;
        }
    }


    // send write request to the storage server
    send(writeSocket, request, sizeof(FILE_REQUEST_STRUCT), 0);
    // Traverse the linked list and write the data to the file
    WriteBuffer *currentNode = head;
    int totalBytesWritten = 0;
    while (currentNode) {
        packet.length = currentNode->dataSize;
        // printf("Data: %s\n",currentNode->data);
        packet.stop = 0;
        strncpy(packet.data, currentNode->data, sizeof(packet.data) - 1);
        packet.data[sizeof(packet.data) - 1] = '\0';  // Null-terminate the string

        // printf("WRITING  FILE\n");
        int bytesWritten = send_full(writeSocket, &packet, sizeof(PACKET));
        if (bytesWritten == -1) {
            perror("Error writing data to file");
            return 51;
        }
        totalBytesWritten += bytesWritten;

        // Print status of bytes written
        // printf("[+] Wrote %d bytes to file\n", bytesWritten);
        // Move to the next node in the list
        // WriteBuffer *temp = currentNode;
        currentNode = currentNode->next;
        // free(temp->data);
        // free(temp);
    }
    // send the stop packet
    packet.stop = 1;
    packet.length = 0;
    int bytesWritten = send_full(writeSocket, &packet, sizeof(PACKET));


    if (bytesWritten == -1) {
        perror("Error writing data to file");
         while(head){
            WriteBuffer* temp = head;
            head = head->next;
            free(temp->data);
            free(temp);
        }
        return 51;
    }
    printf("[+] Total %d bytes written to file %s\n", totalBytesWritten, request->path);

    if(backupSocket1!=-1){
        sendReadRequestToServer(backupSocket1, request, NULL);
        currentNode = head;
        while(currentNode){
            packet.length = currentNode->dataSize;
            packet.stop = 0;
            // strcpy(packet.data, currentNode->data);
            strncpy(packet.data, currentNode->data, sizeof(packet.data) - 1);
            packet.data[sizeof(packet.data) - 1] = '\0';  // Null-terminate the string
            int bytesWritten = send_full(backupSocket1, &packet, sizeof(PACKET));
            if (bytesWritten == -1) {
                perror("Error writing data to file");
                return 51;
            }
            totalBytesWritten += bytesWritten;
            currentNode = currentNode->next;
        }
        packet.stop = 1;
        packet.length = 0;
        int bytesWritten = send_full(writeSocket, &packet, sizeof(PACKET));
        if (bytesWritten == -1) {
            perror("Error writing data to file");
             while(head){
                WriteBuffer* temp = head;
                head = head->next;
                free(temp->data);
                free(temp);
            }
            return 51;
        }
    }
    if(backupSocket2!=-1){
        sendReadRequestToServer(backupSocket2, request, NULL);
        currentNode = head;
        while(currentNode){
            packet.length = currentNode->dataSize;
            packet.stop = 0;
            strncpy(packet.data, currentNode->data, sizeof(packet.data) - 1);
            packet.data[sizeof(packet.data) - 1] = '\0';  // Null-terminate the string
            int bytesWritten = send_full(backupSocket2, &packet, sizeof(PACKET));
            if (bytesWritten == -1) {
                perror("Error writing data to file");
                 while(head){
                    WriteBuffer* temp = head;
                    head = head->next;
                    free(temp->data);
                    free(temp);
                }
                return 51;
            }
            totalBytesWritten += bytesWritten;
            currentNode = currentNode->next;
        }
        packet.stop = 1;
        packet.length = 0;
        int bytesWritten = send_full(writeSocket, &packet, sizeof(PACKET));
        if (bytesWritten == -1) {
            perror("Error writing data to file");
             while(head){
                WriteBuffer* temp = head;
                head = head->next;
                free(temp->data);
                free(temp);
            }
            return 51;
        }
    }

    printf("Cleaning up...\n");
    while(head){
        WriteBuffer* temp = head;
        head = head->next;
        free(temp->data);
        free(temp);
    }



    return __OK__;
}

// Function to check if the path has a .mp3 extension
bool has_mp3_extension(const char *path) {
    // Get the length of the string
    size_t len = strlen(path);
    
    // Check if the string is long enough to contain ".mp3"
    if (len >= 4) { // ".mp3" is 4 characters long
        // Compare the last 4 characters of the string with ".mp3"
        if (strncmp(path + len - 4, ".mp3", 4) == 0) {
            return true; // File has .mp3 extension
        }
    }
    
    return false; // File does not have .mp3 extension
}



void* clientThreadFunction(void* arg){  
    int newClientSocket = *(int*)arg;
    // printf("skdksd\n");
    while(true){
        FILE_REQUEST_STRUCT* request;
        request = (FILE_REQUEST_STRUCT*)namingServerReceive(newClientSocket, __FILE_REQUEST__, true);
        if(request == NULL){

            printf(BRIGHT_RED"[-] Client %d disconnected12\n"RESET, numberOfClients);
            return NULL;
        }
        FILE_REQUEST_STRUCT requestCopy = *request;
        printRequest(request);

        if(strcmp(request->opType, "EXIT") == 0){
            printf(YELLOW"Client Exiting...\n"RESET);
            return NULL;
        }
        else if(strcmp(request->opType,"READ") == 0){
            char* path = addPrefix(request->path);
            
            // search for ss in trie and then send its ip and port to client
            // bool checkDir = getStorageServerDirectlyFromFile(root,path)->isDirectory;
            RETURN_DATA* ret = getStorageServerDirectlyFromFile(root,path);
            bool checkDir = ret->isDirectory;
            RETURN_DATA* stringStorageName = getStorageServerName(root, path);
            if(stringStorageName==NULL||stringStorageName->value==NULL || checkDir || ret==NULL || ret->value==NULL){
                if(stringStorageName!=NULL) free(stringStorageName);

                //////////////// INESH TOOK CARE /////////////////////////
                // printf("Path does not exist in Storage   Servers: %s Failed!\n",request->opType);
                handle_error(ERR_PATH_NOT_EXIST, request->opType);
                
                SERVER_CONFIG server;
                server.portNumber = -1;
                server.errorCode = ERR_PATH_NOT_EXIST;
                if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                    printf(BRIGHT_RED"[-] Client %d disconnected4\n"RESET, numberOfClients);
                    return NULL;
                }
                printServerInfo(&server, request->opType, -1, true);
                logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, true, requestCopy.opType,true);
                printf(RED"[-] Client %d disconnected15\n"RESET, numberOfClients);
                printf(BRIGHT_RED"[-] Client %d disconnected13\n"RESET, numberOfClients);
                continue; // continue to the next request
            }
            // send the server_info to the client
            
            int storageServerName = atoi(stringStorageName->value);
        
            free(stringStorageName);
            // int storageServerName = atoi(stringStorageName);

            TrieNode* node = findNode(root, path);
            requestReadAccess(node);
            

            printf("Storage Server Name: %d\n", storageServerName);
            SERVER_CONFIG server;
            server.portNumber = -1;
            server.errorCode = ERR_SERVER_DOWN;
            if(connected[storageServerName]){
                server.portNumber = listStorgeServers[storageServerName].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName].networkIP);
            }
            else if(storageServerName+1 < numberOfStorageServers && connected[storageServerName+1]){
                server.portNumber = listStorgeServers[storageServerName+1].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName+1].networkIP);
            }
            else if(storageServerName+2 < numberOfStorageServers &&connected[storageServerName+2]){
                server.portNumber = listStorgeServers[storageServerName+2].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName+2].networkIP);
            }
            printf("CLIENT PORT: %d\n", server.portNumber);

            if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected5\n"RESET, numberOfClients);
                return NULL;
            }
            // print the server info
            printServerInfo(&server, request->opType, storageServerName, false);
            logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, false,requestCopy.opType,true);
            printf(YELLOW"========= Waiting for Ack from CLIENT %d =========\n"RESET,numberOfClients);
            int status = __FAILURE__;
            if(newClientSocket == -1 || recv(newClientSocket, &status, sizeof(int), 0) < 0){
                perror("Could not receive the response from SS for READ");
                return NULL;
            }
            
            // print the status recieved
            printf(MAGENTA">> Recieved Status:%d\n"RESET, status);
            logMessageRecieve(newClientSocket, __STATUS__, (void**)&status, true);
            /////////////////////////////////////////
            //// handle the concurrent read request
            ////////////////////////////////////////
            releaseReadAccess(node);
    


        }
        else if(strcmp(request->opType,"WRITE") == 0){
            char* path = addPrefix(request->path);
            RETURN_DATA* ret = getStorageServerDirectlyFromFile(root,path);
            bool checkDir = ret->isDirectory;
            // bool checkDir = getStorageServerDirectlyFromFile(root,path)->isDirectory;
            RETURN_DATA* stringStorageName = getStorageServerName(root, path);
            
            if(stringStorageName==NULL || stringStorageName->value==NULL || checkDir || ret==NULL || ret->value==NULL){
                if(stringStorageName!=NULL) free(stringStorageName);
                // printf("Path does not exist in Storage Servers: %s Failed!\n",request->opType);
                handle_error(ERR_PATH_NOT_EXIST, request->opType);
                
                //////////////// INESH TOOK CARE /////////////////////////

                SERVER_CONFIG server;
                server.portNumber = -1;
                server.errorCode = ERR_PATH_NOT_EXIST;
                if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                    printf(BRIGHT_RED"[-] Client %d disconnected6\n"RESET, numberOfClients);
                    return NULL;
                }
                if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                    printf(BRIGHT_RED"[-] Client %d disconnected6\n"RESET, numberOfClients);
                    return NULL;
                }
                if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                    printf(BRIGHT_RED"[-] Client %d disconnected6\n"RESET, numberOfClients);
                    return NULL;
                }
                printServerInfo(&server, request->opType, -1, true);
                logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, true, request->opType,true);
                printf(RED"[-] Client %d disconnected15\n"RESET, numberOfClients);
                printf(BRIGHT_RED"[-] Client %d disconnected14\n"RESET, numberOfClients);
                continue; // continue to the next request
            }
            int storageServerName = atoi(stringStorageName->value);
            free(stringStorageName);

            TrieNode* node = findNode(root, path);
            requestWritePermission(node);

            SERVER_CONFIG server;
            if(connected[storageServerName]){
                server.portNumber = listStorgeServers[storageServerName].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName].networkIP);
            }
            else{
                server.portNumber = -1;
                strcpy(server.networkIP, "NULL");
            }
            if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected7\n"RESET, numberOfClients);
                return NULL;
            }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////
            ////////////////////////////////// BACKUP //////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////////////////////////////////////////////   

            // print the server info
            printServerInfo(&server, request->opType, storageServerName, false);
            logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, false, request->opType,true);

            SERVER_CONFIG backup1,backup2;
            if(storageServerName+1<numberOfStorageServers && connected[storageServerName+1]){
                backup1.portNumber = listStorgeServers[storageServerName+1].clientPort;
                strcpy(backup1.networkIP, listStorgeServers[storageServerName+1].networkIP);
            }
            else{
                backup1.portNumber = -1;
                strcpy(backup1.networkIP, "NULL");
            }
            if(storageServerName+2<numberOfStorageServers && connected[storageServerName+2]){
                backup2.portNumber = listStorgeServers[storageServerName+2].clientPort;
                strcpy(backup2.networkIP, listStorgeServers[storageServerName+2].networkIP);
            }
            else{
                backup2.portNumber = -1;
                strcpy(backup2.networkIP,"NULL");
            }

            if (backup1.portNumber == -1) {
                printf(YELLOW "[⚠] Warning: Backup 1 is not configured (Port: NULL).\n" RESET);
            }

            if (backup2.portNumber == -1) {
                printf(YELLOW "[⚠] Warning: Backup 2 is not configured (Port: NULL).\n" RESET);
            }

            if (backup1.portNumber != -1 && backup2.portNumber != -1) {
                printf(GREEN "[✔] Both backups are configured successfully.\n" RESET);
            } else if (backup1.portNumber != -1) {
                printf(CYAN "[✔] Backup 1 is configured successfully. (Port: %d)\n" RESET, backup1.portNumber);
            } else if (backup2.portNumber != -1) {
                printf(CYAN "[✔] Backup 2 is configured successfully. (Port: %d)\n" RESET, backup2.portNumber);
            }


            if(send(newClientSocket, &backup1, sizeof(SERVER_CONFIG), 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected8\n"RESET, numberOfClients);
                return NULL;
            }
            if(send(newClientSocket, &backup2, sizeof(SERVER_CONFIG), 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected9\n"RESET, numberOfClients);
                return NULL;
            }

            int status = __FAILURE__;
            

            //////////////////////// recieve acknowledgment from client //////////////////////////
            // if(recv(newClientSocket, &status, sizeof(int), 0) < 0){
            //     perror("Could not receive the response from SS for WRITE");
            //     return NULL;
            // }
            // printf("Status: %d\n", status);
            // bool flag = true;
            // if(status==__OK__ && flag){
            //     printf("HERE1\n");
            //     releaseWriteAccess(node);
            //     flag=false;
            // }
            // if(recv(newClientSocket, &status, sizeof(int), 0) < 0){
            //     perror("Could not receive the response from SS for WRITE");
            //     return NULL;
            // }
            // printf("Status: %d\n", status);
            // if(status==__OK__ && flag){
            //     printf("here2\n");
            //     releaseWriteAccess(node);
            //     flag=false;
            // }
            // if(recv(newClientSocket, &status, sizeof(int), 0) < 0){
            //     perror("Could not receive the response from SS for WRITE");
            //     return NULL;
            // }
            // printf("Status: %d\n", status);
            // if(flag){
            //     printf("Here3\n");
            //     releaseWriteAccess(node);
            // }       
            STATUS_WRITE statusWrite ={__FAILURE__,__FAILURE__,__FAILURE__};
            // if(recv(newClientSocket, &statusWrite, sizeof(STATUS_WRITE), 0) < 0){
            //     perror("Could not receive the response from SS for WRITE");
            //     return NULL;
            // }

            int flag = MSG_WAITALL;
            // use this flag to receive the data in one go
            while(recv(newClientSocket, &statusWrite, sizeof(STATUS_WRITE), flag) < 0){
                perror("Could not receive the response from client for WRITE");
                // return NULL;
            }
            // if(recv_full(newClientSocket, &statusWrite, sizeof(STATUS_WRITE))==-1){
            //     perror("Could not receive the response from SS for WRITE");
            //     return NULL;
            // }

            printf("Status: %d\n", statusWrite.actual);    
            printf("Status: %d\n", statusWrite.backup1);
            printf("Status: %d\n", statusWrite.backup2);
            if(statusWrite.actual==__OK__ || statusWrite.backup1==__OK__ || statusWrite.backup2==__OK__){
                releaseWriteAccess(node);
            }
            else{
                releaseWriteAccess(node); /////////////////////////////////////////////////////////////////////////////////////////////// ASK SIR ////////////////////////////////////////
            }

        }
        else if(strcmp(request->opType,"GET")==0){
            char* path = addPrefix(request->path);
            RETURN_DATA* ret = getStorageServerDirectlyFromFile(root,path);
            bool checkDir = ret->isDirectory;

            RETURN_DATA* stringStorageName = getStorageServerName(root, path);
            if(stringStorageName==NULL || stringStorageName->value==NULL || ret==NULL || ret->value==NULL){
                if(stringStorageName!=NULL) free(stringStorageName);
                // printf("Path does not exist in Storage Servers: %s Failed!\n",request->opType);
                handle_error(ERR_PATH_NOT_EXIST, request->opType);

                ////////////////// INESH TOOK CARE /////////////////////////

                SERVER_CONFIG server;
                server.portNumber = -1;
                server.errorCode = ERR_PATH_NOT_EXIST;
                if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                    printf(BRIGHT_RED"[-] Client %d disconnected8\n"RESET, numberOfClients);
                    return NULL;
                }
                printServerInfo(&server, request->opType, -1, true);
                logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, true, request->opType,true);
                printf(RED"[-] Client %d disconnected15\n"RESET, numberOfClients);
                continue; // continue to the next request
            }
            // connect to the storage server
            requestReadAccess(findNode(root, path));

            int storageServerName = atoi(stringStorageName->value);
            free(stringStorageName);
            SERVER_CONFIG server;
            // server.portNumber = listStorgeServers[storageServerName].clientPort;
            // strcpy(server.networkIP, listStorgeServers[storageServerName].networkIP);

            server.portNumber = -1;
            server.errorCode = ERR_SERVER_DOWN;
            if(connected[storageServerName]){
                server.portNumber = listStorgeServers[storageServerName].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName].networkIP);
            }
            else if(storageServerName+1 < numberOfStorageServers && connected[storageServerName+1]){
                server.portNumber = listStorgeServers[storageServerName+1].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName+1].networkIP);
            }
            else if(storageServerName+2 < numberOfStorageServers &&connected[storageServerName+2]){
                server.portNumber = listStorgeServers[storageServerName+2].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName+2].networkIP);
            }

            if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected9\n"RESET, numberOfClients);
                return NULL;
            }
            
            printServerInfo(&server, request->opType, storageServerName, false);
            logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, false, request->opType,true);

            int status = __FAILURE__;
            if(recv(newClientSocket, &status, sizeof(int), 0) < 0){
                perror("Could not receive the response from SS for GET");
                return NULL;
            }
            printf("Status: %d\n", status);
            releaseReadAccess(findNode(root, path));
        }
        else if(strcmp(request->opType,"STREAM")==0 ){
            char* path = addPrefix(request->path);
            RETURN_DATA* ret = getStorageServerDirectlyFromFile(root,path);
            bool checkDir = ret->isDirectory;
            bool mp3extension = has_mp3_extension(path);
            printf("Has mp3: ??%d\n",mp3extension);
            RETURN_DATA* stringStorageName = getStorageServerName(root, path);
            // also please check if the path has .mp3 extension
            if(stringStorageName==NULL || stringStorageName->value==NULL || checkDir || ret==NULL || ret->value==NULL || mp3extension==false){
                if(stringStorageName!=NULL) free(stringStorageName);
                // printf("Path does not exist in Storage Servers: %s Failed!\n",request->opType);
                handle_error(ERR_PATH_NOT_EXIST, request->opType);
                
                ////////////////// INESH TOOK CARE /////////////////////////
                SERVER_CONFIG server;
                server.portNumber = -1;
                server.errorCode = ERR_PATH_NOT_EXIST;
                if(mp3extension==false){
                    server.errorCode = ERR_INVALID_FILE_FORMAT;
                }
                if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                    printf(BRIGHT_RED"[-] Client %d disconnected10\n"RESET, numberOfClients);
                    return NULL;
                }
                printServerInfo(&server, request->opType, -1, true);
                logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, true, request->opType,true);
                printf(RED"[-] Client %d disconnected15\n"RESET, numberOfClients);
                continue; // continue to the next request
            }
            // send the server_info to the client
            int storageServerName = atoi(stringStorageName->value);
            free(stringStorageName);
            SERVER_CONFIG server;
            // server.portNumber = listStorgeServers[storageServerName].clientPort;
            // strcpy(server.networkIP, listStorgeServers[storageServerName].networkIP);
            server.portNumber = -1;
            server.errorCode = ERR_SERVER_DOWN;
            if(connected[storageServerName]){
                server.portNumber = listStorgeServers[storageServerName].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName].networkIP);
            }
            else if(storageServerName+1 < numberOfStorageServers && connected[storageServerName+1]){
                server.portNumber = listStorgeServers[storageServerName+1].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName+1].networkIP);
            }
            else if(storageServerName+2 < numberOfStorageServers &&connected[storageServerName+2]){
                server.portNumber = listStorgeServers[storageServerName+2].clientPort;
                strcpy(server.networkIP, listStorgeServers[storageServerName+2].networkIP);
            }
            
            if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected11\n"RESET, numberOfClients);
                return NULL;
            }
            printServerInfo(&server, request->opType, storageServerName, false);
            logMessageSent(newClientSocket, __SERVER_INFO__, (void**)&server, false, request->opType,true);

            int status = __FAILURE__;
            int socketServer = createSocketForAck(storageServerName);

            if(recv(newClientSocket, &status, sizeof(int), 0) < 0){
                perror("Could not receive the response from SS for STREAM");
                return NULL;
            }
            printf("Status: %d\n", status);

        }
        else if(strcmp(request->opType,"DELETE") == 0 || strcmp(request->opType, "CREATE") == 0){
            char* path = addPrefix(request->path);
            // printf("Directory: %d\n", request->isDirectory);
            RETURN_DATA* stringStorageName;
            bool checkDir = false;
            char copy[__BUFFER_SIZE__];
            strcpy(copy, path);
            if(request->opType == "DELETE"){
                stringStorageName = getStorageServerName(root, path);

                printf("Path: %s\n", path);
                strcpy(path, copy);
            }
            else{
                // char* parentPath = getParentPath(path);
                char* parentPath = path;
                stringStorageName = getStorageServerName(root, parentPath);
                printf("Parent Path: %s\n", parentPath);
                strcpy(path, copy);
            }

            if(stringStorageName==NULL || stringStorageName->value==NULL){
                if(stringStorageName!=NULL) free(stringStorageName);
                printf("Path does not exist in Storage Servers: %s Failed!",request->opType);
                handle_error(ERR_PATH_NOT_EXIST, request->opType);
                int status = __FAILURE__;
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                printCreateDeleteRequest(request, status,true);
                continue; // continue to the next request
            }
            if(strcmp(request->opType,"DELETE") == 0){
                checkDir = getStorageServerDirectlyFromFile(root,path)->isDirectory;
            }
            // connect to the storage server
            if(strcmp(request->opType,"DELETE")==0 && request->isDirectory != checkDir){
                handle_error(ERR_DIRECTORY_MISMATCH, NULL);
                int status = __FAILURE__;
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                printCreateDeleteRequest(request, status,true);
                continue;
            }

            int storageServerName = atoi(stringStorageName->value);
            // request->isDirectory = stringStorageName->isDirectory; //askshubham
            free(stringStorageName);
            /////////////////////////////////////////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////////////////////////////////////////
            if(connected[storageServerName]==false){
                if(connected[storageServerName+1]==true && connected[storageServerName+2]==true){
                    // redirect to backup 1 and create backup in backup 2
                    int status = sendToSSForDeleteAndCreate(request, storageServerName+1, false,storageServerName);
                    int backup2 = sendToSSForDeleteAndCreate(request, storageServerName+2, true,storageServerName);
                    printbackupDetails(status, backup2,true,false);
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    continue;
                }
                else if(connected[storageServerName+1]==true){
                    // redirect to backup 1
                    int status = sendToSSForDeleteAndCreate(request, storageServerName+1, false,storageServerName);
                    printbackupDetails(status, __FAILURE__,true,false);
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    
                    continue;
                }
                else if(connected[storageServerName+2]==true){
                    // redirect to backup 2
                    int status = sendToSSForDeleteAndCreate(request, storageServerName+2, false,storageServerName);
                    printbackupDetails(status, __FAILURE__,false,true);
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    continue;
                }
                else{
                    int status = __FAILURE__;
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    continue;
                }
            }
            int status = sendToSSForDeleteAndCreate(request, storageServerName, false,storageServerName);

            int backup1=__FAILURE__,backup2=__FAILURE__;
            if(storageServerName+1<__MAX_STORAGE_SERVERS__){
                backup1 = sendToSSForDeleteAndCreate(request,storageServerName+1,true,storageServerName);
            }
            if(storageServerName+2<__MAX_STORAGE_SERVERS__){
                backup2 = sendToSSForDeleteAndCreate(request,storageServerName+2,true,storageServerName);
            }
            printbackupDetails(backup1, backup2,false,false);
            sendCreateDeleteCopyStatus(newClientSocket, status, request);
            logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
            printCreateDeleteRequest(request, status,true);
            if(status != __OK__){
                continue;
            }
        }
        else if(strcmp(request->opType,"COPY")==0){
            // get the source and destination storage server names
            char* srcPath = addPrefix(request->path); char* destPath = addPrefix(request->destination_path);
            char cpySrc[__BUFFER_SIZE__]; char cpyDest[__BUFFER_SIZE__];
            strcpy(cpySrc, srcPath); strcpy(cpyDest, destPath);
            RETURN_DATA* srcStorage = getStorageServerDirectlyFromFile(root, cpySrc);
            RETURN_DATA* destStorage = getStorageServerDirectlyFromFile(root, cpyDest);


            if(srcStorage==NULL||srcStorage->value==NULL){
                if(srcStorage!=NULL) free(srcStorage);
                if(destStorage!=NULL) free(destStorage);
                printf("Source path does not exist in Storage Servers: COPY Failed!");
                int status = __FAILURE__;
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                printCreateDeleteRequest(request, status,true);
                continue; // continue to the next request
            }
            if(destStorage==NULL || destStorage->value==NULL){
                if(srcStorage!=NULL) free(srcStorage);
                if(destStorage!=NULL) free(destStorage);

                printf("Destination path does not exist in Storage Servers: COPY Failed!");
                int status = __FAILURE__;
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                printCreateDeleteRequest(request, status,true);

                continue; // continue to the next request
            }
            
            int srcStorageServer = atoi(srcStorage->value);
            int destStorageServer = atoi(destStorage->value);
            printf("Source Storage Server: %d\n", srcStorageServer);
            printf("Destination Storage Server: %d\n", destStorageServer);

            if(srcStorage->isDirectory==false && destStorage->isDirectory==false){
                printf(YELLOW"==================== File to File Copy ====================\n"RESET);
                // we assume that both file exists and copy the content of the source file to the destination file
                // create Read and Write requests
                int readSocketServer = createSocketAndConnect(srcStorageServer);
                
                FILE_REQUEST_STRUCT readRequest = *request;
                strcpy(readRequest.opType, "READ");
                strcpy(readRequest.path, srcPath);
                // char pipe[__BUFFER_SIZE__];
                int status;
                // if(status != __OK__){
                //     sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //     logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //     printCreateDeleteRequest(request, status,true);
                //     free(srcStorage);
                //     free(destStorage);
                //     return NULL;
                // }
                // close(readSocketServer);
                printf(CYAN">> Read Path: %s\n"RESET, readRequest.path);
                send_full(readSocketServer, &readRequest, sizeof(FILE_REQUEST_STRUCT));
                ////////////////////////////////////////////////////////////////
                // send the write request to the new storage server
                FILE_REQUEST_STRUCT writeRequest = *request;
                strcpy(writeRequest.opType, "WRITE");
                strcpy(writeRequest.path, destPath);
                printf(CYAN">> Write Path: %s\n"RESET, writeRequest.path);
                
                // if(connected[destStorageServer]==false){
                //     int status = __FAILURE__;
                //     if(connected[destStorageServer+1]==true && connected[destStorageServer+2]==true){
                //         // redirect to backup 1 and create backup in backup 2
                //         int socketServer = createSocketAndConnect(destStorageServer+1);
                //         status = sendWriteRequestToServer(socketServer, &writeRequest, pipe);
                //         int socketServer2 = createSocketAndConnect(destStorageServer+2);
                //         int backup2 = sendWriteRequestToServer(socketServer2, &writeRequest, pipe);
                //         printbackupDetails(status, backup2,true,false);
                //         close(socketServer2);
                //         close(socketServer);
                //     }
                //     else if(connected[destStorageServer+1]==true){
                //         // redirect to backup 1
                //         int socketServer = createSocketAndConnect(destStorageServer+1);
                //         status = sendWriteRequestToServer(socketServer, &writeRequest, pipe);
                //         printbackupDetails(status, __FAILURE__,true,false);
                //         close(socketServer);
                //     }
                //     else if(connected[destStorageServer+2]==true){
                //         // redirect to backup 2
                //         int socketServer = createSocketAndConnect(destStorageServer+2);
                //         status = sendWriteRequestToServer(socketServer, &writeRequest, pipe);
                //         printbackupDetails(status, __FAILURE__,false,true);
                //         close(socketServer);
                //     }
                //     if(status != __OK__){
                //         printf("Could not COPY\n");
                //         int status = __FAILURE__;
                //         sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //         logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //         printCreateDeleteRequest(request, status,true);
                //         free(srcStorage);
                //         free(destStorage);
                //         continue;
                //     }
                // }
                // else{
                //     int socketServer = createSocketAndConnect(destStorageServer);
                //     int status = sendWriteRequestToServer(socketServer, &writeRequest, pipe);

                //     int backup1 = __FAILURE__,backup2 = __FAILURE__;
                //     if(destStorageServer+1<__MAX_STORAGE_SERVERS__){
                //         int socketServer1 = createSocketAndConnect(destStorageServer+1);
                //         backup1 = sendWriteRequestToServer(socketServer1, &writeRequest, pipe);
                //         close(socketServer1);
                //     }
                //     if(destStorageServer+2<__MAX_STORAGE_SERVERS__){
                //         int socketServer2 = createSocketAndConnect(destStorageServer+2);
                //         backup2 = sendWriteRequestToServer(socketServer2, &writeRequest, pipe);
                //         close(socketServer2);
                //     }
                //     close(socketServer);
                //     printbackupDetails(backup1, backup2,false,false);
                //     if(status != __OK__){
                //         printf("Could not COPY\n");
                //         int status = __FAILURE__;
                //         sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //         logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //         printCreateDeleteRequest(request, status,true);
                //         free(srcStorage);
                //         free(destStorage);
                //         continue;
                //     }
                // }
                // if(status != __OK__){
                //     sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //     logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //     printCreateDeleteRequest(request, status,true);
                //     free(srcStorage);
                //     free(destStorage);
                //     return NULL;
                // }
                // sendCreateDeleteCopyStatus(newClientSocket, status, request);
                // logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                // printCreateDeleteRequest(request, status,true);
                if(connected[destStorageServer]==false){
                    int status = __FAILURE__;
                    if(connected[destStorageServer+1]==true && connected[destStorageServer+2]==true){
                        // redirect to backup 1 and create backup in backup 2
                        int socketServer = createSocketAndConnect(destStorageServer+1);
                        int socketServer2 = createSocketAndConnect(destStorageServer+2);
                        status = write_file(-1,"abc",readSocketServer,socketServer,socketServer2,-1,&writeRequest);
                        printbackupDetails(status, status,true,false);
                        close(socketServer2);
                        close(socketServer);
                    }
                    else if(connected[destStorageServer+1]==true){
                        // redirect to backup 1
                        int socketServer = createSocketAndConnect(destStorageServer+1);
                        status = write_file(-1,"abc",readSocketServer,socketServer,-1,-1,&writeRequest);
                        printbackupDetails(status, __FAILURE__,true,false);
                        close(socketServer);
                    }
                    else if(connected[destStorageServer+2]==true){
                        // redirect to backup 2
                        int socketServer = createSocketAndConnect(destStorageServer+2);
                        status = write_file(-1,"abc",readSocketServer,-1,socketServer,-1,&writeRequest);
                        printbackupDetails(status, __FAILURE__,false,true);
                        close(socketServer);
                    }
                    if(status != __OK__){
                        printf("Could not COPY\n");
                        int status = __FAILURE__;
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                        printCreateDeleteRequest(request, status,true);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                }
                else{
                    int socketServer = createSocketAndConnect(destStorageServer);
                    int socketServer1 = -1,socketServer2 = -1;

                    int backup1 = __FAILURE__,backup2 = __FAILURE__;
                    if(destStorageServer+1<__MAX_STORAGE_SERVERS__){
                        socketServer1 = createSocketAndConnect(destStorageServer+1);
                        
                    }
                    if(destStorageServer+2<__MAX_STORAGE_SERVERS__){
                        socketServer2 = createSocketAndConnect(destStorageServer+2);
                    }
                    int status = write_file(-1,"abc",readSocketServer,socketServer,socketServer1,socketServer2,&writeRequest);
                    close(socketServer);
                    close(socketServer1);
                    close(socketServer2);
                    printbackupDetails(backup1, backup2,false,false);
                    if(status != __OK__){
                        printf("Could not COPY\n");
                        int status = __FAILURE__;
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                        printCreateDeleteRequest(request, status,true);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                }
                if(status != __OK__){
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    free(srcStorage);
                    free(destStorage);
                    return NULL;
                }
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                printCreateDeleteRequest(request, status,true);

            }
            else if(srcStorage->isDirectory==true && destStorage->isDirectory==false){
                printf(YELLOW"==================== Directory to File Copy ====================\n"RESET);
                int status = __FAILURE__;
                printf(BRIGHT_RED"[Directory to File Copy is not allowed]\n"RESET);
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                printCreateDeleteRequest(request, status,true);
                printf(BRIGHT_RED"[-] Client %d disconnected22\n"RESET, numberOfClients);
                free(srcStorage);
                free(destStorage);
                continue;
            }
            else if(srcStorage->isDirectory==false && destStorage->isDirectory==true){
                // file to directory copy get the file name from the path
                printf(YELLOW"==================== File to Directory Copy ====================\n"RESET);
                char fileName[__BUFFER_SIZE__];
                getFileNameFromPath(srcPath, fileName);
                printf(CYAN">> File Name: %s\n"RESET, fileName);
                if(fileName==NULL){
                    int status = __FAILURE__;
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);

                    free(srcStorage);
                    free(destStorage);
                    continue;
                }
                // get the path of the file in the destination directory
                FILE_REQUEST_STRUCT createRequest = *request;
                char destPathFile[__BUFFER_SIZE__];
                strcpy(destPathFile, destPath);
                strcat(destPathFile, "/");
                strcat(destPathFile, fileName);
                strcpy(createRequest.path, destPathFile);
                createRequest.isDirectory = false;
                strcpy(createRequest.opType, "CREATE");
                
                // int flag = 0;

                // if(connected[destStorageServer]==false){
                //     int status = __FAILURE__;
                //     if(connected[destStorageServer+1]==true && connected[destStorageServer+2]==true){
                //         // redirect to backup 1 and create backup in backup 2
                //         flag = 1;
                //         status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer, false);
                //         int backup2 = sendToSSForDeleteAndCreate(&createRequest, destStorageServer+2, true);
                //         printbackupDetails(status, backup2,true,false);
                        
                //     }
                //     else if(connected[destStorageServer+1]==true){
                //         // redirect to backup 1
                //         flag = 2;
                //         status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer+1, false);
                //         printbackupDetails(status, __FAILURE__,true,false);
                //     }
                //     else if(connected[destStorageServer+2]==true){
                //         // redirect to backup 2
                //         flag = 3;
                //         status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer+2, false);
                //         printbackupDetails(status, __FAILURE__,false,true);
                //     }
                //     if(status != __OK__){
                //         printf("Could not COPY\n");
                //         int status = __FAILURE__;
                //         sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //         logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //         printCreateDeleteRequest(request, status,true);
                //         free(srcStorage);
                //         free(destStorage);
                //         continue;
                //     }
                // }
                // else{

                //     int status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer, false);
                //     int backup1 = __FAILURE__,backup2 = __FAILURE__;
                //     flag = 4;
                //     if(destStorageServer+1<__MAX_STORAGE_SERVERS__){
                //         backup1 = sendToSSForDeleteAndCreate(&createRequest,destStorageServer+1,true);
                //         flag+=1;
                //     }
                //     if(destStorageServer+2<__MAX_STORAGE_SERVERS__){
                //         backup2 = sendToSSForDeleteAndCreate(&createRequest,destStorageServer+2,true);
                //         flag+=2;
                //     }
                //     printbackupDetails(backup1, backup2,false,false);
                //     if(status != __OK__){
                //         printf("Could not COPY\n");
                //         int status = __FAILURE__;
                //         sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //         logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //         printCreateDeleteRequest(request, status,true);

                //         free(srcStorage);
                //         free(destStorage);
                //         continue;
                //     }
                // }
                
                // // create Read and Write requests
                // int readSocketServer = createSocketAndConnect(srcStorageServer);

                // FILE_REQUEST_STRUCT readRequest = *request;
                // strcpy(readRequest.opType, "READ");
                // strcpy(readRequest.path, srcPath);
                // char pipe[__BUFFER_SIZE__];
                // int status = sendReadRequestToServer(readSocketServer, &readRequest, pipe);
                
                // if(status != __OK__){
                //     sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //     logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //     printCreateDeleteRequest(request, status,true);
                //     return NULL;
                // }
                // close(readSocketServer);
                // printf(CYAN">> Read Path: %s\n"RESET, readRequest.path);
                // ////////////////////////////////////////////////////////////////
                // // send the write request to the new storage server
                // FILE_REQUEST_STRUCT writeRequest = *request;
                // strcpy(writeRequest.opType, "WRITE");
                // strcpy(writeRequest.path, createRequest.path);
                // printf(CYAN">> Write Path: %s\n"RESET, writeRequest.path);
                // if(flag==1){
                //     int socket = createSocketAndConnect(destStorageServer+1);
                //     status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //     close(socket);
                //     socket = createSocketAndConnect(destStorageServer+2);
                //     int backup2 = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //     close(socket);
                //     printbackupDetails(status, backup2,true,false);
                // }
                // else if(flag==2){
                //     int socket = createSocketAndConnect(destStorageServer+1);
                //     status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //     close(socket);
                //     printbackupDetails(status, __FAILURE__,true,false);
                // }
                // else if(flag==3){
                //     int socket = createSocketAndConnect(destStorageServer+2);
                //     status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //     close(socket);
                //     printbackupDetails(status, __FAILURE__,false,true);
                // }
                // else{
                //     int writeSocketServer = createSocketAndConnect(destStorageServer);
                //     status = sendWriteRequestToServer(writeSocketServer, &writeRequest, pipe);
                //     sendCreateDeleteCopyStatus(newClientSocket, status, request);
                //     logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                //     printCreateDeleteRequest(request, status,true);
                //     if(flag==4){
                //         printbackupDetails(__FAILURE__, __FAILURE__,false,false);
                //     }
                //     else if(flag==5){
                //         int socket = createSocketAndConnect(destStorageServer+1);
                //         status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //         close(socket);
                //         printbackupDetails(status, __FAILURE__,false,false);
                //     }
                //     else if(flag==6){
                //         int socket = createSocketAndConnect(destStorageServer+2);
                //         status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //         close(socket);
                //         printbackupDetails(__FAILURE__, status,false,false);
                //     }
                //     else if(flag==7){
                //         int socket = createSocketAndConnect(destStorageServer+1);
                //         status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //         close(socket);
                //         socket = createSocketAndConnect(destStorageServer+2);
                //         int backup2 = sendWriteRequestToServer(socket, &writeRequest, pipe);
                //         close(socket);
                //         printbackupDetails(status, backup2,false,false);
                //     }
                //     close(writeSocketServer);
                // }


                int flag = 0;

                if(connected[destStorageServer]==false){
                    int status = __FAILURE__;
                    if(connected[destStorageServer+1]==true && connected[destStorageServer+2]==true){
                        // redirect to backup 1 and create backup in backup 2
                        flag = 1;
                        status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer, false,destStorageServer);
                        int backup2 = sendToSSForDeleteAndCreate(&createRequest, destStorageServer+2, true,destStorageServer);
                        printbackupDetails(status, backup2,true,false);
                        
                    }
                    else if(connected[destStorageServer+1]==true){
                        // redirect to backup 1
                        flag = 2;
                        status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer+1, false,destStorageServer);
                        printbackupDetails(status, __FAILURE__,true,false);
                    }
                    else if(connected[destStorageServer+2]==true){
                        // redirect to backup 2
                        flag = 3;
                        status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer+2, false,destStorageServer);
                        printbackupDetails(status, __FAILURE__,false,true);
                    }
                    if(status != __OK__){
                        printf("Could not COPY\n");
                        int status = __FAILURE__;
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                        printCreateDeleteRequest(request, status,true);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                }
                else{

                    int status = sendToSSForDeleteAndCreate(&createRequest, destStorageServer, false,destStorageServer);
                    int backup1 = __FAILURE__,backup2 = __FAILURE__;
                    flag = 4;
                    if(destStorageServer+1<__MAX_STORAGE_SERVERS__){
                        backup1 = sendToSSForDeleteAndCreate(&createRequest,destStorageServer+1,true,destStorageServer);
                        flag+=1;
                    }
                    if(destStorageServer+2<__MAX_STORAGE_SERVERS__){
                        backup2 = sendToSSForDeleteAndCreate(&createRequest,destStorageServer+2,true,destStorageServer);
                        flag+=2;
                    }
                    printbackupDetails(backup1, backup2,false,false);
                    if(status != __OK__){
                        printf("Could not COPY\n");
                        int status = __FAILURE__;
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                        printCreateDeleteRequest(request, status,true);

                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                }
                
                // create Read and Write requests
                int readSocketServer = createSocketAndConnect(srcStorageServer);

                FILE_REQUEST_STRUCT readRequest = *request;
                strcpy(readRequest.opType, "READ");
                strcpy(readRequest.path, srcPath);
                int status = send_full(readSocketServer, &readRequest, sizeof(FILE_REQUEST_STRUCT));
                
                if(status != __OK__){
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    return NULL;
                }
                printf(CYAN">> Read Path: %s\n"RESET, readRequest.path);
                ////////////////////////////////////////////////////////////////
                // send the write request to the new storage server
                FILE_REQUEST_STRUCT writeRequest = *request;
                strcpy(writeRequest.opType, "WRITE");
                strcpy(writeRequest.path, createRequest.path);
                printf(CYAN">> Write Path: %s\n"RESET, writeRequest.path);
                int socket = -1; int backup1Socket = -1,backup2Socket = -1;
                printf("flag: %d\n",flag);
                if(flag==1){
                    int socket = createSocketAndConnect(destStorageServer+1);
                    // status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                    // close(socket);
                    backup1Socket = createSocketAndConnect(destStorageServer+2);
                    // int backup2 = sendWriteRequestToServer(socket, &writeRequest, pipe);
                    write_file(-1,"abc",readSocketServer,socket,backup1Socket,-1,&writeRequest);

                    // close(socket);
                    printbackupDetails(status, status,true,false);
                }
                else if(flag==2){
                    int socket = createSocketAndConnect(destStorageServer+1);
                    status = write_file(-1,"abc",readSocketServer,socket,-1,-1,&writeRequest);
                    close(socket);
                    printbackupDetails(status, __FAILURE__,true,false);
                }
                else if(flag==3){
                    int socket = createSocketAndConnect(destStorageServer+2);
                    status = write_file(-1,"abc",readSocketServer,-1,socket,-1,&writeRequest);
                    close(socket);
                    printbackupDetails(status, __FAILURE__,false,true);
                }
                else{
                    int backup1socket = -1,backup2socket = -1;
                    int writeSocketServer = createSocketAndConnect(destStorageServer);
                    // status = sendWriteRequestToServer(writeSocketServer, &writeRequest, pipe);
                    // sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    // logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    // printCreateDeleteRequest(request, status,true);
                    if(flag==4){
                        printbackupDetails(__FAILURE__, __FAILURE__,false,false);
                    }
                    else if(flag==5){
                        // int socket = createSocketAndConnect(destStorageServer+1);
                        // status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                        // close(socket);
                        backup1Socket = createSocketAndConnect(destStorageServer+1);
                        printbackupDetails(status, __FAILURE__,false,false);
                    }
                    else if(flag==6){
                        // int socket = createSocketAndConnect(destStorageServer+2);
                        // status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                        // close(socket);
                        backup2Socket = createSocketAndConnect(destStorageServer+2);
                        printbackupDetails(__FAILURE__, status,false,false);
                    }
                    else if(flag==7){
                        // int socket = createSocketAndConnect(destStorageServer+1);
                        // status = sendWriteRequestToServer(socket, &writeRequest, pipe);
                        // close(socket);
                        // socket = createSocketAndConnect(destStorageServer+2);
                        // int backup2 = sendWriteRequestToServer(socket, &writeRequest, pipe);
                        // close(socket);
                        backup1Socket = createSocketAndConnect(destStorageServer+1);
                        backup2Socket = createSocketAndConnect(destStorageServer+2);
                        printbackupDetails(status, status,false,false);
                        // printbackupDetails(status, backup2,false,false);
                    }
                    printf("WRITING\n");
                    status = write_file(-1,"abc",readSocketServer,writeSocketServer,backup1Socket,backup2Socket,&writeRequest);
                    sendCreateDeleteCopyStatus(newClientSocket, status, request);
                    logMessageSent(newClientSocket, __STATUS__, (void**)&status, false, request->opType,true);
                    printCreateDeleteRequest(request, status,true);
                    close(writeSocketServer);
                }

                if(status != __OK__){
                    free(srcStorage);
                    free(destStorage);
                    return NULL;
                }
            }
            else{
                printf(YELLOW"==================== DIRECTORY TO DIRECTORY COPY ====================\n"RESET);

                if(connected[destStorageServer]==false){
                    if(connected[destStorageServer+1]==true && connected[destStorageServer+2]==true){
                        // redirect to backup 1 and create backup in backup 2
                        int status = makeCopyRequest(request, srcStorageServer,destStorageServer+1,false,destStorageServer);
                        int backup2 = makeCopyRequest(request, srcStorageServer,destStorageServer+2,true,destStorageServer);
                        printbackupDetails(status, backup2,true,false);
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                    else if(connected[destStorageServer+1]==true){
                        // redirect to backup 1
                        int status = makeCopyRequest(request, srcStorageServer,destStorageServer+1,false,destStorageServer);
                        printbackupDetails(status, __FAILURE__,true,false);
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                    else if(connected[destStorageServer+2]==true){
                        // redirect to backup 2
                        int status = makeCopyRequest(request, srcStorageServer,destStorageServer+2,false,destStorageServer);
                        printbackupDetails(status, __FAILURE__,false,true);
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                    else{
                        int status = __FAILURE__;
                        sendCreateDeleteCopyStatus(newClientSocket, status, request);
                        free(srcStorage);
                        free(destStorage);
                        continue;
                    }
                }
                int status = makeCopyRequest(request, srcStorageServer,destStorageServer,false,destStorageServer);
                int backup1=__FAILURE__,backup2=__FAILURE__;
                if(destStorageServer+1 < __MAX_STORAGE_SERVERS__ && connected[destStorageServer+1]==true){
                    backup1 = makeCopyRequest(request, srcStorageServer,destStorageServer+1,true,destStorageServer);
                }
                if(destStorageServer+2 < __MAX_STORAGE_SERVERS__ && connected[destStorageServer+2]==true){
                    backup2 = makeCopyRequest(request, srcStorageServer,destStorageServer+2,true,destStorageServer);
                }
                printbackupDetails(backup1, backup2,false,false);

                printf("Status: %d\n", status);
                sendCreateDeleteCopyStatus(newClientSocket, status, request);
                if(status != __OK__){
                    printf(RED"==================== COPY FAILED ====================\n" RESET);
                }
                else{
                    printf(GREEN"==================== COPY SUCCESS ====================\n" RESET);
                }
            }
            free(srcStorage);
            free(destStorage);
      
        }
        else if(strcmp(request->opType,"LIST")==0){
            // generate the list of all paths in the trie
            char dirPath[8192];
            strcpy(dirPath,"");
            char finalList[8192]="";
            createStringinListAllPaths(finalList, root, dirPath);
            if(send(newClientSocket, finalList, strlen(finalList)+1, 0) <= 0){
                printf(BRIGHT_RED"[-] Client %d disconnected11\n"RESET, numberOfClients);
                return NULL;
            }
            printf("Final List: %s\n", finalList);
            printServedListRequest(request, finalList);
            logMessageSent(newClientSocket, __STRING__, (void**)&finalList, false, request->opType,true);
        }
        // else{
        //     int index = atoi(stringServerNumber);
        //     if(connected[index] == false){
        //         if(connected[0] == true){
        //             index = 1;
        //         }
        //         else if(connected[1] == true){
        //             index = 0;
        //         }
        //         else{
        //             printf("Could not get data from any storage server\n");
        //             server.portNumber = -1;
        //         }
        //     }
        //     pthread_mutex_lock(&ssListLock);
        //     if(index==-1){
        //         server.portNumber = -1;
        //     }
        //     else{
        //         server.portNumber = listStorgeServers[index].clientPort;
        //         strcpy(server.networkIP, listStorgeServers[index].networkIP);
        //     }
        //     pthread_mutex_lock(&ssListLock);

        //     struct TrieNode* node = findNode(root, request->path);
        //     if(strcmp(request->opType, "READ") == 0){
        //         requestReadAccess(node);
        //     }
        //     if(strcmp(request->opType, "WRITE") == 0){
        //         requestWritePermission(node);
        //     }
        // }
        // if(send(newClientSocket, &server, sizeof(SERVER_CONFIG), 0) <= 0){
        //     printf(BRIGHT_RED"[-] Client %d disconnected1\n"RESET, numberOfClients);
        //     return NULL;
        // }
        // if(strcmp(requestCopy.opType, "READ") == 0){
        //     int ack;
        //     recv(newClientSocket, &ack, sizeof(int), 0);
        //     releaseReadAccess(findNode(root, requestCopy.path));
        // }
        // if(strcmp(requestCopy.opType, "WRITE") == 0){
        //     int ack;
        //     recv(newClientSocket, &ack, sizeof(int), 0);
        //     releaseWriteAccess(findNode(root, requestCopy.path));
        // }
    }
    pthread_mutex_lock(&clientLock);
    numberOfClients--;
    printf(BRIGHT_RED"[-] Client %d disconnected2\n"RESET, numberOfClients);
    pthread_mutex_unlock(&clientLock);
    return NULL;
}

void* listenClientThread(){
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1){
        perror("socket() failed");
        return NULL;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, '\0', sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(__NAMING_CLIENT_PORT__);
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
        perror("bind() failed");
        close(serverSocket);
        return NULL;
    }

    if(listen(serverSocket, 10) == -1){
        perror("listen() failed");
        close(serverSocket);
        return NULL;
    }
    // printf(BRIGHT_CYAN"Naming server listening for clients on port %d\n"RESET, __NAMING_CLIENT_PORT__);
    printf(BRIGHT_CYAN"\n=======================================\n" YELLOW"🚀 Naming Server is Ready to Connect! \n"PINK"---------------------------------------\n"YELLOW"📡 Listening for Clients on: Port %d\n"CYAN"=======================================\n\n"RESET,__NAMING_CLIENT_PORT__);


    while(true){
        int clientSocket;
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        if(numberOfClients == __MAX_CLIENTS__){
            continue;
        }
        if((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength)) == -1){
            perror("accept() failed");
            continue;
        }
        pthread_mutex_lock(&clientLock);
        numberOfClients++;
        // printf(BRIGHT_GREEN"[+] Client %d connected\n"RESET, numberOfClients);
        printf(BRIGHT_GREEN"[+] " YELLOW "Client #%d successfully connected\n" RESET, numberOfClients);
        pthread_create(&listOfClientThreads[numberOfClients-1], NULL, clientThreadFunction, (void*)&clientSocket);
        pthread_mutex_unlock(&clientLock);
    }
    close(serverSocket);
    return NULL;
}


#include <signal.h>
int main(){
    // create a cache
    
    signal(SIGPIPE, SIG_IGN);

    cache = makeLRUCache(__MAX_LRU_CACHE_SIZE__);
    // initiate the storage server list
    initStorageServerList();

    // create a trie
    root = createTrieNode();

    pthread_t checkServer;
    if(pthread_create(&checkServer, NULL, listenStorageServerThread, NULL)!=0){
        perror("pthread_create() failed");
        return 0;
    }

    
    pthread_t checkClient;
    if(pthread_create(&checkClient, NULL, listenClientThread, NULL)!=0){
        perror("pthread_create() failed");
        return 0;
    }
    // pthread_join(checkServer,NULL);
    pthread_join(checkClient, NULL);
    cleanupTrie(root);
    deallocateLRUCache(cache);

    return 0;
}

// gcc namingServer/namingServer.c lru/lru.c log/log.c caching/caching.c path/path.c tries/tries.c -o main -pthread -g 