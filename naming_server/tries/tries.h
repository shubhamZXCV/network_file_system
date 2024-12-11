// Implementation of Trie functions.
// Tries are preferred over hashmaps because they are superior in space complexity for large datasets.
#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>
#include <pthread.h>
#define ALPHABET_SIZE 128

#include"../return_struct.h"

typedef struct TrieNode
{
    struct TrieNode *trieChildren[ALPHABET_SIZE];
    int endOfWordCount;
    char *data;
    pthread_mutex_t lock;
    pthread_mutex_t writeMutex;
    int readerCount;
    bool isDirectory;
    int latestVersion;
} TrieNode;

// Function to create a new Trie node
TrieNode *createTrieNode();

// Function to insert a key-value pair into the Trie
TrieNode *trieInsert(TrieNode *root,   char *key,   char *data, bool isDirectory);

// Function to get the value associated with a given key in the Trie
// Returns the value if key exists, otherwise NULL
char *getValueFromTrie(TrieNode *root,   char *key);

// Function to delete a key-value pair from the Trie
// Returns true if key found and deleted, false otherwise
bool removeKeyinTrie(TrieNode *root,   char *key);

// Function to free memory used by the Trie
void cleanupTrie(TrieNode *root);
RETURN_DATA* getReturnDataFromTrie(TrieNode *root, char *key);
struct  TrieNode *findNode(TrieNode *root,   char *key);

// LOCK FUNCTIONS
pthread_mutex_t *acquireLock(TrieNode *root,   char *key);
pthread_mutex_t *getNodeWritelock(TrieNode *root,   char *key);

void requestReadAccess(TrieNode *node);
void releaseReadAccess(TrieNode *node);
bool deleteKeyAndExtensions(TrieNode *root, char *key);
void requestWritePermission(TrieNode *node);
void releaseWriteAccess(TrieNode *node);
void listAllPathsinTries(TrieNode *root, char *path);
void createStringinListAllPaths(char* dest,TrieNode*root,char* path);
void createStringinListAllPathsForValue(char* dest,TrieNode*root,char* path,char* value);
TrieNode* getNodeFromTrie(TrieNode *root, char *key);
char* listAllDirectoriesForCopy(char* searchPath,TrieNode* root,char* path,int storageServer);
char* listAllDirectoriesForCopy1(char* searchPath,TrieNode* root,char* path,int storageServer);
#endif /* TRIE_H */
