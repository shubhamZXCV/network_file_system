#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tries.h"


void initTrieNode(TrieNode *node){
    node->endOfWordCount = 0;
    node->data = NULL;
    for(int i = 0; i < ALPHABET_SIZE; i++){
        node->trieChildren[i] = NULL;
    }
    if (pthread_mutex_init(&(node->lock), NULL) != 0){
        perror("pthread_mutex_init() failed");
        node=NULL;
        return;
    }

    if (pthread_mutex_init(&(node->writeMutex), NULL) != 0){
        perror("pthread_mutex_init() failed");
        pthread_mutex_destroy(&(node->lock));
        node=NULL;
        return;
    }
    node->isDirectory = true;
    node->latestVersion = -1;
    node->readerCount = 0;
}

TrieNode* wrapperForTrie(){
    TrieNode* root = (TrieNode*)malloc(sizeof(TrieNode));
    if(root == NULL){
        perror("malloc() failed");
        return NULL;
    }
    initTrieNode(root);
    return root;
}


TrieNode *createTrieNode(){
    TrieNode* newNode = wrapperForTrie();
    // newNode->lock = PTHREAD_MUTEX_INITIALIZER;
    // newNode->writeMutex = PTHREAD_MUTEX_INITIALIZER;
    return newNode;
}

TrieNode *trieInsert(TrieNode *root, char *key, char *data,bool isDirectory){
    TrieNode *currentNode = root;
    int i = 0;

    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            currentNode->trieChildren[key[i]] = createTrieNode();
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }
    // Increment wordEndCnt for the last currentNode pointer as it points to the end of the key.
    currentNode->endOfWordCount++;
    currentNode->isDirectory = isDirectory;
    // Set the data associated with the key
    if (currentNode->data != NULL){
        free(currentNode->data); // Free previously allocated data if present
    }
    currentNode->data = strdup(data); // Copy the value into the node

    return root;
}

char *getValueFromTrie(TrieNode *root, char *key){
    TrieNode *currentNode = root;
    int i = 0;

    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            return NULL; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }

    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        return currentNode->data; // Return the value associated with the key
    }

    return NULL; // Key not found
}

RETURN_DATA* getReturnDataFromTrie(TrieNode *root, char *key){
    TrieNode *currentNode = root;
    int i = 0;
    RETURN_DATA* returnData = (RETURN_DATA*)malloc(sizeof(RETURN_DATA));
    if(returnData == NULL){
        perror("malloc() failed");
        return NULL;
    }
    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            returnData->value = NULL;
            returnData->isDirectory = false;
            return returnData; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }

    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        returnData->value = currentNode->data; // Return the value associated with the key
        returnData->isDirectory = currentNode->isDirectory;
        return returnData;
    }

    returnData->value = NULL;
    returnData->isDirectory = false;
    return returnData; // Key not found
}



TrieNode* getNodeFromTrie(TrieNode *root, char *key){
    TrieNode *currentNode = root;
    int i = 0;

    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            return NULL; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        // printf("%c\n",key[i]);
        i++;
    }

    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        return currentNode; // Return the value associated with the key
    }

    return NULL; // Key not found
}

void createStringinListAllPathsForValue(char* dest,TrieNode*root,char* path,char* value){
    if(root == NULL) return;
    if (root->endOfWordCount > 0 && root->data != NULL && strcmp(root->data, value) == 0) {
        if (root->isDirectory) {
            strcat(dest, "\033[0;36m");
            strcat(dest, path);
            strcat(dest, "\033[0m\n"); // CYAN color for directories
            // printf("HELLO\n");
        } 
        else {
            strcat(dest, path);
            strcat(dest, "\033[0m\n"); // WHITE color for files
        }
    }

    for(int i = 0; i < ALPHABET_SIZE; i++){
        if(root->trieChildren[i] != NULL){
            char *newPath = (char *)malloc(strlen(path) + 2);
            strcpy(newPath, path);
            newPath[strlen(path)] = i;
            newPath[strlen(path) + 1] = '\0';
            createStringinListAllPathsForValue(dest,root->trieChildren[i], newPath, value);
            free(newPath);
        }
    }
}

