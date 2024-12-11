#include"./storage_server.h"
#ifndef NETWORK_H
#define NETWORK_H


void *recieve_by_ss(int clientSocket, int dataStruct);
void send_by_ss(int socket,void * dataToSend,int data_type);
char * get_ip_address();


#endif
