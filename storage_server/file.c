#define BUFFER_SIZE 8192
#define FILE_SIZE_THRESHOLD 30000
// #include"storage_server.h"
#include "file.h"
#include "lib.h"

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

int read_file(int clientSocket, const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf(RED "%s not found or unable to open\n" RESET, path);
        return INVALID_PATH;
    }

   PACKET packet;   // Packet structure to send
    size_t bytes_read;
    int totalBytes = 0;

    // Read the file in chunks and send PACKETs
    while ((bytes_read = fread(packet.data, 1, sizeof(packet.data), file)) > 0) {
        packet.length = bytes_read;
        totalBytes += bytes_read;
        packet.stop = 0; // More data to come

        // Use send_full to send the PACKET structure
        if (send_full(clientSocket, &packet, sizeof(packet)) < 0) {
            perror("Failed to send packet");
            fclose(file);
            return READ_ERR;
        }

        printf(GREEN "[+] Sent %zu bytes of data.\n" RESET, bytes_read);
    }

    printf("bytes send %d\n",totalBytes);

    // Send the final packet to indicate the end of the transfer
    packet.length = 0;    // No data in this packet
    packet.stop = 1;      // Indicates the end of the transfer
    if (send_full(clientSocket, &packet, sizeof(packet)) < 0) {
        perror("Failed to send end-of-transfer packet\n");
        fclose(file);
        return READ_ERR;
    }

    fclose(file);
    printf(GREEN "File '%s' sent successfully.\n" RESET, path);
    return SUCCESS;
}


// Function to create a new WriteBuffer node
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

void *async_write(void *args)
{
    printf(CYAN"Writing file asynchronously\n"RESET);
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    const char *filePath = threadArgs->filePath;
    WriteBuffer *head = threadArgs->head;

    // Open the file for writing
    int file_fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1)
    {
        perror("Error opening file for writing");
        pthread_exit((void *)WRITE_ERR);
    }

    // Traverse the linked list and write the data to the file
    WriteBuffer *currentNode = head;
    size_t totalBytesWritten = 0;
    while (currentNode)
    {
        ssize_t bytesWritten = write(file_fd, currentNode->data, currentNode->dataSize);
        if (bytesWritten == -1)
        {
            perror("Error writing data to file");
            close(file_fd);
            pthread_exit((void *)WRITE_ERR);
        }
        totalBytesWritten += bytesWritten;

        // Print status of bytes written
        printf("[+] Wrote %ld bytes to file\n", bytesWritten);

        // Free the current node
        WriteBuffer *temp = currentNode;
        currentNode = currentNode->next;
        free(temp->data);
        free(temp);
    }

    printf("[+] Total %zu bytes written to file %s\n", totalBytesWritten, filePath);

    // Close the file
    close(file_fd);

    // Exit thread
    pthread_exit((void *)SUCCESS);
}

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

int write_file(int clientSocket, const char *filePath, bool sync) {
    // printf("bool %d\n", sync);

    PACKET packet;
    WriteBuffer *head = NULL;
    WriteBuffer *tail = NULL;
    bool isLarge = false;
    int totalByteRecv = 0;

    // Receive data from client in PACKETs and store it in linked list
    while (1) {
        // Receive the PACKET structure fully
        ssize_t bytesReceived = recv_full(clientSocket, &packet, sizeof(PACKET));
        if (bytesReceived <= 0) {
            perror("Error receiving data from client");
            return FAILURE;
        }

        totalByteRecv += packet.length;

        // Create a new WriteBuffer node for each PACKET received
        WriteBuffer *newNode = create_write_buffer_node(packet.data, packet.length,filePath,clientSocket);
        if (!newNode) {
            return WRITE_ERR;
        }

        // Append new node to the linked list
        if (tail) {
            tail->next = newNode;
        } else {
            head = newNode; // First node
        }
        tail = newNode; // Move the tail to the new node

        printf("[+] Received %d bytes\n", packet.length);

        // Check if file size exceeds threshold
        if (totalByteRecv > FILE_SIZE_THRESHOLD) {
            isLarge = true;
        }

        // Stop receiving if the stop flag is set
        // printf("STOPPED : %d",packet.stop);
        if (packet.stop == 1) {
            printf("STOPPED\n");
            break;
        }
    }

     if (isLarge && sync == false)
    {
        printf(CYAN"File is large\n"RESET);
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        strcpy(args->filePath, filePath);
        args->head = head;
        pthread_t async_write_thread;
        pthread_create(&async_write_thread, NULL, async_write, (void*)args);
        return SUCCESS;
    }

    // Open the file for writing
    int file_fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("Error opening file for writing");
        return WRITE_ERR;
    }

    // Traverse the linked list and write the data to the file
    WriteBuffer *currentNode = head;
    int totalBytesWritten = 0;
    while (currentNode) {
        // printf("WRITING  FILE\n");
        int bytesWritten = write(file_fd, currentNode->data, currentNode->dataSize);
        if (bytesWritten == -1) {
            perror("Error writing data to file");
            close(file_fd);
            return WRITE_ERR;
        }
        totalBytesWritten += bytesWritten;

        // Print status of bytes written
        printf("[+] Wrote %d bytes to file\n", bytesWritten);

        // Move to the next node in the list
        WriteBuffer *temp = currentNode;
        currentNode = currentNode->next;
        free(temp->data);
        free(temp);
    }

    printf("[+] Total %d bytes written to file %s\n", totalBytesWritten, filePath);

    // Close the file
    close(file_fd);

    return SUCCESS;
}



