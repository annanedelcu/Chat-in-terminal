/* 
	Nedelcu Ana-Florentina, 325CC 
	Tema 3, Protocoale de Comunicatii
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include "lib.h"
#include "lib.c"

void error(char *msg)
{
	perror(msg);
	exit(1);
}

// aloc fiecarui tip de comanda pe care o poate primi clientul un numar
int get_cmd_no(char* cmd) {
	if(!strcmp(cmd, "status"))
		return 1;
	else
		if(!strcmp(cmd, "kick"))
			return 2;
		else
			if(!strcmp(cmd, "quit"))
				return 3;
			else
				return 4;
}

int main(int argc, char *argv[])
{	
	//declaratii
	int sockfd, newsockfd, cli_addr_len;
	char buffer[BUFLEN], cmd[CLEN], client_name[LEN], client_list[LIST_LEN];
	struct sockaddr_in serv_addr, cli_addr;
	struct client cli[MAX_CLIENTS+1], new_client;
	int n, i, j, cmd_no, no_fds;
	int nrc;		// nr de clienti activi
	int is_file_waiting;
	int fdmax;		//valoare maxima file descriptor din multimea read_fds
	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//copie a multimii de mai sus
	struct timeval timeout;
	uint32_t int_to_send;

	// initializari
	cmd_no = 0;
	timeout.tv_sec = TIMEOUT_S;
	nrc = 0;

	if (argc < 2) {
		fprintf(stderr,"Usage : %s port\n", argv[0]);
		exit(1);
	}

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);	// foloseste adresa IP a masinii
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) 
		error("ERROR on binding");

	listen(sockfd, MAX_CLIENTS);

	// adaugam noul file descriptor (socketul pe care se asculta conexiuni) 
	// si filedescriptorul pt stdin in multimea read_fds
	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = max(sockfd, STDIN_FILENO);


	// main loop
	while (1) {
		tmp_fds = read_fds; 
		if ((no_fds = select(fdmax + 1, &tmp_fds, NULL, NULL, &timeout)) == -1) 
			error("ERROR in select");
		if(no_fds) {
			for(i = 0; i <= fdmax; i++) {
				if (FD_ISSET(i, &tmp_fds)) {
					if (i == sockfd) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
						// actiunea serverului: accept()
						cli_addr_len = sizeof(cli_addr);
						if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_addr_len)) == -1) {
							error("ERROR in accept");
						} 
						else {
							// incerc sa iau datele clientului nou conectat
							if ((n = recv(newsockfd, &new_client, sizeof(new_client), 0)) <= 0) {
								if (n == 0) {
									//conexiunea s-a inchis
									printf("server: socket %d hung up, nu am date\n", i);
								} 
								else {
									error("ERROR in recv");
								}
								close(newsockfd); 
							} 
					
							else { // am reusit sa iau datele
								if(exists_client(new_client.name, cli, nrc)) {
									memset(buffer, 0, CLEN);
									sprintf(buffer, "duplicate");
									n = send(newsockfd, buffer, strlen(buffer), 0);
									if (n < 0) 
			 							error("ERROR writing to socket");
									close(newsockfd);
								}
								else {
									//adaug noul socket intors de accept() la multimea descriptorilor de citire
									// si in lista de clienti activi
									FD_SET(newsockfd, &read_fds);
									fdmax = max(fdmax, newsockfd);
									new_client.sfd = newsockfd;
									memcpy(&(new_client.addr), &cli_addr, sizeof(cli_addr));
									memcpy(&(cli[nrc]), &new_client, sizeof(struct client));
									nrc++;
								}
							}
						}
					}
					else {
						if(i == STDIN_FILENO) {
							// am primit o comanda de la standard input
							scanf("%s", cmd);
							cmd_no = get_cmd_no(cmd);
							switch(cmd_no) {
								case 1: {		// status
									printf("Lista clientilor activi:\n");
									for(j = 0; j < nrc; j++) {
										printf("%s %s %d\n", cli[j].name, inet_ntoa(cli[j].addr.sin_addr) , cli[j].portno);
									}
									break;
								}
								case 2: {	// kick
									scanf("%s", client_name);
									memset(buffer,0,CLEN);
									sprintf(buffer, "disconnect");
									for(j = 0; j < nrc; j++) {
										if(strcmp(client_name, cli[j].name) == 0) {
											n = send(cli[j].sfd, buffer, strlen(buffer), 0);
											if (n < 0) 
			 									error("ERROR writing to socket");
			 								FD_CLR(cli[j].sfd, &read_fds); 
											remove_client(j, cli, nrc);
											nrc--;
											break;
										}
									}
									break;
								}
								case 3: {	// quit
									memset(buffer,0,CLEN);
									sprintf(buffer, "disconnect");
									for(j = 0; j < nrc; j++) {
										if(j != STDIN_FILENO) {
											n = send(cli[j].sfd, buffer, strlen(buffer), 0);
												if (n < 0) 
			 										error("ERROR writing to socket");	
										}
									}
									exit(0);

								}
								default: printf("Ati introdus o comanda gresita!");
							}

						}
						else {					
							// am primit date pe unul din socketii cu care vorbesc cu clientii
							//actiunea serverului: recv()
							memset(buffer, 0, BUFLEN);
							if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
								if (n == 0) {
									//conexiunea s-a inchis
									printf("server: socket %d hung up\n", i);
								} 
								else {
									error("ERROR in recv");
								}
								close(i); 
								FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul i
								for(j = 0; j < nrc; j++) {
									if(cli[j].sfd == i) {
										remove_client(j, cli, nrc);
										nrc--;
										break;
									}
								}
							} 
					
							else { //recv intoarce > 0
								printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);
								
								if(strcmp(buffer, "listclients") == 0) {
									memset(buffer,0,CLEN);
									sprintf(buffer, "listclients");
									n = send(i, buffer, sizeof(buffer), 0);
									if (n < 0) 
										error("ERROR writing to socket");
									// pun numele clientilor intr-un sir
									strcpy(client_list,"");
									for(j = 0; j < nrc; j++) {
										strcat(client_list, cli[j].name);
										strcat(client_list," ");
									}
									n = send(i, client_list, LIST_LEN, 0);
									if (n < 0) 
										error("ERROR writing to socket");
								}

								if(strcmp(buffer, "infoclient") == 0) {
									memset(client_name,0,LEN);
									if ((n = recv(i, client_name, sizeof(client_name), 0)) <= 0) {
										if (n == 0) {
										//conexiunea s-a inchis
											printf("server: socket %d hung up\n", i);
										} 
										else {
											error("ERROR in recv");
										}
										close(i); 
										FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul i
										for(j = 0; j < nrc; j++) {
											if(cli[j].sfd == i) {
												remove_client(j, cli, nrc);
												nrc--;
												break;
											}
										}
									}
									else { //recv intoarce >0
										printf ("Am primit de la clientul de pe socketul %d, numele clientului: %s\n", i, client_name);
									
										for(j = 0; j < nrc; j++) {
											if(strcmp(cli[j].name, client_name) == 0) {
												memset(buffer,0,CLEN);
												sprintf(buffer, "infoclient");
												n = send(i, buffer, sizeof(buffer), 0);
												if (n < 0) 
													error("ERROR writing to socket");
												n = send(i,&cli[j],sizeof(struct client), 0);
												if (n < 0) 
													error("ERROR writing to socket");
												break;
											}
										}

										if(nrc == j) { // daca nu am gasit clientul cerut
											memset(buffer,0,CLEN);
											sprintf(buffer, "wrongclient");
											n = send(i, buffer, sizeof(buffer), 0);
											if (n < 0) 
												error("ERROR writing to socket");
										}
									}
								}

								if(strcmp(buffer, "broadcast") == 0) {
									for(j = 0; j < nrc; j++) { // trimit informatii pentru toti clientii
										memset(buffer, 0, CLEN);
										sprintf(buffer, "infoclient");
										n = send(i, buffer, sizeof(buffer), 0);
										if (n < 0) 
											error("ERROR writing to socket");
										n = send(i, &cli[j], sizeof(struct client), 0);
										if (n < 0) 
											error("ERROR writing to socket");
									}
								} 
								if(strcmp(buffer, "quit") == 0) {
									close(i);
									FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul i
									for(j = 0; j < nrc; j++) {
										if(cli[j].sfd == i) {
											remove_client(j, cli, nrc);
											nrc--;
											break;
										}
									}
								}
							}
						} 
					}
						
				}
			}

		}
	}

	close(sockfd);
   
	return 0; 
}


