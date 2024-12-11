#include "client.h"
#include <alsa/asoundlib.h>
#include <mpv/client.h>
#include <sys/stat.h> // Needed for mkfifo
#include <fcntl.h>    // Needed for open()

#define REMOVE_SIZE 18
#define FIFO_PATH "/tmp/mpv_fifo"

char *NAMING_SERVER_IP = "127.0.0.1";

void handle_error(int errcode)
{
    switch (errcode)
    {
    case INVALID_PATH:
        fprintf(stderr, RED "Error: Invalid file path provided.\n" RESET);
        break;
    case WRITE_ERR:
        fprintf(stderr, RED "Error: Failed to write to the file or stream.\n" RESET);
        break;
    case DELETE_ERR:
        fprintf(stderr, RED "Error: Could not delete the requested file.\n" RESET);
        break;
    case CREATE_ERR:
        fprintf(stderr, RED "Error: Failed to create the file or resource.\n" RESET);
        break;
    case STREAM_ERR:
        fprintf(stderr, RED "Error: Streaming operation failed.\n" RESET);
        break;
    case INVALID_OP:
        fprintf(stderr, RED "Error: Invalid operation requested.\n" RESET);
        break;
    case READ_ERR:
        fprintf(stderr, RED "Error: Failed to read from the file or stream.\n" RESET);
        break;
    case ALREADY_EXIST:
        fprintf(stderr, YELLOW "Warning: The resource already exists.\n" RESET);
        break;
    case SUCCESS:
        fprintf(stdout, GREEN "Operation completed successfully.\n" RESET);
        break;
    case FAILURE:
        fprintf(stderr, RED "Error: Operation failed due to an unknown error.\n" RESET);
        break;
    case CONNECT_ERR:
        fprintf(stderr, RED "Error: Failed to connect to the server.\n" RESET);
        break;
    case FIFO_WRITE_ERR:
        fprintf(stderr, RED "Error: Failed to write to FIFO.\n" RESET);
        break;
    case OPEN_FIFO_ERR:
        fprintf(stderr, RED "Error: Failed to open FIFO.\n" RESET);
        break;
    case SEND_ERR:
        fprintf(stderr, RED "Error: Failed to send data.\n" RESET);
        break;
    case MPV_FIFO_ERR:
        fprintf(stderr, RED "Error: MPV FIFO error encountered.\n" RESET);
        break;
    case MPV_INIT_ERR:
        fprintf(stderr, RED "Error: MPV initialization failed.\n" RESET);
        break;
    case MPV_CREATE_ERR:
        fprintf(stderr, RED "Error: Failed to create MPV instance.\n" RESET);
        break;
    case MKFIFO_ERR:
        fprintf(stderr, RED "Error: Failed to create named FIFO.\n" RESET);
        break;
    case UNLINK_FIFO_ERR:
        fprintf(stderr, RED "Error: Failed to unlink FIFO.\n" RESET);
        break;
    case STORAGE_SERVER_ERR:
        fprintf(stderr, RED "Error: Storage server error encountered.\n" RESET);
        break;
    case NAMING_SERVER_ERR:
        fprintf(stderr, RED "Error: Naming server error encountered.\n" RESET);
        break;
    case INVALID_METADATA_ERR:
        fprintf(stderr, RED "Error: Invalid metadata provided.\n" RESET);
        break;
    case INVALID_SERVER_INFO_ERR:
        fprintf(stderr, RED "Error: Invalid server information provided.\n" RESET);
        break;
    case UNKNOWN_ERR:
        fprintf(stderr, RED "Error: An unknown error occurred.\n" RESET);
        break;
    case PERMISSION_DENIED_ERR:
        fprintf(stderr, RED "Error: Permission denied.\n" RESET);
        break;
    case PATH_NOT_FOUND_ERR:
        fprintf(stderr, RED "Error: Path not found.\n" RESET);
        break;
    case DATA_SEND_ERR:
        fprintf(stderr, RED "Error: Failed to send data to the server.\n" RESET);
        break;
    case RECV_ERR:
        fprintf(stderr, RED "Error: Failed to receive data.\n" RESET);
        break;
    case ERR_PATH_NOT_EXIST:
        fprintf(stderr, RED "Error: Path does not exist in Storage Servers.\n" RESET);
        break;
    case ERR_INVALID_FILE_FORMAT:
        fprintf(stderr, RED "Error: The format is invalid\n" RESET);
        break;
    case ERR_SERVER_DOWN:
        fprintf(stderr, RED "Error: The server is down\n" RESET);
        break;
    default:
        fprintf(stderr, RED "Error: Unknown error code: %d.\n" RESET, errcode);
        break;
    }
}

void format_file_name(char *fileName)
{
    // Ensure the file name starts with "./"
    if (strncmp(fileName, "./", 2) != 0)
    {
        char temp[256];                                 // Temporary buffer to hold the formatted file name
        snprintf(temp, sizeof(temp), "./%s", fileName); // Prepend "./" to the file name
        strncpy(fileName, temp, 256);                   // Copy back to fileName
    }

    // Remove trailing "/" if present
    size_t len = strlen(fileName);
    if (len > 0 && fileName[len - 1] == '/')
    {
        fileName[len - 1] = '\0';
    }
}

// Connect to the server (storage or naming server, this is the common part in both ways)
int connect_to_server(const char *ip, int port)
{
    int socket_fd;
    struct sockaddr_in server_address;

    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        handle_error(CREATE_ERR); // Error code for socket creation failure
        return FAILURE;
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &(server_address.sin_addr)) <= 0)
    {
        handle_error(INVALID_PATH); // Error code for invalid IP address
        close(socket_fd);
        return FAILURE;
    }

    // Connect to the server
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        handle_error(CONNECT_ERR); // Error code for connection failure
        close(socket_fd);
        return FAILURE;
    }

    handle_error(SUCCESS); // Success message
    return socket_fd;
}

