

#include"lib.h"
#include"network.h"
#define BUFFER_SIZE 8192
void *recieve_by_ss(int clientSocket, int dataStruct)
{
    void *structRecieved;

    if (dataStruct == __FILE_REQUEST__)
    {
        structRecieved = malloc(sizeof(FILE_REQUEST_STRUCT));
        if (recv(clientSocket, structRecieved, sizeof(FILE_REQUEST_STRUCT), 0) == -1)
        {
            perror("recv failed");
            return NULL;
        }
        return structRecieved;
    }
    else if (dataStruct == __STRING__)
    {
        structRecieved = malloc(sizeof(char) * 8192);
        int byte_recv = recv(clientSocket, structRecieved, 8192, 0);
        if (byte_recv > 0)
        {
            // Ensure proper null-termination of the received string
            ((char *)structRecieved)[byte_recv] = '\0';
        }
        else
        {
            perror("recv failed");
            return NULL;
        }
        return structRecieved;
    }
}

void send_by_ss(int socket,void * dataToSend,int dataType){
    int dataSize;
    if(dataType==__STRING__){
        dataSize = strlen(dataToSend)+1;
    }
    else if(dataType == __FILE_REQUEST__){
        dataSize = sizeof(FILE_REQUEST_STRUCT);

    }
    else if(dataType == __SERVER_INIT_INFO__){
        dataSize = sizeof(SERVER_SETUP_INFO);

    }
    else{
        printf("send by ss\n");
        dataSize=sizeof(FILE_METADATA);
    }

    if(send(socket,dataToSend,dataSize,0)==-1){
        perror("send failed");
        return;  
    }
}

char * get_ip_address() {
    struct ifaddrs *ifaddr, *ifa;
    char * ip=(char*)malloc(sizeof(char)*INET_ADDRSTRLEN);
    // char ip[INET_ADDRSTRLEN];

    // Get list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Loop through all the interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        // Check for IPv4 address
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;

            // Convert IP address to a readable string
            inet_ntop(AF_INET, &(addr->sin_addr), ip, sizeof(char)*INET_ADDRSTRLEN);

            // Skip the loopback address (127.0.0.1)
            if (strcmp(ip, "127.0.0.1") != 0) {
                printf("Interface: %s\n", ifa->ifa_name);
                printf("IP Address: %s\n", ip);
            }
        }
    }

    // Free the memory allocated by getifaddrs
    freeifaddrs(ifaddr);
    return ip;
}







