#ifndef LRUCACHE_H
#define LRUCACHE_H
#include"../return_struct.h"

typedef struct CacheNode {
    char* key;
    char* data;
    bool isDirectory;
    struct CacheNode* prev;
    struct CacheNode* next;
} CacheNode;


typedef struct LRUCache {
    int maxSize;
    CacheNode* head;
    CacheNode* tail;
    CacheNode** cachePointer;
} LRUCache;

// typedef struct RETURN_DATA{
//     char* value;
//     bool isDirectory;
// }RETURN_DATA;

CacheNode* allocateCacheNode(char* key, char* value);
LRUCache* makeLRUCache(int maxSize);
void removeNodeLRU(CacheNode* node);
void insertAtHeadLRU(CacheNode* node, CacheNode* head);
int computeHashLRU(char* key);
RETURN_DATA* fetchFromLRU(LRUCache* obj, char* key);
// char* fetchFromLRU(LRUCache* obj, char* key);
// void updateCache(LRUCache* obj, char* key, char* value);
void updateCache(LRUCache* obj, char* key, RETURN_DATA* data);
void deallocateLRUCache(LRUCache* obj);
#endif /* LRUCACHE_H */