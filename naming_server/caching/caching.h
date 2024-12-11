#ifndef CACHING_H
#define CACHING_H
#include "../tries/tries.h"
#include "../lru/lru.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include"../return_struct.h"

// typedef struct RETURN_DATA{
//     char* value;
//     bool isDirectory;
// }RETURN_DATA;
void removeKey(struct TrieNode *root, char *key, LRUCache* obj,bool isDirectory);
RETURN_DATA* fetchValue(struct TrieNode *root, char *key, LRUCache* obj);
// char* fetchValue(struct TrieNode *root, char *key, LRUCache* obj);
void concatenateStrings(char *dest, const char *src);
#endif /* CACHING_H */