char* listAllDirectoriesForCopy(char* searchPath,TrieNode* root,char* path,int storageServer){
    char* dest = (char*)malloc(8192);
    TrieNode* node = getNodeFromTrie(root,searchPath);
    if(dest == NULL){
        perror("malloc() failed");
        return NULL;
    }
    if(node == NULL){
        printf("Given path not found in Trie\n");
        return NULL;
    }
    strcpy(dest,"");
    createStringinListAllPaths(dest,node,path);
    char str[10];
    sprintf(str, "%d", storageServer);
    // createStringinListAllPathsForValue(dest,node,path,str);
    return dest;
}

char* listAllDirectoriesForCopy1(char* searchPath,TrieNode* root,char* path,int storageServer){
    char* dest = (char*)malloc(8192);
    TrieNode* node = getNodeFromTrie(root,searchPath);
    if(dest == NULL){
        perror("malloc() failed");
        return NULL;
    }
    if(node == NULL){
        printf("Given path not found in Trie\n");
        return NULL;
    }
    strcpy(dest,"");
    // createStringinListAllPaths(dest,node,path);
    char str[10];
    sprintf(str, "%d", storageServer);
    createStringinListAllPathsForValue(dest,node,path,str);
    return dest;
}




bool removeKeyinTrie(TrieNode *root, char *key){
    TrieNode *currentNode = root;
    int i = 0;

    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            return false; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }

    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        currentNode->endOfWordCount--;

        // Clear the value associated with the key
        if (currentNode->data != NULL){
            free(currentNode->data);
            currentNode->data = NULL;
        }

        return true;
    }

    return false; // Key not found
}


// Helper function to recursively delete all nodes below the current node
void deleteSubtree(TrieNode *node) {
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->trieChildren[i]) {
            deleteSubtree(node->trieChildren[i]);
            free(node->trieChildren[i]);
            node->trieChildren[i] = NULL;
        }
    }

    // Free the associated data
    if (node->data != NULL) {
        free(node->data);
        node->data = NULL;
    }
}

// Function to delete a key and all extensions from the Trie
bool deleteKeyAndExtensions(TrieNode *root, char *key) {
    TrieNode *currentNode = root;
    int i = 0;

    // Traverse to the node representing the end of the key
    while (key[i] != '\0') {
        if (currentNode->trieChildren[key[i]] == NULL) {
            return false;  // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }

    // If the key exists, delete all extensions
    if (currentNode != NULL) {
        deleteSubtree(currentNode);  // Delete all nodes under the key
        free(currentNode->data);    // Free associated data
        currentNode->data = NULL;
        currentNode->endOfWordCount = 0;
    }

    return true;  // Key and its extensions were successfully removed
}

void cleanupTrie(TrieNode *root){
    if(root == NULL) return;
    
    for(int i = 0; i < ALPHABET_SIZE; i++){
        cleanupTrie(root->trieChildren[i]);
    }
    if(root->data != NULL){
        free(root->data);
    }
    free(root);
}

// LOCK FUNCTIONS
pthread_mutex_t *acquireLock(TrieNode *root, char *key){
    TrieNode *currentNode = root;
    int i = 0;

    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            return NULL; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }

    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        return &currentNode->lock; // Return the value associated with the key
    }

    return NULL; // Key not found
}

pthread_mutex_t *getNodeWritelock(TrieNode *root, char *key){
    TrieNode *currentNode = root;
    int i = 0;

    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            return NULL; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }
    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        return &currentNode->writeMutex; // Return the value associated with the key
    }
    return NULL; // Key not found
}

struct  TrieNode* findNode(TrieNode *root, char *key){

    TrieNode *currentNode = root;
    int i = 0;
    while (key[i] != '\0'){
        if (currentNode->trieChildren[key[i]] == NULL){
            return NULL; // Key not found
        }
        currentNode = currentNode->trieChildren[key[i]];
        i++;
    }

