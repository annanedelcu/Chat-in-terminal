/* 
	Nedelcu Ana-Florentina, 325CC 
	Tema 3, Protocoale de Comunicatii
*/

#ifndef __LIB_C__
#define __LIB_C__

#include "lib.h"

// intoarce o diferenta de timp in milisecunde
long timediff(struct timeval *t1, struct timeval *t2) {
	long msec;
	msec=(t2->tv_sec-t1->tv_sec)*1000;
	msec+=(t2->tv_usec-t1->tv_usec)/1000;
	return msec;
}

// face maximul intre 2 numere intregi
int max(int a, int b) {
	if(a > b)
		return a;
	return b;
}

// verifica existenta unui client in vectorul de clienti
int exists_client(char* name, struct client * cli, int k) {
	int i;
	for(i = 0; i < k; i++) {
		if(strcmp(name, cli[i].name) == 0)
			return 1;
	}
	return 0;
}

// sterge un client din vectorul de clienti
void remove_client(int i, struct client * cli, int nrc) {
	int j;
	for(j = i; j < nrc - 1; j++) {
		memcpy(&cli[j], &cli[j+1], sizeof(struct client));
	}
}

// intoarce numele unui client in functie de socketu pe care se comunica cu el
char* clientname(int sockfd, struct client * cli, int nrc) {
	int i;
	for(i = 0; i < nrc; i++)
		if(cli[i].sfd == sockfd)
			return cli[i].name;
}

#endif