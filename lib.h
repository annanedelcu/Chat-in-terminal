/* 
	Nedelcu Ana-Florentina, 325CC 
	Tema 3, Protocoale de Comunicatii
*/

#ifndef __LIB_H__
#define __LIB_H__

#define LEN 256
#define CLEN 15 
#define MAX_CLIENTS 10
#define TIMEOUT_S 1
#define BUFLEN 1024
#define LIST_LEN 2600
#define MSG_LEN 2000

// structura pentru retinerea informatiilor despre un client
struct client {
	char name[LEN];
	int portno;
	int sfd;
	struct timeval cnct_time;
	struct sockaddr_in addr;
};

// structura pentr o "bucata" din fisierul de trimis
struct filecontent {
	int len;
	char buf[BUFLEN];
};

long timediff(struct timeval *t1, struct timeval *t2);
int max(int a, int b);
int exists_client(char* name, struct client * cli, int k);
void remove_client(int i, struct client * cli, int nrc);
char* clientname(int sockfd, struct client * cli, int nrc);

#endif
