#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "lru.h"

CacheNode* allocateCacheNode(char* key, char* data) {
    CacheNode* newNode = (CacheNode*)malloc(sizeof(CacheNode));
    newNode->key = strdup(key);
    newNode->data = strdup(data);
    newNode->prev = NULL;
    newNode->next = NULL;
    
    return newNode;
}

LRUCache* makeLRUCache(int maxSize) {
    LRUCache* cachePointer = (LRUCache*)malloc(sizeof(LRUCache));
    cachePointer->maxSize = maxSize;
    cachePointer->cachePointer = (CacheNode**)malloc(sizeof(CacheNode*) * 10000); // Assuming key values are less than 10000
    cachePointer->head = allocateCacheNode("head", "");
    cachePointer->tail = allocateCacheNode("tail", "");
    cachePointer->head->next = cachePointer->tail;
    cachePointer->tail->prev = cachePointer->head;
    return cachePointer;
}

void removeNodeLRU(CacheNode* node) {
    CacheNode* prev = node->prev;
    CacheNode* nextNode = node->next;
    prev->next = nextNode;
    nextNode->prev = prev;
}

void insertAtHeadLRU(CacheNode* node, CacheNode* head) {
    CacheNode* nextNode = head->next;
    head->next = node;
    node->prev = head;
    node->next = nextNode;
    nextNode->prev = node;
}

void delete(LRUCache* obj, char* key) {
    int hash = computeHashLRU(key);
    if (obj->cachePointer[hash] != NULL) {
        CacheNode* node = obj->cachePointer[hash];
        if (strcmp(node->key, key) == 0) {
            removeNodeLRU(node);
            obj->cachePointer[hash] = NULL;
            free(node->key);
            free(node->data);
            free(node);
            obj->maxSize++;
        }
    }
}

int computeHashLRU(char* key) {
    unsigned long hash = 5381; // Initializing hash with a prime number
    int c;

    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return (int)(hash % 10000); // Assuming key values are less than 10000
}

RETURN_DATA* fetchFromLRU(LRUCache* obj, char* key) {
    RETURN_DATA* returnData = (RETURN_DATA*)malloc(sizeof(RETURN_DATA));
    int hash = computeHashLRU(key);
    if (obj->cachePointer[hash] != NULL) {
        CacheNode* node = obj->cachePointer[hash];
        if (strcmp(node->key, key) == 0) {
            removeNodeLRU(node);
            insertAtHeadLRU(node, obj->head);
            // return node->data;
            returnData->value = node->data; // data contains the value
            returnData->isDirectory = node->isDirectory;
            return returnData;
        }
    }
    free(returnData);
    return NULL;
}

// void updateCache(LRUCache* obj, char* key, char* data) {
//     int hash = computeHashLRU(key);
//     if (obj->cachePointer[hash] != NULL) {
//         CacheNode* node = obj->cachePointer[hash];
//         if (strcmp(node->key, key) == 0) {
//             free(node->data);
//             node->data = strdup(data);
//             removeNodeLRU(node);
//             insertAtHeadLRU(node, obj->head);
//             return;
//         }
//     }
//     CacheNode* newNode = allocateCacheNode(key, data);
//     if (obj->maxSize <= 0) {
//         CacheNode* tailPrev = obj->tail->prev;
//         removeNodeLRU(tailPrev);
//         obj->cachePointer[computeHashLRU(tailPrev->key)] = NULL;
//         free(tailPrev->key);
//         free(tailPrev->data);
//         free(tailPrev);
//         obj->maxSize++;
//     }
//     insertAtHeadLRU(newNode, obj->head);
//     obj->cachePointer[hash] = newNode;
//     obj->maxSize--;
// }

void updateCache(LRUCache* obj, char* key, RETURN_DATA* data) {
    int hash = computeHashLRU(key);
    if (obj->cachePointer[hash] != NULL) {
        CacheNode* node = obj->cachePointer[hash];
        if (strcmp(node->key, key) == 0) {
            free(node->data);
            node->data = strdup(data->value);
            node->isDirectory = data->isDirectory;
            removeNodeLRU(node);
            insertAtHeadLRU(node, obj->head);
            return;
        }
    }
    if(data->value == NULL) {
        fprintf(stderr, "Value is NULL\n");
        return;
    }
    CacheNode* newNode = allocateCacheNode(key, data->value);
    newNode->isDirectory = data->isDirectory;
    if (obj->maxSize <= 0) {
        CacheNode* tailPrev = obj->tail->prev;
        removeNodeLRU(tailPrev);
        obj->cachePointer[computeHashLRU(tailPrev->key)] = NULL;
        free(tailPrev->key);
        free(tailPrev->data);
        free(tailPrev);
        obj->maxSize++;
    }
    insertAtHeadLRU(newNode, obj->head);
    obj->cachePointer[hash] = newNode;
    obj->maxSize--;
}

void deallocateLRUCache(LRUCache* obj) {
    for (int i = 0; i < 10000; i++) {
        if (obj->cachePointer[i] != NULL) {
            free(obj->cachePointer[i]->key);
            free(obj->cachePointer[i]->data);
            free(obj->cachePointer[i]);
        }
    }
    free(obj->cachePointer);
    free(obj->head->key);
    free(obj->head->data);
    free(obj->tail->key);
    free(obj->tail->data);
    free(obj->head);
    free(obj->tail);
    free(obj);
}

// int main() {
//     LRUCache* cachePointer = makeLRUCache(5);
// // Hash size should be less than 10000.
//     updateCache(cachePointer, "key1", "value1");
//     updateCache(cachePointer, "key2", "value2");
//     printf("%s\n", fetchFromLRU(cachePointer, "key1")); 
//     updateCache(cachePointer, "key3", "value3");
//     printf("%s\n", fetchFromLRU(cachePointer, "key2")); 
//     updateCache(cachePointer, "key4", "value4");
//     printf("%s\n", fetchFromLRU(cachePointer, "key1")); 
//     printf("%s\n", fetchFromLRU(cachePointer, "key3")); 
//     printf("%s\n", fetchFromLRU(cachePointer, "key4")); 

//     deallocateLRUCache(cachePointer);
//     return 0;
// }
