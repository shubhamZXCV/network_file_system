#include "caching.h"
RETURN_DATA* fetchValue(struct TrieNode *root, char *key, LRUCache* obj){
    RETURN_DATA* value=fetchFromLRU(obj,key);
    if(value != NULL){
        printf("Cache Hit\n");
        return value;
    }
    printf("Cache Miss\n");
    // value=getValueFromTrie(root,key);
    value=getReturnDataFromTrie(root,key);
    if(value != NULL){
        updateCache(obj,key,value);
    }
    // printf("Value: %s\n",value->value);

    return value;
}


void removeInHash(struct TrieNode *root, char *key, LRUCache* obj){
    int hash=computeHashLRU(key);
    if(obj->cachePointer[hash] != NULL){
        CacheNode* node=obj->cachePointer[hash];
        if(strcmp(node->key,key)==0){
            removeNodeLRU(node);
            free(node->key);
            free(node->data);
            free(node);
            obj->cachePointer[hash]=NULL;
            obj->maxSize++;
        }
    }
    return;
}

void removeKey(struct TrieNode *root, char *key, LRUCache* obj,bool isDirectory){
    if(isDirectory) deleteKeyAndExtensions(root,key);
    else removeKeyinTrie(root,key);
    removeInHash(root,key,obj);
    return;
}

void concatenateStrings(char *dest, const char *src){
    int i = 0;
    while (dest[i] != '\0'){
        i++;
    }

    int j = 0;
    while (src[j] != '\0'){
        dest[i] = src[j];
        j++; i++;
    }
    dest[i] = '\0'; // Add null terminator at the end
}