    if (currentNode != NULL && currentNode->endOfWordCount > 0){
        return currentNode; // Return the value associated with the key
    }
    return NULL; // Key not found
    /* data */
}


void requestReadAccess(TrieNode *node){
    pthread_mutex_lock(&node->lock);
    node->readerCount++;
    if (node->readerCount == 1){ // askShubham
        // DISABLE WRITERS TO ENTER
        pthread_mutex_lock(&node->writeMutex);
    }
    pthread_mutex_unlock(&node->lock);
}

void releaseReadAccess(TrieNode *node){
    pthread_mutex_lock(&node->lock);
    node->readerCount--;
    if (node->readerCount == 0){
        // ENABLE WRITERS TO ENTER
        pthread_mutex_unlock(&node->writeMutex);
    }
    pthread_mutex_unlock(&node->lock);
}

void requestWritePermission(TrieNode *node){
    pthread_mutex_lock(&node->writeMutex);
}
void releaseWriteAccess(TrieNode *node){
    pthread_mutex_unlock(&node->writeMutex);
}

// int main() {
//     TrieNode *root = createTrieNode();

//     // Inserting key-value pairs
//     root = trieInsert(root, "ap/.ple", "A fruit");
//     root = trieInsert(root, "banana", "Another fruit");
//     root = trieInsert(root, "app", "Application");

//     // Retrieving values
//     char *value1 = getValueFromTrie(root, "ap/.ple");
//     char *value2 = getValueFromTrie(root, "banana");
//     char *value3 = getValueFromTrie(root, "app");

//     printf("Value for 'apple': %s\n", value1); // Output: Value for 'apple': A fruit
//     printf("Value for 'banana': %s\n", value2); // Output: Value for 'banana': Another fruit
//     printf("Value for 'app': %s\n", value3); // Output: Value for 'app': Application

//     // Deleting a ke
//     bool deleted = removeKey(root, "ap/.ple");
//     if (deleted) {
//         printf("Deleted 'apple'\n");
//     }

//     // Retrieving the value after deletion
//     char *deletedValue = getValueFromTrie(root, "ap/.ple");
//     if (deletedValue == NULL) {
//         printf("Value for 'apple' not found (after deletion)\n");
//     }

//     // Freeing the memory used by the Trie
//     cleanupTrie(root);

//     return 0;
// }

// function to list all paths in the trie
void listAllPathsinTries(TrieNode *root, char *path){
    if(root == NULL) return;
    if (root->endOfWordCount > 0) {
    if (root->isDirectory) {
        printf("\033[0;36m%s\033[0m\n", path); // CYAN color for directories
    } 
    else {
        printf("%s\033[0m\n", path); // WHITE color for files
    }
}

    for(int i = 0; i < ALPHABET_SIZE; i++){
        if(root->trieChildren[i] != NULL){
            char *newPath = (char *)malloc(strlen(path) + 2);
            strcpy(newPath, path);
            newPath[strlen(path)] = i;
            newPath[strlen(path) + 1] = '\0';
            listAllPathsinTries(root->trieChildren[i], newPath);
            free(newPath);
        }
    }
}


void createStringinListAllPaths(char* dest,TrieNode*root,char* path){
    if(root == NULL) return;
    if (root->endOfWordCount > 0) {
        if (root->isDirectory) {
            strcat(dest, "\033[0;36m");
            strcat(dest, path);
            strcat(dest, "\033[0m\n"); // CYAN color for directories
            // printf("HELLO\n");
        } 
        else {
            strcat(dest, path);
            strcat(dest, "\033[0m\n"); // WHITE color for files
        }
    }

    for(int i = 0; i < ALPHABET_SIZE; i++){
        if(root->trieChildren[i] != NULL){
            char *newPath = (char *)malloc(strlen(path) + 2);
            strcpy(newPath, path);
            newPath[strlen(path)] = i;
            newPath[strlen(path) + 1] = '\0';
            createStringinListAllPaths(dest,root->trieChildren[i], newPath);
            free(newPath);
        }
    }
}