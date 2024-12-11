#include "../globals.h"
#include "path.h"


void copyPath(char *dest, char *src){
    int i = 0;
    while (src[i+2] != '\0'){
        dest[i] = src[i+2];
        i++;
    }
    dest[i] = '\0';
    return;
}

char* removePrefix(char *path){
    if (path[0] != '.') return path;

    int length = strlen(path);
    char *ret = (char *)calloc(length + 5, sizeof(char));
    if(ret == NULL){
        perror("calloc() failed");
        return NULL;
    }
    // int i = 0;
    // while (path[i + 2] != '\0'){
    //     ret[i] = path[i + 2];
    //     i++;
    // }
    // ret[i] = '\0';
    copyPath(ret, path);

    return ret;
}

void copyPathforAdd(char *dest, char *src){
    dest[0] = '.';
    dest[1] = '/';
    int i = 0;
    while (src[i] != '\0'){
        dest[i + 2] = src[i];
        i++;
    }
    dest[i + 2] = '\0';
    return;
}
char* addPrefix(char *path){
    if (path[0] == '.') return path;
    int length = strlen(path);
    char *ret = (char *)calloc(length + 5, sizeof(char));

    if(ret == NULL){
        perror("calloc() failed");
        return NULL;
    }
    // int i = 0;
    // ret[0] = '.';
    // ret[1] = '/';
    // while (path[i] != '\0'){
    //     ret[i + 2] = path[i];
    //     i++;
    // }
    // ret[i + 2] = '\0';
    copyPathforAdd(ret, path);
    return ret;
}

