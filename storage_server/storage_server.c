#include "storage_server.h"
#include "lib.h"
void connect_with_nm_server();
void *client_listen_thread_function();
void *nm_listen_thread_function();
int perform_op(int clientSocket, FILE_REQUEST_STRUCT *fileReq);

#define BUFFER_SIZE 8192

SERVER_SETUP_INFO ssInitInfo;

int NM_SS_PORT;
int _CLIENT_PORT_;
char *NM_SERVER_IP = "127.0.0.1";
int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Usage: %s <nm server port> <client port> <nm ip>\n", argv[0]);
        exit(0);
    }

    NM_SS_PORT = atoi(argv[1]);
    _CLIENT_PORT_ = atoi(argv[2]);
    if (argc > 3)
    {
        NM_SERVER_IP = argv[3];
    }

    strcpy(ssInitInfo.networkIP, get_ip_address());
    ssInitInfo.clientPort = _CLIENT_PORT_;
    ssInitInfo.namingServerPort = NM_SS_PORT;
    ssInitInfo.isBackup = false;

    connect_with_nm_server();

    pthread_t nm_listen_thread;
    pthread_create(&nm_listen_thread, NULL, nm_listen_thread_function, NULL);

    client_listen_thread_function();
    pthread_join(nm_listen_thread, NULL);
    return 0;
}

int connect_to_server(const char *server_ip, int port) {
    int sock = 0;
    struct sockaddr_in server_address;

    // Step 1: Create a socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation error");
        return -1;
    }
    printf("[+] Socket created successfully.\n");

    // Step 2: Configure the server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    // Convert the IP address to binary form and set it
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        perror("Invalid IP address or address not supported");
        close(sock);
        return -1;
    }
    printf("[+] Server IP address set to %s:%d.\n", server_ip, port);

    // Step 3: Connect to the server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection to server failed");
        close(sock);
        return -1;
    }
    printf("[+] Connected to the server successfully.\n");

    return sock; // Return the socket file descriptor
}

void *handle_nm(void *args)
{
    int nm_socket = *(int *)args;
    FILE_REQUEST_STRUCT req = *(FILE_REQUEST_STRUCT *)recieve_by_ss(nm_socket, __FILE_REQUEST__);
    int ERRCODE;
    ERRCODE = perform_op(nm_socket, &req);
    if (send(nm_socket, &ERRCODE, sizeof(int), 0) == -1)
    {
        perror("send");
    }
    printf("ERROR CODE sent to nm: %d",ERRCODE);
}

void *nm_listen_thread_function()
{
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1)
    {
        perror("Couldn't create socket");
        return NULL;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ssInitInfo.namingServerPort);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    inet_ntop(AF_INET, &(server_addr.sin_addr), ssInitInfo.networkIP, INET_ADDRSTRLEN);

    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind failed");
        close(listen_socket);
        return NULL;
    }

    if (listen(listen_socket, BACKLOG) == -1)
    {
        perror("Listen failed");
        close(listen_socket);
        return NULL;
    }

    printf(BLUE "Listening for instructions from Naming Server on Port: %d\n" RESET, ssInitInfo.namingServerPort);

    while (1)
    {
        int nm_socket = accept(listen_socket, NULL, NULL);
        if (nm_socket < 0)
        {
            perror("Accept failed");
            close(listen_socket);
            continue;
        }
        printf(GREEN "[+] nm server connected\n" RESET);
        pthread_t handle_connection_thread;
        pthread_create(&handle_connection_thread, NULL, handle_nm, (void *)&nm_socket);
    }

    close(listen_socket);

    return NULL;
}

void *failure_detection(void *args)
{
    int nmfd = *((int *)args);

    while (true)
    {
        char *failure_buffer = recieve_by_ss(nmfd, __STRING__);
        if (strcmp(failure_buffer, "PING") == 0)
        {
            // printf(GREEN "I am alive\n" RESET);
            send_by_ss(nmfd, "PONG", __STRING__);
        }
    }
}

