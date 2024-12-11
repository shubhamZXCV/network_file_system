#include"log.h"
#include"../tries/tries.h"
#include"../globals.h"

void logMessageRecieve(int socket,int dataType,void** messageStruct,bool fromClient){

    void* message = *messageStruct;
    char ip[INET_ADDRSTRLEN];
    int portNumber;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if(getsockname(socket,(struct sockaddr*)&addr,&len) < 0){
        perror("getsockname() failed");
        return;
    }

    inet_ntop(AF_INET,&addr.sin_addr,ip,INET_ADDRSTRLEN);
    portNumber = ntohs(addr.sin_port);

    FILE* fptr = fopen("../logs.txt","a+");
    if(fptr == NULL){
        perror("fopen() failed");
        return;
    }
    if(fromClient){
        fprintf(fptr, YELLOW"=== New Message Received from Client ===\n"RESET);
    }
    else{
        fprintf(fptr, YELLOW"=== New Message Received from Server ===\n"RESET);
    }
    fprintf(fptr, GREEN"Source IP Address: %s\n", ip);
    fprintf(fptr, "Source Port: %d\n"RESET, portNumber);

    if(dataType == __STRING__){
        char* messageString = (char*)message;
        if(message==NULL){
            fprintf(fptr, CYAN"Message Type: STRING\n"RESET);
            fprintf(fptr, "Content: (NULL)\n");
        }
        else{
            fprintf(fptr, CYAN"Message Type: STRING\n"RESET);
            fprintf(fptr, "Content: %s\n", messageString);
        }
    }
    else if(dataType == __FILE_REQUEST__){
        FILE_REQUEST_STRUCT* messageString = (FILE_REQUEST_STRUCT*)message;
        fprintf(fptr, CYAN"Message Type: FILE REQUEST\n"RESET);
        fprintf(fptr, "Operation Type: %s\n", messageString->opType);
        fprintf(fptr, "Requested File Path: %s\n", messageString->path);
    }
    else if(dataType == __SERVER_INIT_INFO__){
        SERVER_SETUP_INFO* messageString = (SERVER_SETUP_INFO*)message;
        fprintf(fptr, CYAN"Message Type: SERVER INITIALIZATION INFO\n"RESET);
        if(messageString->networkIP==NULL){
            fprintf(fptr, "Network IP: (NULL)\n");
            fprintf(fptr, "Naming Server Port: (NULL)\n");
            fprintf(fptr, "Client Communication Port: (NULL)\n");
        }
        else{
            fprintf(fptr, "Network IP: %s\n", messageString->networkIP);
            fprintf(fptr, "Naming Server Port: %d\n", messageString->namingServerPort);
            fprintf(fptr, "Client Communication Port: %d\n", messageString->clientPort);
        }
    }
    else if(dataType == __SERVER_INFO__){
        SERVER_CONFIG* messageString = (SERVER_CONFIG*)messageStruct;
        fprintf(fptr, CYAN"Message Type: SERVER CONFIGURATION\n"RESET);
        fprintf(fptr, "Network IP: %s\n", messageString->networkIP);
        fprintf(fptr, "Port Number: %d\n", messageString->portNumber);
    }
    else if(dataType == __STATUS__){
        int* messageString = (int*)messageStruct;
        fprintf(fptr, CYAN"Message Type: STATUS\n"RESET);
        if(*(int*)messageString==__OK__) fprintf(fptr, "Status: OK\n");
        else fprintf(fptr, "Status: %d\n", *(int*)messageString);
    }
    else{
        fprintf(fptr, RED"Message Type: UNKNOWN\n");
        fprintf(fptr, "Error: Unrecognized data type\n"RESET);
    }
    fprintf(fptr, YELLOW"=== End of Message ===\n\n"RESET);

    fclose(fptr);
}
// function to log the message sent
void logMessageSent(int socket,int dataType,void** messageStruct,bool isNull,char* request,bool isClient){

    void* message = *messageStruct;
    char ip[INET_ADDRSTRLEN];
    int portNumber;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if(getsockname(socket,(struct sockaddr*)&addr,&len) < 0){
        perror("getsockname() failed");
        return;
    }

    inet_ntop(AF_INET,&addr.sin_addr,ip,INET_ADDRSTRLEN);
    portNumber = ntohs(addr.sin_port);

    FILE* fptr = fopen("../logs.txt","a+");
    if(fptr == NULL){
        perror("fopen() failed");
        return;
    }
    if(isClient){
        fprintf(fptr, YELLOW"=== New Message Sent to Client ===\n"RESET);
    }
    else{
        fprintf(fptr, YELLOW"=== New Message Sent to Server ===\n"RESET);
    }
    fprintf(fptr, GREEN"Destination IP Address: %s\n", ip);
    fprintf(fptr, "Destination Port: %d\n"RESET, portNumber);
    fprintf(fptr, "Operation Type Served: %s\n", request);

    if(dataType == __STRING__){
        char* messageString = (char*)messageStruct;
        if(message==NULL){
            fprintf(fptr, CYAN"Message Type: STRING\n"RESET);
            fprintf(fptr, "Content: (NULL)\n");
        }
        else{
            fprintf(fptr, CYAN"Message Type: STRING\n"RESET);
            fprintf(fptr, "Content: %s\n", messageString);
        }
    }
    else if(dataType == __FILE_REQUEST__){
        FILE_REQUEST_STRUCT* messageString = (FILE_REQUEST_STRUCT*)message;
        if(messageString==NULL){
            fprintf(fptr, CYAN"Message Type: FILE REQUEST\n"RESET);
            fprintf(fptr, "Operation Type: (NULL)\n");
            fprintf(fptr, "Requested File Path: (NULL)\n");
        }
        else{
            fprintf(fptr, CYAN"Message Type: FILE REQUEST\n"RESET);
            fprintf(fptr, "Operation Type: %s\n", messageString->opType);
            fprintf(fptr, "Requested File Path: %s\n", messageString->path);
        }
    }
    else if(dataType == __SERVER_INIT_INFO__){
        SERVER_SETUP_INFO* messageString = (SERVER_SETUP_INFO*)message;
        if(messageString==NULL){
            fprintf(fptr, CYAN"Message Type: SERVER INITIALIZATION INFO\n"RESET);
            fprintf(fptr, "Network IP: (NULL)\n");
            fprintf(fptr, "Naming Server Port: (NULL)\n");
            fprintf(fptr, "Client Communication Port: (NULL)\n");
        }
        else{
            fprintf(fptr, CYAN"Message Type: SERVER INITIALIZATION INFO\n"RESET);
            fprintf(fptr, "Network IP: %s\n", messageString->networkIP);
            if(isNull) fprintf(fptr, "Naming Server Port: (NULL)\n");
            else fprintf(fptr, "Naming Server Port: %d\n", messageString->namingServerPort);
            fprintf(fptr, "Client Communication Port: %d\n", messageString->clientPort);
        }
    }
    else if(dataType == __SERVER_INFO__){
        SERVER_CONFIG* messageString = (SERVER_CONFIG*)messageStruct;
        if(messageString==NULL){
            fprintf(fptr, CYAN"Message Type: SERVER CONFIGURATION\n"RESET);
            fprintf(fptr, "Network IP: (NULL)\n");
            fprintf(fptr, "Port Number: (NULL)\n");
            fprintf(fptr, "Error Code: (NULL)\n");   
        }
        else{
            fprintf(fptr, CYAN"Message Type: SERVER CONFIGURATION\n"RESET);
            if(messageString->networkIP==NULL) fprintf(fptr, "Network IP: (NULL)\n");
            else fprintf(fptr, "Network IP: %s\n", messageString->networkIP);
            fprintf(fptr, "Port Number: %d\n", messageString->portNumber);
            fprintf(fptr, "Error Code: %d\n", messageString->errorCode);
        }
    }
    else if(dataType == __STATUS__){
        int* messageString = (int*)messageStruct;
        if(messageString==NULL){
            fprintf(fptr, CYAN"Message Type: STATUS\n"RESET);
            fprintf(fptr, "Status: (NULL)\n");
        }
        else{
            fprintf(fptr, CYAN"Message Type: STATUS\n"RESET);
            if(*(int*)messageString==__OK__) fprintf(fptr, "Status: OK\n");
            else fprintf(fptr, "Status: %d\n", *messageString);
        }
    }
    else{
        fprintf(fptr, RED"Message Type: UNKNOWN\n");
        fprintf(fptr, "Error: Unrecognized data type\n"RESET);
    }
    fprintf(fptr, YELLOW"=== End of Message ===\n\n"RESET);
    
    fclose(fptr);
}





