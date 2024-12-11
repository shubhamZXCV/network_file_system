#ifndef LOG_H
#define LOG_H
#include<stdbool.h>
void logMessageRecieve(int socket,int dataType,void** messageStruct,bool fromClient);
void logMessageSent(int socket,int dataType,void** messageStruct,bool isNull,char* request,bool isClient);
#endif