void connect_with_nm_server()
{
    int socket_fd;
    struct sockaddr_in server_address;

    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setup server address struct
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NM_PORT);

    // Convert IP address to binary form and validate it
    if (inet_pton(AF_INET, NM_SERVER_IP, &(server_address.sin_addr)) <= 0)
    {
        perror("Invalid IP address");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Connect to the Naming Server
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Connection to Naming Server failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf(GREEN "[+] Connected to Naming Server.\n" RESET);

    ssInitInfo.Paths = NULL;
    

    send_by_ss(socket_fd, (void *)&ssInitInfo, __SERVER_INIT_INFO__);

    char result[100000] = "";
    const char *initial_path = "storage";
    seek_recursive_concatenate(initial_path, "storage", result);
    printf("send -> %s\n", result);

    send_by_ss(socket_fd, result, __STRING__);

    pthread_t failure_detection_thread;

    pthread_create(&failure_detection_thread, NULL, failure_detection, (void *)&socket_fd);

    // close(socket_fd);
}

void *client_listen_thread_function()
{
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1)
    {
        perror("Socket creation failed");
        return NULL;
    }
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(_CLIENT_PORT_);

    int opt = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        close(listenSocket);
        return NULL;
    }

    if (bind(listenSocket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Bind failed");
        close(listenSocket);
        return NULL;
    }

    if (listen(listenSocket, BACKLOG) == -1)
    {
        perror("Listen failed");
        close(listenSocket);
        return NULL;
    }

    printf(HI_BLUE "Listening for instructions from clients on Port: %d\n" RESET, _CLIENT_PORT_);

    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (true)
    {
        int clientSocket;

        clientSocket = accept(listenSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0)
        {
            perror("Accept failed");
            close(listenSocket);
            continue;
        }
        else
        {
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            printf("Client connected: IP = %s, Port = %d\n", clientIP, ntohs(clientAddr.sin_port));
        }

        pthread_t handle_client_thread;
        pthread_create(&handle_client_thread, NULL, handle_client, (void *)&clientSocket);
    }
}

int perform_op(int clientSocket, FILE_REQUEST_STRUCT *fileReq)
{
    // printf("perform op: %s\n",fileReq->opType);
    // printf("perform op: %s\n",fileReq->path);
    int ERRCODE;
    if (strcmp(fileReq->opType, "READ") == 0)
    {
        ERRCODE = read_file(clientSocket, fileReq->path);
        return ERRCODE;
    }
    else if (strcmp(fileReq->opType, "WRITE") == 0)
    {
        printf("entered write\n");
        // char *dataToWrite = (char *)recieve_by_ss(clientSocket, __STRING__);
        // printf("bool perform :%d\n",fileReq->sync);
        ERRCODE = write_file(clientSocket, fileReq->path,fileReq->sync);
        return ERRCODE;
    }
    else if (strcmp(fileReq->opType, "CREATE") == 0)
    {
        // printf("bool %d\n",fileReq->isDirectory);
        ERRCODE = create_file_dir(fileReq->path, fileReq->isDirectory);
        return ERRCODE;
    }
    else if (strcmp(fileReq->opType, "DELETE") == 0)
    {
        // printf("hello\n");
        ERRCODE = delete_file_dir(fileReq->path, fileReq->isDirectory);
        return ERRCODE;
    }
    else if (strcmp(fileReq->opType, "COPY") == 0)
    {
    }
    else if (strcmp(fileReq->opType, "GET") == 0)
    {
        FILE_METADATA fileMetaData;
        ERRCODE = get_file(fileReq->path, &fileMetaData);

        if (ERRCODE == SUCCESS)
        {

            // printf("file metadata send\n");
            send_by_ss(clientSocket, &fileMetaData, 10);
            // printf("file metadata send\n");
        }

        return ERRCODE;
    }
    else if (strcmp(fileReq->opType, "STREAM") == 0)
    {
        // printf("hello\n");
        ERRCODE = stream_audio(clientSocket, fileReq->path);
        return ERRCODE;
    }

    return INVALID_OP;
}

void *handle_client(void *args)
{
    int clientSocket = *((int *)args);

    FILE_REQUEST_STRUCT *fileReq = (FILE_REQUEST_STRUCT *)recieve_by_ss(clientSocket, __FILE_REQUEST__);
    // printf("%s\n",fileReq->opType);

    if (fileReq == NULL)
    {
        return NULL;
    }

    int ERRCODE = perform_op(clientSocket, fileReq);

   
        printf("errcode sent : %d\n",ERRCODE);
        // sleep(1);
        if (send(clientSocket, &ERRCODE, sizeof(int), 0) <= 0)
        {
            perror("send");
        }
  
    close(clientSocket);
    return NULL;
}