// Connect to the naming server to get the storage server info or just validation for CREATE/DELETE
int connect_to_naming_server(const char *nm_ip, int nm_port, SERVER_CONFIG *storage_server_info, const char *filePath, const char *operation, int isDirectory, bool sync,int * nmSocket) {
    int socket_fd;
    FILE_REQUEST_STRUCT request;

    // Connect to the naming server
    socket_fd = connect_to_server(nm_ip, nm_port);
    if (socket_fd == -1) {
        handle_error(CONNECT_ERR); // Error for connection failure
        return FAILURE;
    }
    if(nmSocket!=NULL) *nmSocket= socket_fd;
    // printf(">> SOCKET %d\n",socket_fd);
    // Prepare the request for the naming server
    strcpy(request.opType, operation);                         // Operation type ("CREATE", "DELETE", etc.)
    strncpy(request.path, filePath, sizeof(request.path) - 1); // File path
    request.path[sizeof(request.path) - 1] = '\0';             // Ensure null termination

    if (strcmp(operation, "CREATE") == 0 || strcmp(operation, "DELETE") == 0) {
        request.isDirectory = isDirectory;
    }

    // Send the request to the naming server
    if (send(socket_fd, &request, sizeof(request), 0) == -1) {
        handle_error(WRITE_ERR); // Error for sending data
        close(socket_fd);
        return FAILURE;
    }
    printf(GREEN "[+] Sent %s request to naming server\n" RESET, operation);

    // Handle CREATE/DELETE
    if (strcmp(operation, "CREATE") == 0 || strcmp(operation, "DELETE") == 0) {
        int response;
        ssize_t bytesReceived = recv(socket_fd, &response, sizeof(response), 0);
        if (bytesReceived == -1 || bytesReceived == 0) {
            perror("Error receiving acknowledgment from naming server");
            close(socket_fd);
            return FAILURE;
        }

        if (response == SUCCESS) {
            printf(GREEN "[+] Operation %s completed successfully.\n" RESET, operation);
        } else {
            printf(RED "[-] Naming server validation failed for the %s operation.\n" RESET, operation);
        }

        close(socket_fd);
        return response == SUCCESS ? SUCCESS : FAILURE;
    }

    // Handle WRITE
    if (strcmp(operation, "WRITE") == 0) {
        for (int i = 0; i < 3; i++) {
            ssize_t bytesReceived = recv(socket_fd, &storage_server_info[i], sizeof(SERVER_CONFIG), 0);
            if (bytesReceived == -1 || bytesReceived == 0) {
                perror("Error receiving storage server info");
                close(socket_fd);
                return FAILURE;
            }

            // printf(GREEN "[+] Received storage server info: IP = %s, Port = %d\n" RESET, storage_server_info[i].networkIP, storage_server_info[i].portNumber);
        }
        if(nmSocket!=NULL) *nmSocket= socket_fd;
        // close(socket_fd);
        return SUCCESS;
    }

    // Handle other operations
    ssize_t bytesReceived = recv(socket_fd, storage_server_info, sizeof(SERVER_CONFIG), 0);
    if (bytesReceived == -1 || bytesReceived == 0) {
        perror("Error receiving storage server info");
        close(socket_fd);
        return FAILURE;
    }

    if (strlen(storage_server_info->networkIP) == 0 || storage_server_info->portNumber == -1) {
        handle_error(INVALID_PATH); // Invalid server info
        close(socket_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Received storage server info: IP = %s, Port = %d\n" RESET, storage_server_info->networkIP, storage_server_info->portNumber);
    // close(socket_fd);
    if(nmSocket!=NULL)
    {
    *nmSocket= socket_fd;

    }
    return SUCCESS;

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

    return 0; // Success
}
// Function to read exactly `size` bytes
int recv_full(int sock_fd, void* buffer, size_t size) {
    size_t total_received = 0;
    char* ptr = (char*)buffer;

    while (total_received < size) {
        int bytes_received = recv(sock_fd, ptr + total_received, size - total_received, 0);
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

int read_from_file(const char *fileName, char *buffer)
{
    int socket_fd;
    SERVER_CONFIG storage_server_info;
    FILE_REQUEST_STRUCT request;
    char *operation = "READ";
    int total_bytes_received = 0;
    int bytes_received = 0;
    char temp_buffer[BUFFER_SIZE]; // Temporary buffer to receive file data
    int nmSocket;
    // Connect to the naming server to get storage server details
    if (connect_to_naming_server(NAMING_SERVER_IP, NAMING_SERVER_PORT, &storage_server_info, fileName, operation, 0, true,&nmSocket) == FAILURE)
    {
        handle_error(NAMING_SERVER_ERR); // Failed to connect to the naming server or invalid response
        return FAILURE;
    }

    // Connect to the storage server using the information obtained from the naming server
    socket_fd = connect_to_server(storage_server_info.networkIP, storage_server_info.portNumber);
    if (socket_fd == -1)
    {
        handle_error(CONNECT_ERR); // Failed to connect to the storage server
        return FAILURE;
    }

    // Initialize the READ request structure
    memset(&request, 0, sizeof(request));
    strcpy(request.opType, "READ");
    strncpy(request.path, fileName, sizeof(request.path) - 1);

    // Send the READ request to the storage server
    if (send(socket_fd, &request, sizeof(request), 0) == -1)
    {
        handle_error(WRITE_ERR); // Failed to send the READ request
        close(socket_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Sent READ request for file: %s\n" RESET, fileName);



    printf("\n[+] File content:\n%s\n", buffer);

    total_bytes_received = 0;

    PACKET* pkt = (PACKET*)malloc(sizeof(PACKET));
    pkt->length = 0;
    pkt->data[0] = '\0';
    pkt->stop = 0;
    // recieve pkts from server in while loop
   // Receive packets from the server in a loop
    while (true) {
        memset(pkt, 0, sizeof(PACKET)); // Clear the packet memory

        int bytes_received = recv_full(socket_fd, pkt, sizeof(PACKET));
        if (bytes_received == -1) {
            handle_error(READ_ERR); // Failed to receive file content
            close(socket_fd);
            return FAILURE;
        } else if (bytes_received == 0) {
            // Server closed connection unexpectedly
            fprintf(stderr, "Connection closed by server.\n");
            break;
        }

        if (pkt->stop == 1) {
            // Last packet
            break;
        }

        total_bytes_received += pkt->length;
        printf("%s", pkt->data);
        fflush(stdout);
    }
    free(pkt);

    printf("\n[+] File content received successfully.\n");

    printf("Total bytes received: %d\n", total_bytes_received);

    int response;
    if(recv(socket_fd, &response, sizeof(response), 0) == -1){
        handle_error(RECV_ERR);
        close(socket_fd);
        return FAILURE;
    }
    printf("response: %d\n", response);
    handle_error(response);
    if(send(nmSocket, &response, sizeof(response), 0) == -1){
        handle_error(SEND_ERR);
        close(socket_fd);
        return FAILURE;
    }

    close(nmSocket);
  
    close(socket_fd);

    return SUCCESS;
}




int large_file(const char *fileName, bool sync) {
    int socket_fd[3]; // Array to store sockets for 3 storage servers
    SERVER_CONFIG storage_server_info[3]; // Array to store server details
    FILE_REQUEST_STRUCT request;
    request.sync = sync;
    
    PACKET packet; // Packet structure to send
    // packet.data = (char *)malloc(BUFFER_SIZE); // Allocate memory for data buffer
    if (!packet.data) {
        perror("Memory allocation failed for PACKET");
        return FAILURE;
    }

    printf("largefile %d\n", sync);
    char *operation = "WRITE";

    int successCount = 0; // Count successful writes
    int failureCount = 0; // Count failures
    int nm_socket;
    // Connect to the naming server to get the storage server details
    if (connect_to_naming_server(NAMING_SERVER_IP, NAMING_SERVER_PORT, storage_server_info, fileName, operation, 0, sync, &nm_socket) == FAILURE) {
        handle_error(NAMING_SERVER_ERR); // Failed to connect to the naming server or invalid response
        // free(packet.data);
        return FAILURE;
    }

    // Connect to the storage servers using the information obtained from the naming server
    for (int i = 0; i < 3; i++) {
        if(storage_server_info[i].portNumber!=-1){
            socket_fd[i] = connect_to_server(storage_server_info[i].networkIP, storage_server_info[i].portNumber);
            if (socket_fd[i] == -1) {
                handle_error(CONNECT_ERR); // Failed to connect to one of the storage servers
                failureCount++;
            } 
            else {
                successCount++;
            }
        }
        else{
            socket_fd[i]=-1;
        }
    }

    // If all connections failed, return failure
    if (successCount == 0) {
        printf(RED "All storage server connections failed.\n" RESET);
        // free(packet.data);
        return FAILURE;
    }

    // Initialize the WRITE request structure
    memset(&request, 0, sizeof(request));
    strcpy(request.opType, "WRITE");
    strncpy(request.path, fileName, sizeof(request.path) - 1);
    request.sync = sync;

    // Send the WRITE request to each storage server
    for (int i = 0; i < 3; i++) {
        if (socket_fd[i] != -1) {
            if (send(socket_fd[i], &request, sizeof(request), 0) == -1) {
                handle_error(WRITE_ERR); // Failed to send the WRITE request
                close(socket_fd[i]);
                failureCount++;
                socket_fd[i] = -1; // Mark this server as failed
            }
        }
    }

    FILE *file = fopen("large.txt", "r");
    if (!file) {
        perror("Error opening file");
        // free(packet.data);
        return FAILURE; // Error: File could not be opened
    }

    struct timeval start, mid, end;
    gettimeofday(&start, NULL);

    size_t bytes_read;
    int totalBytesSent = 0;

    // Read the file in chunks and send PACKETs to each storage server
    while ((bytes_read = fread(packet.data, 1, BUFFER_SIZE, file)) > 0) {
        packet.length = bytes_read;
        totalBytesSent += bytes_read;
        packet.stop = 0; // More data to come

        // Send the PACKET structure to all active storage servers
        for (int i = 0; i < 3; i++) {
            if (socket_fd[i] != -1) {
               
                if (send_full(socket_fd[i], &packet, sizeof(PACKET)) == -1) {
                    perror(RED "Error sending PACKET to storage server" RESET);
                    close(socket_fd[i]);
                    socket_fd[i] = -1; // Mark this server as failed
                    failureCount++;
                }

            }
        }
    }

    // Indicate the end of file transfer with a final PACKET
    packet.length = 0;
    packet.stop = 1;
    for (int i = 0; i < 3; i++) {
        if (socket_fd[i] != -1) {
           
            if (send_full(socket_fd[i], &packet, sizeof(PACKET)) == -1) {
            perror(RED "Error sending final PACKET to storage server" RESET);
            close(socket_fd[i]);
            socket_fd[i] = -1; // Mark this server as failed
            failureCount++;
            }
        }
    }

    gettimeofday(&mid, NULL);
    printf("Time taken to send the file: %f seconds\n",
           ((mid.tv_sec * 1000000 + mid.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);

    fclose(file); 
   
    STATUS_WRITE status = {FAILURE, FAILURE, FAILURE};
    if(socket_fd[0]!=-1)
        recv(socket_fd[0], &status.actual, sizeof(status.actual), 0);
    if(socket_fd[1]!=-1)
        recv(socket_fd[1], &status.backup1, sizeof(status.backup1), 0);
    if(socket_fd[2]!=-1)
        recv(socket_fd[2], &status.backup2, sizeof(status.backup2), 0);

    printf("status: %d\n",status.actual);
    printf("status: %d\n",status.backup1);
    printf("status: %d\n",status.backup2);
    printf(">> Nm : %d",nm_socket);
    // send(nm_socket,&status,sizeof(status),0);
    send_full(nm_socket,&status,sizeof(status));

    if(status.actual==SUCCESS){
        successCount++;
    }
    else{
        failureCount++;
    }
    if(status.backup1==SUCCESS){
        successCount++;
    }
    else{
        failureCount++;
    }
    if(status.backup2==SUCCESS){
        successCount++;
    }
    else{
        failureCount++;
    }



    gettimeofday(&end, NULL);
    printf("Time taken to complete the file operation: %f seconds\n",
           ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000.0);

    printf("Time taken to write the file: %f seconds\n",
           ((end.tv_sec * 1000000 + end.tv_usec) - (mid.tv_sec * 1000000 + mid.tv_usec)) / 1000000.0);

    printf(GREEN "Success count: %d\n" RESET, successCount);
    printf(RED "Failure count: %d\n" RESET, failureCount);

    // Return appropriate status based on success and failure counts
    if (successCount > 0 && failureCount == 0) {
        return SUCCESS; // All servers succeeded
    } else if (successCount > 0) {
        return FAILURE; // Some servers succeeded
    } else {
        return FAILURE; // All servers failed
    }
}


int count = 0;
int write_to_file(const char *fileName, const char *data) {
    count++;
    printf("Count:%d\n",count);
    int socket_fd = -1;
    SERVER_CONFIG storage_server_info[3]; // Array to hold 3 server configurations
    FILE_REQUEST_STRUCT request;
    char *operation = "WRITE";
    int data_len = strlen(data);
    int total_bytes_sent = 0;
    int chunk_size = BUFFER_SIZE; // The size of each chunk to send
    const char *data_ptr = data;
    int nmSocket;

    // Connect to naming server and retrieve storage server info
    if (connect_to_naming_server(NAMING_SERVER_IP, NAMING_SERVER_PORT, storage_server_info, fileName, operation, 0, true, &nmSocket) == FAILURE) {
        printf(RED "[-] Failed to connect to naming server.\n" RESET);
        return FAILURE;
    }

    int successCount = 0; // Count successful writes
    int failureCount = 0; // Count failures

    for (int i = 0; i < 3; i++) {
        printf(YELLOW "[*] Trying to connect to server %d: IP = %s, Port = %d\n" RESET,
               i + 1, storage_server_info[i].networkIP, storage_server_info[i].portNumber);

        // Validate the server configuration
        if (strlen(storage_server_info[i].networkIP) == 0 || storage_server_info[i].portNumber == -1) {
            printf(RED "[-] Invalid server configuration for server %d\n" RESET, i + 1);
            int res = FAILURE;
            send(nmSocket, &res, sizeof(res), 0);
            failureCount++;
            continue;
        }

        // Attempt to connect to the storage server
        socket_fd = connect_to_server(storage_server_info[i].networkIP, storage_server_info[i].portNumber);
        if (socket_fd == -1) {
            printf(RED "[-] Failed to connect to server %d\n" RESET, i + 1);
            failureCount++;
            continue;
        }

        printf(GREEN "[+] Connected to server %d\n" RESET, i + 1);

        // Initialize the WRITE request
        memset(&request, 0, sizeof(request));
        strcpy(request.opType, "WRITE");
        strncpy(request.path, fileName, sizeof(request.path) - 1);
        request.path[sizeof(request.path) - 1] = '\0';

        // Send the WRITE request to the storage server
        if (send(socket_fd, &request, sizeof(request), 0) == -1) {
            perror("Error sending WRITE request");
            close(socket_fd);
            failureCount++;
            continue;
        }

        // // Send data in chunks
        total_bytes_sent = 0;
        data_ptr = data; // Reset data pointer for each server
        PACKET* pkt = (PACKET*)malloc(sizeof(PACKET));
        pkt->length = 0;
        pkt->data[0] = '\0';
        pkt->stop = 0;
        // Send packets to server in a while loop using send_full
        while (true) {
            printf("1\n");
            memset(pkt, 0, sizeof(PACKET)); // Clear the packet memory

            int bytes_to_send = 0;
            if (total_bytes_sent < data_len) {
                // Calculate the bytes to send in the current packet
                bytes_to_send = data_len - total_bytes_sent;
                bytes_to_send = bytes_to_send < BUFFER_SIZE ? bytes_to_send : BUFFER_SIZE;

                pkt->length = bytes_to_send; // Update the packet length
                strncpy(pkt->data, data_ptr + total_bytes_sent, bytes_to_send); // Copy the data chunk
                total_bytes_sent += bytes_to_send;
            } else {
                // If all data has been sent, set the stop flag
                pkt->stop = 1;
                printf("Stop\n");
                printf("total_bytes_sent: %d\n", total_bytes_sent);
                printf("data_len: %d\n", data_len);
                printf("bytes_to_send: %d\n", bytes_to_send);
            }

            // Send the packet using send_full
            if (send_full(socket_fd, pkt, sizeof(PACKET)) == -1) {
                perror("Error sending data");
                close(socket_fd);
                failureCount++;
                break;
            }

            // If the stop flag is set, break the loop
            if (pkt->stop == 1) {
                break;
            }
        }
        printf(GREEN "[+] Sent %d bytes of data to server %d\n" RESET, total_bytes_sent, i + 1);
        // Receive response from the server
        int res;
        if (recv(socket_fd, &res, sizeof(res), 0) <= 0) {
            perror("Error receiving response from storage server");
            close(socket_fd);
            failureCount++;
            continue;
        }

        printf("errcode %d\n",res);

        send(nmSocket, &res, sizeof(res), 0); // Notify naming server about the result

        if (res != SUCCESS) {
            printf(RED "[-] Server %d reported failure for WRITE operation.\n" RESET, i + 1);
            failureCount++;
        } else {
            printf(GREEN "[+] File successfully written to server %d.\n" RESET, i + 1);
            successCount++;
        }

        close(socket_fd); // Close the connection to the current server
    }

    // Final report
    close(nmSocket); // Close naming server socket
    if (successCount > 0) {
        printf(GREEN "[+] Successfully written to %d server(s).\n" RESET, successCount);
        return SUCCESS;
    } else {
        printf(RED "[-] Failed to write to all storage servers.\n" RESET);
        return FAILURE;
    }
}


// // //////////////////////////////////////////////////////////////////////////////////////////////
// // CREATE operation
int create_file_or_folder(const char *path, bool isDirectory)
{
    int result;
    FILE_REQUEST_STRUCT request;

    // Prepare the request for creating a file or folder
    memset(&request, 0, sizeof(request));
    strcpy(request.opType, "CREATE");
    strncpy(request.path, path, sizeof(request.path) - 1);
    request.isDirectory = isDirectory; // Set the directory flag

    // Connect to the naming server to process the CREATE operation
    result = connect_to_naming_server(NAMING_SERVER_IP, NAMING_SERVER_PORT, NULL, path, "CREATE", isDirectory, true,NULL);
    if (result == FAILURE)
    {
        handle_error(NAMING_SERVER_ERR); // Naming server validation or connection failed
        return FAILURE;
    }

    // If the request was successfully processed
    printf(GREEN "[+] CREATE request processed by naming server for path: %s\n" RESET, path);
    return SUCCESS;
}

// DELETE operation
int delete_file_or_folder(const char *path, bool isDirectory)
{
    int result;
    FILE_REQUEST_STRUCT request;

    // Prepare the request for deleting a file or folder
    memset(&request, 0, sizeof(request));
    strcpy(request.opType, "DELETE");
    strncpy(request.path, path, sizeof(request.path) - 1);
    request.isDirectory = isDirectory; // Set the directory flag

    // Connect to the naming server to process the DELETE operation
    result = connect_to_naming_server(NAMING_SERVER_IP, NAMING_SERVER_PORT, NULL, path, "DELETE", isDirectory, true,NULL);
    if (result == FAILURE)
    {
        handle_error(NAMING_SERVER_ERR); // Naming server validation or connection failed
        return FAILURE;
    }

    // If the request was successfully processed
    printf(GREEN "[+] DELETE request processed by naming server for path: %s\n" RESET, path);
    return SUCCESS;
}

int list(char *responseBuffer, size_t bufferSize)
{
    int socket_fd;
    // bufferSize = 4096;
    FILE_REQUEST_STRUCT request;

    // Connect to the naming server
    socket_fd = connect_to_server(NAMING_SERVER_IP, NAMING_SERVER_PORT);
    if (socket_fd == -1)
    {
        handle_error(NAMING_SERVER_ERR); // Naming server connection failed
        return FAILURE;
    }

    // Prepare the request
    memset(&request, 0, sizeof(request)); // Initialize all fields to 0
    strcpy(request.opType, "LIST");       // Set the operation type to "LIST"

    // Send the LIST request to the naming server
    if (send(socket_fd, &request, sizeof(request), 0) == -1)
    {
        handle_error(SEND_ERR); // Failed to send the LIST request
        close(socket_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Sent LIST request to naming server.\n" RESET);

    // Receive the response from the naming server
    int bytesReceived = recv(socket_fd, responseBuffer, bufferSize, 0);
    if (bytesReceived == -1)
    {
        handle_error(RECV_ERR); // Failed to receive response
        close(socket_fd);
        return FAILURE;
    }
    printf("Bytes received: %d\n", bytesReceived);

    printf(GREEN "[+] Received response from naming server:\n%s\n" RESET, responseBuffer);

    close(socket_fd);
    return SUCCESS;
}

int copy(const char *sourcePath, const char *destinationPath)
{
    int socket_fd;
    FILE_REQUEST_STRUCT request;

    // Connect to the naming server
    socket_fd = connect_to_server(NAMING_SERVER_IP, NAMING_SERVER_PORT);
    if (socket_fd == -1)
    {
        handle_error(NAMING_SERVER_ERR); // Naming server connection failed
        return FAILURE;
    }

    // Prepare the request
    memset(&request, 0, sizeof(request));                                                     // Initialize all fields to 0
    strcpy(request.opType, "COPY");                                                           // Set the operation type to "COPY"
    strncpy(request.path, sourcePath, sizeof(request.path) - 1);                              // Set the source path
    strncpy(request.destination_path, destinationPath, sizeof(request.destination_path) - 1); // Set the destination path

    // Send the COPY request to the naming server
    if (send(socket_fd, &request, sizeof(request), 0) == -1)
    {
        handle_error(SEND_ERR); // Failed to send COPY request
        close(socket_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Sent COPY request to naming server.\n" RESET);

    // Receive the status code from the naming server
    int response;
    if (recv(socket_fd, &response, sizeof(response), 0) == -1)
    {
        handle_error(RECV_ERR); // Failed to receive response
        close(socket_fd);
        return FAILURE;
    }

    // Interpret the response
    if (response == SUCCESS)
    {
        printf(GREEN "[+] File/Directory successfully copied from '%s' to '%s'.\n" RESET, sourcePath, destinationPath);
    }
    else if (response == PATH_NOT_FOUND_ERR)
    {
        handle_error(PATH_NOT_FOUND_ERR); // Source path not found
    }
    else if (response == PERMISSION_DENIED_ERR)
    {
        handle_error(PERMISSION_DENIED_ERR); // Permission denied
    }
    else
    {
        handle_error(UNKNOWN_ERR); // Unhandled error
    }

    // Close the socket
    close(socket_fd);
    return response == SUCCESS ? SUCCESS : FAILURE;
}

int get(const char *fileName, FILE_METADATA *metadata)
{
    int naming_server_fd, storage_server_fd;
    SERVER_CONFIG storage_server_info;
    FILE_REQUEST_STRUCT request;

    // Step 1: Connect to the naming server
    naming_server_fd = connect_to_server(NAMING_SERVER_IP, NAMING_SERVER_PORT);
    if (naming_server_fd == -1)
    {
        handle_error(NAMING_SERVER_ERR); // Naming server connection failed
        return FAILURE;
    }

    // Step 2: Prepare the request for the naming server
    memset(&request, 0, sizeof(request));
    strcpy(request.opType, "GET");                             // Operation type: "GET"
    strncpy(request.path, fileName, sizeof(request.path) - 1); // Set the file name/path

    // Send the GET request to the naming server
    if (send(naming_server_fd, &request, sizeof(request), 0) == -1)
    {
        handle_error(SEND_ERR); // Failed to send GET request to naming server
        close(naming_server_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Sent GET request to naming server for file: %s\n" RESET, fileName);

    // Step 3: Receive the storage server information from the naming server
    if (recv(naming_server_fd, &storage_server_info, sizeof(SERVER_CONFIG), 0) == -1)
    {
        handle_error(RECV_ERR); // Failed to receive storage server info
        close(naming_server_fd);
        return FAILURE;
    }

    // Check if the received server information is valid
    if (strlen(storage_server_info.networkIP) == 0 || storage_server_info.portNumber == 0)
    {
        handle_error(INVALID_SERVER_INFO_ERR); // Invalid storage server information
        close(naming_server_fd);
        return FAILURE;
    }

    // Close the naming server connection
    // close(naming_server_fd);

    // Step 4: Connect to the storage server
    storage_server_fd = connect_to_server(storage_server_info.networkIP, storage_server_info.portNumber);
    if (storage_server_fd == -1)
    {
        handle_error(STORAGE_SERVER_ERR); // Storage server connection failed
        return FAILURE;
    }

    // Step 5: Send the GET request to the storage server
    if (send(storage_server_fd, &request, sizeof(request), 0) == -1)
    {
        handle_error(SEND_ERR); // Failed to send GET request to storage server
        close(storage_server_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Sent GET request to storage server for file: %s\n" RESET, fileName);

    // Step 6: Receive the file metadata from the storage server
    if (recv(storage_server_fd, metadata, sizeof(FILE_METADATA), 0) == -1)
    {
        handle_error(RECV_ERR); // Failed to receive file metadata from storage server
        close(storage_server_fd);
        return FAILURE;
    }

    // Step 7: Validate the received metadata
    if (strlen(metadata->filename) == 0 || metadata->fileSize < 0)
    {
        handle_error(INVALID_METADATA_ERR); // Invalid file metadata received
        close(storage_server_fd);
        return FAILURE;
    }

    printf(GREEN "[+] Received file metadata:\n" RESET);
    printf("    Filename: %s\n" RESET, metadata->filename);
    printf("    File Size: %lld bytes\n" RESET, metadata->fileSize);
    printf("    Permissions: %s\n" RESET, metadata->filePermissions);

    int res;
    recv(storage_server_fd,&res,sizeof(res),0);

    handle_error(res);

    send(naming_server_fd,&res,sizeof(int),0);

    // Step 8: Close the storage server connection
    close(naming_server_fd);
    close(storage_server_fd);

    return SUCCESS;
}



int stream_audio_file(const char *nm_ip, int nm_port, const char *fileName)
{
    int storage_socket_fd;
    SERVER_CONFIG storage_server_info;
    char buffer[BUFFER_SIZE];
    int bytesReceived;
    int nmSocket;
    // Step 1: Connect to naming server and get storage server info
    if (connect_to_naming_server(nm_ip, nm_port, &storage_server_info, fileName, "STREAM", 0, true,&nmSocket) == FAILURE)
    {
        handle_error(NAMING_SERVER_ERR);
        return FAILURE;
    }

    // Step 2: Connect to the storage server
    storage_socket_fd = connect_to_server(storage_server_info.networkIP, storage_server_info.portNumber);
    if (storage_socket_fd == -1)
    {
        handle_error(STORAGE_SERVER_ERR);
        return FAILURE;
    }

    // Step 3: Check if FIFO exists and remove it
    if (access(FIFO_PATH, F_OK) == 0)
    {
        if (unlink(FIFO_PATH) == -1)
        {
            handle_error(UNLINK_FIFO_ERR);
            close(storage_socket_fd);
            return FAILURE;
        }
    }

    // Step 4: Create a new FIFO
    if (mkfifo(FIFO_PATH, 0666) == -1)
    {
        handle_error(MKFIFO_ERR);
        close(storage_socket_fd);
        return FAILURE;
    }

    // Step 5: Initialize MPV
    mpv_handle *mpv = mpv_create();
    if (!mpv)
    {
        handle_error(MPV_CREATE_ERR);
        close(storage_socket_fd);
        unlink(FIFO_PATH);
        return FAILURE;
    }

    if (mpv_initialize(mpv) < 0)
    {
        handle_error(MPV_INIT_ERR);
        mpv_destroy(mpv);
        close(storage_socket_fd);
        unlink(FIFO_PATH);
        return FAILURE;
    }

    // Configure MPV to load the FIFO
    const char *load_fifo_command[] = {"loadfile", FIFO_PATH, "replace", NULL};
    if (mpv_command(mpv, load_fifo_command) < 0)
    {
        handle_error(MPV_FIFO_ERR);
        mpv_destroy(mpv);
        close(storage_socket_fd);
        unlink(FIFO_PATH);
        return FAILURE;
    }

    // Step 6: Send the STREAM request to the storage server
    FILE_REQUEST_STRUCT request = {0};
    strcpy(request.opType, "STREAM");
    strncpy(request.path, fileName, sizeof(request.path) - 1);

    if (send(storage_socket_fd, &request, sizeof(request), 0) == -1)
    {
        handle_error(SEND_ERR);
        mpv_destroy(mpv);
        close(storage_socket_fd);
        unlink(FIFO_PATH);
        return FAILURE;
    }

    printf("[+] Sent STREAM request for file: %s\n", fileName);

    // Step 7: Open the FIFO and write data to it
    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1)
    {
        handle_error(OPEN_FIFO_ERR);
        mpv_destroy(mpv);
        close(storage_socket_fd);
        unlink(FIFO_PATH);
        return FAILURE;
    }

    // Step 8: Stream data from the server to MPV via FIFO
    while ((bytesReceived = recv(storage_socket_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        ssize_t bytesWritten = 0;
        while (bytesWritten < bytesReceived)
        {
            ssize_t written = write(fifo_fd, buffer + bytesWritten, bytesReceived - bytesWritten);
            if (written == -1)
            {
                handle_error(FIFO_WRITE_ERR);
                mpv_destroy(mpv);
                close(storage_socket_fd);
                close(fifo_fd);
                unlink(FIFO_PATH);
                return FAILURE;
            }
            bytesWritten += written;
        }
    }

    if (bytesReceived == -1)
    {
        handle_error(RECV_ERR);
    }

    int res;
    recv(storage_socket_fd,&res,sizeof(res),0);
    handle_error(res);
    send(nmSocket,&res,sizeof(res),0);

    // Step 9: Cleanup
    mpv_destroy(mpv);
    close(storage_socket_fd);
    close(nmSocket);
    close(fifo_fd);
    unlink(FIFO_PATH);

    return bytesReceived == -1 ? FAILURE : SUCCESS;
}

int main(int argc, char *argv[])
{

    if (argc > 2)
    {
        printf("USAGE <ip>");
        return 0;
    }

    if (argc == 2)
    {
        NAMING_SERVER_IP = argv[1];
    }  


    printf("NAMING SERVER IP : %s\n", NAMING_SERVER_IP);
    char path[256];
    bool isDirectory; // Flag to determine if it's a file or directory
    char operation[20];
    char isDirectoryInput[4]; // To hold the user input for YES/NO
    char buffer[BUFFER_SIZE];
    char fileData[BUFFER_SIZE];
    char fileData2[BUFFER_SIZE] = {0};
    char *current = fileData2;

    while (true)
    {
        char fileName[256];
        printf(YELLOW "\n=== Client Menu ===\n" RESET);
        // printf("\n=== Client Menu ===\n");
        printf(YELLOW);
        printf("Enter one of the following operations:\n");
        printf(RESET);
        printf(MAGENTA);
        printf("LIST - to list all files\n");
        printf("READ - to read a file\n");
        printf("WRITE - to write a file\n");
        printf("DELETE - to delete a file\n");
        printf("CREATE - to create a file\n");
        printf("EXIT - to exit the program\n");
        printf("COPY - to copy a file\n");
        printf("GET - to get file metadata\n");
        printf("AUDIO - to get the audio file\n");
        printf(RESET);
        printf(YELLOW);
        printf("===================\n");
        printf("Enter your operation: ");
        printf(RESET);
        // fgets(operation, sizeof(operation), stdin);
        int x = scanf("%s",operation);
        // printf("x:  : %d\n",x);
        // operation[strcspn(operation, "\n")] = '\0'; // Remove newline character

        if (strcmp(operation, "READ") == 0)
        { // READ operation
            printf(YELLOW "Enter the file name to READ: " RESET);
            // fgets(fileName, sizeof(fileName), stdin);
            scanf("%s",fileName);
            // fileName[strcspn(fileName, "\n")] = '\0'; // Remove newline character

            format_file_name(fileName);

            if (read_from_file(fileName, buffer) == SUCCESS)
            {
                // printf("\n[+] File content:\n%s\n", buffer);
            }
            else
            {
                printf(RED "\n[-] Failed to READ file.\n" RESET);
            }
        }
        else if (strcmp(operation, "WRITE") == 0)
        { // WRITE operation
            printf(YELLOW "Enter the file name to WRITE: " RESET);
            // fgets(fileName, sizeof(fileName), stdin);
            scanf("%s",fileName);
            
            

            // format_file_name(fileName);

            printf(YELLOW "Enter the data to write to the file: " RESET);
            // if(fgets(fileData, sizeof(fileData), stdin) == NULL){
            //     perror("fgets");
            //     break;
            // }
            // scanf("%s",fileData);
            scanf(" %[^\n]", fileData);
            // while (fgets(fileData, sizeof(fileData), stdin) != NULL)
            // {
            //     strcpy(current, fileData);
            //     current += strlen(fileData);
            // }

            // fileData[strcspn(fileData, "\n")] = '\0'; // Remove newline character
            // fflush(stdin);
            // rewind(stdin);


            if (write_to_file(fileName, fileData) == SUCCESS)
            {
                printf(GREEN "[+] File written successfully.\n" RESET);
            }
            else
            {
                printf(RED "[+] Failed to write to file.\n" RESET);
            }

            // memset(fileData, 0, sizeof(fileData));
            // current = fileData2;
            // *fileData = '\0';
            
        }
        else if (strncmp(operation, "LARGE", 5) == 0)
        {
            bool sync = false;
            char syncArr[10];
            printf(YELLOW "Do you want sync(YES,NO): " RESET);
            scanf("%s",syncArr);
            
            if(strcmp(syncArr,"YES")==0){
                sync = true;
            }
            printf(YELLOW "Enter the file name to WRITE: " RESET);
            // fgets(fileName, sizeof(fileName), stdin);
            // fileName[strcspn(fileName, "\n")] = '\0'; // Remove newline character
            scanf("%s",fileName);

            format_file_name(fileName);

            printf("main: %d\n", sync);
            large_file(fileName, sync);
        }
        else if (strcmp(operation, "CREATE") == 0)
        { // CREATE operation
            printf(YELLOW "Enter the path to CREATE: " RESET);
            // fgets(path, sizeof(path), stdin);
            scanf("%s",path);
            // path[strcspn(path, "\n")] = '\0'; // Remove newline character

            format_file_name(fileName);

            printf(YELLOW "Is this a directory? (YES/NO): " RESET);
            // fgets(isDirectoryInput, sizeof(isDirectoryInput), stdin);
            scanf("%s",isDirectoryInput);
            // isDirectoryInput[strcspn(isDirectoryInput, "\n")] = '\0'; // Remove newline character

            // Set isDirectory based on user input
            if (strcmp(isDirectoryInput, "YES") == 0)
            {
                isDirectory = true;
            }
            else if (strcmp(isDirectoryInput, "NO") == 0)
            {
                isDirectory = false;
            }
            else
            {
                printf(RED "Invalid input. Please enter 'YES' or 'NO'.\n" RESET);
                continue; // Restart the loop
            }

            if (create_file_or_folder(path, isDirectory) == SUCCESS)
            {
                printf(GREEN "[+] File/Folder created successfully.\n" RESET);
            }
            else
            {
                printf(RED "[+] Failed to create file/folder.\n" RESET);
            }
        }
        else if (strcmp(operation, "DELETE") == 0)
        { // DELETE operation
            printf(YELLOW "Enter the path to DELETE: " RESET);
            // fgets(path, sizeof(path), stdin);
            scanf("%s",path);
            // path[strcspn(path, "\n")] = '\0'; // Remove newline character

            format_file_name(fileName);

            printf(YELLOW "Is this a directory? (YES/NO): " RESET);
            // fgets(isDirectoryInput, sizeof(isDirectoryInput), stdin);
            scanf("%s",isDirectoryInput);
            // isDirectoryInput[strcspn(isDirectoryInput, "\n")] = '\0'; // Remove newline character

            // Set isDirectory based on user input
            if (strcmp(isDirectoryInput, "YES") == 0)
            {
                isDirectory = true;
            }
            else if (strcmp(isDirectoryInput, "NO") == 0)
            {
                isDirectory = false;
            }
            else
            {
                printf(RED "Invalid input. Please enter 'YES' or 'NO'.\n" RESET);
                continue; // Restart the loop
            }

            if (delete_file_or_folder(path, isDirectory) == SUCCESS)
            {
                printf(YELLOW "[+] File/Folder deleted successfully.\n" RESET);
            }
            else
            {
                printf(RED "[+] Failed to delete file/folder.\n" RESET);
            }
        }
        else if (strcmp(operation, "LIST") == 0)
        {                              // LIST operation
            char responseBuffer[8192]; // Buffer to hold the response from the server

            // Call the list function directly without needing an additional fgets
            list(responseBuffer, sizeof(responseBuffer));
        }
        else if (strcmp(operation, "COPY") == 0)
        { // COPY operation
            char sourcePath[256], destinationPath[256];

            printf(YELLOW "Enter source path: " RESET);
            // fgets(sourcePath, sizeof(sourcePath), stdin);
            scanf("%s",sourcePath);
            // sourcePath[strcspn(sourcePath, "\n")] = '\0'; // Remove newline character

            format_file_name(sourcePath);

            printf(YELLOW "Enter destination path: " RESET);
            // fgets(destinationPath, sizeof(destinationPath), stdin);
            // destinationPath[strcspn(destinationPath, "\n")] = '\0'; // Remove newline character
            scanf("%s",destinationPath);
            format_file_name(destinationPath);

            if (copy(sourcePath, destinationPath) == SUCCESS)
            {
                printf(GREEN "[+] Copy operation completed successfully.\n" RESET);
            }
            else
            {
                printf(RED "[-] Copy operation failed.\n" RESET);
            }
        }
        else if (strcmp(operation, "GET") == 0)
        { // GET operation
            char fileName[256];
            FILE_METADATA metadata;

            printf(YELLOW "Enter file name to retrieve: " RESET);
            // fgets(fileName, sizeof(fileName), stdin);
            // fileName[strcspn(fileName, "\n")] = '\0'; // Remove newline character
            scanf("%s",fileName);
            format_file_name(fileName);

            if (get(fileName, &metadata) == SUCCESS)
            {
                printf(GREEN "[+] File metadata successfully retrieved.\n" RESET);
            }
            else
            {
                printf(RED "[-] Failed to retrieve file metadata.\n" RESET);
            }
        }
        else if (strcmp(operation, "AUDIO") == 0)
        {
            char fileName[256];

            printf(YELLOW "Enter the audio file name to stream: " RESET);
            // fgets(fileName, sizeof(fileName), stdin);
            // fileName[strcspn(fileName, "\n")] = '\0'; // Remove newline character
            scanf("%s",fileName);
            format_file_name(fileName);

            // Attempt to stream the audio file
            if (stream_audio_file(NAMING_SERVER_IP, NAMING_SERVER_PORT, fileName) == SUCCESS)
            {
                printf(GREEN "[+] Successfully streamed audio file: %s\n" RESET, fileName);
            }
            else
            {
                printf(RED "[-] Failed to stream audio file: %s\n" RESET, fileName);
            }
        }
        else if (strcmp(operation, "EXIT") == 0)
        { // Exit
            printf(YELLOW "Exiting the client application. Goodbye!\n" RESET);
            return 0;
        }
        else
        {
            printf(RED "Invalid operation. Please try again.\n" RESET);
        }
    }

    return 0;
}