int create_file_dir(const char *path, bool isDir)
{
    // char filepath[256];
    // snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, path);
    // printf("bool %d",isDir);
    if (!isDir)
    {
        FILE *file = fopen(path, "w");
        if (file == NULL)
        {
            printf(RED "%s Unable to create file\n" RESET, path);
            return INVALID_PATH;
        }

        printf(GREEN "File '%s' created successfully.\n" RESET, path);
        fclose(file);
        return SUCCESS;
    }
    else
    {
        if (mkdir(path, 0755) == -1)
        {
            if (errno == EEXIST)
            {
                printf(RED "Directory '%s' already exists.\n" RESET, path);
                return ALREADY_EXIST;
            }
            else
            {
                printf(RED "%s Unable to create directory\n" RESET, path);
                return CREATE_ERR;
            }
        }
        else
        {
            printf(GREEN "Directory '%s' created successfully.\n" RESET, path);
            return SUCCESS;
        }
    }
}

int delete_directory(const char *dirpath)
{

    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        printf(RED " %s Failed to open directory\n" RESET, dirpath);
        return INVALID_PATH;
    }

    struct dirent *entry;
    char filepath[256];

    // Loop through the directory contents
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path for each entry
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);

        // Check if it's a file or directory
        struct stat path_stat;
        stat(filepath, &path_stat);

        if (S_ISDIR(path_stat.st_mode))
        {
            // Recursive call to delete sub-directory
            int code = delete_directory(filepath);
            if (code == DELETE_ERR)
            {
                return DELETE_ERR;
            }
        }
        else
        {
            // Delete the file
            if (remove(filepath) != 0)
            {
                printf(RED "Failed to delete file" RESET);
                return DELETE_ERR;
            }
        }
    }
    closedir(dir);

    // Delete the empty directory itself
    if (rmdir(dirpath) == 0)
    {
        printf(GREEN "Directory '%s' deleted successfully.\n" RESET, dirpath);
        return SUCCESS;
    }
    else
    {
        perror("Failed to delete directory");
        return DELETE_ERR;
    }
}

int delete_file_dir(const char *path, bool isDir)
{

    if (!isDir)
    {

        if (remove(path) == 0)
        {
            printf(GREEN "File '%s' deleted successfully.\n" RESET, path);
            return SUCCESS;
        }
        else
        {
            printf("Failed to delete file\n");
            return DELETE_ERR;
        }
    }
    else
    {
        return delete_directory(path);
    }
}

void get_file_permissions(mode_t mode, char *permissions)
{
    permissions[0] = (mode & S_IRUSR) ? 'r' : '-';
    permissions[1] = (mode & S_IWUSR) ? 'w' : '-';
    permissions[2] = (mode & S_IXUSR) ? 'x' : '-';
    permissions[3] = (mode & S_IRGRP) ? 'r' : '-';
    permissions[4] = (mode & S_IWGRP) ? 'w' : '-';
    permissions[5] = (mode & S_IXGRP) ? 'x' : '-';
    permissions[6] = (mode & S_IROTH) ? 'r' : '-';
    permissions[7] = (mode & S_IWOTH) ? 'w' : '-';
    permissions[8] = (mode & S_IXOTH) ? 'x' : '-';
    permissions[9] = '\0'; // Null-terminate the string
}

int get_file(const char *path, FILE_METADATA *fileMetadata)
{
    struct stat fileStat;

    // Get file information using stat()
    if (stat(path, &fileStat) == -1)
    {
        perror("stat");
        return FAILURE; // Return -1 on error
    }
    // printf("hi\n");
    // Fill the FILE_METADATA structure
    strncpy(fileMetadata->filename, path, sizeof(fileMetadata->filename) - 1);
    fileMetadata->filename[sizeof(fileMetadata->filename) - 1] = '\0'; // Ensure null-termination
    fileMetadata->fileSize = fileStat.st_size;
    get_file_permissions(fileStat.st_mode, fileMetadata->filePermissions);

    return SUCCESS; // Success
}

// Recursive function to concatenate files and directories
void seek_recursive_concatenate(const char *path, const char *current_path, char *result)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        perror(RED "opendir" RESET);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
        {
            continue; // Skip hidden files/directories
        }

        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat file_stat;
        if (stat(full_path, &file_stat) == -1)
        {
            perror("stat");
            continue;
        }

        // Prepare current full path
        char new_current_path[BUFFER_SIZE];
        snprintf(new_current_path, sizeof(new_current_path), "%s/%s", current_path, entry->d_name);

        // Temporary buffer to hold the result for each entry
        char temp[BUFFER_SIZE];

        // Check if it is a directory
        if (S_ISDIR(file_stat.st_mode))
        {
            // Append '1' at the start of directory name
            snprintf(temp, sizeof(temp), "1%s;", new_current_path);
        }
        else
        {
            // Append '0' at the start of file name
            snprintf(temp, sizeof(temp), "0%s;", new_current_path);
        }

        // Concatenate to the final result
        strcat(result, temp);

        // If it's a directory, recursively call for subdirectory
        if (S_ISDIR(file_stat.st_mode))
        {
            seek_recursive_concatenate(full_path, new_current_path, result);
        }
    }

    closedir(dir);
}

int stream_audio(int clientSocket, const char *path)
{
    FILE *audioFile = fopen(path, "rb");
    if (!audioFile)
    {
        perror("Error opening audio file");
        return INVALID_PATH;
    }

    char buffer[4096]; // 4KB buffer size
    size_t bytesRead;

    // Read the file in chunks and send to the client

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), audioFile)) > 0)
    {
        // Send the chunk to the client
        if (send(clientSocket, buffer, bytesRead, 0) == -1)
        {
            perror("Error sending audio data");
            fclose(audioFile);
            return STREAM_ERR;
        }
    }

    fclose(audioFile);
    printf(GREEN "Audio streaming complete.\n" RESET);
    return SUCCESS;
}
