/* 
	Nedelcu Ana-Florentina, 325CC 
	Tema 3, Protocoale de Comunicatii
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "lib.h"
#include "lib.c"

void error(char *msg)
{
	perror(msg);
	exit(0);
}

// aloc fiecarui tip de comanda pe care o poate primi clientul un numar
int get_cmd_no(char* cmd) {
	if(!strcmp(cmd, "listclients"))
		return 1;
	else
		if(!strcmp(cmd, "infoclient"))
			return 2;
		else
			if(!strcmp(cmd, "message"))
				return 3;
			else
				if(!strcmp(cmd, "broadcast")) 
					return 4;
				else
					if(!strcmp(cmd, "sendfile"))
						return 5;
					else
						if(!strcmp(cmd, "history"))
							return 6;
						else
							if(!strcmp(cmd, "quit"))
								return 7;
							else
								return 8;
}

int main(int argc, char *argv[])
{
	// declaratii

	int sockfd, clisockfd, n, cmd_no, is_file_waiting, fdmax;
	int cli_addr_len, no_fds, newsockfd, file_sockfd, i, j, nrc;
	int print;
	struct sockaddr_in serv_addr, my_addr, cli_addr;
	char buffer[BUFLEN], cmd[CLEN], *buffer_r, client_name[LEN], history_filename[LEN + 8];
	char client_list[LIST_LEN], msg[MSG_LEN], filename[LEN], recv_filename[LEN];
	struct client this_client, new_client, cli[MAX_CLIENTS+1], client_info;
	struct timeval timeout, current_time;
	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//copie a multimii de mai sus
	int file, recvfile, history;		// fisier
	int file_to_send, count;	
	struct filecontent cont, recv_cont;
	time_t now;
	char *timestamp;

	// initializari

	cmd_no = 0;
	is_file_waiting = 0;	// // nu am niciun fisier ce asteapta sa fie trimis
	timeout.tv_sec = TIMEOUT_S;
	print = 0;
	nrc = 0;
	file_to_send = 0;	// nu am niciun fisier de trimis

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if (argc < 5) {
		fprintf(stderr,"Usage %s client_name client_port server_address server_port \n", argv[0]);
		exit(0);
	}
	
	// deschid socket pt. conexiunea la server
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) 
		error("ERROR opening socket");

	// deschid socket pt. conexiuni cu clienti
	clisockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (clisockfd < 0) 
		error("ERROR opening socket");

	// datele serverului
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[4]));
	inet_aton(argv[3], &serv_addr.sin_addr);
	
	// datele clientului

	memset(this_client.name,0,LEN);
	memcpy(this_client.name, argv[1], strlen(argv[1]));
	this_client.portno = atoi(argv[2]);

	sprintf(history_filename, "%s_history", this_client.name);
	history = open(history_filename, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0644);

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(this_client.portno);
	inet_aton("127.0.0.1", &my_addr.sin_addr);

	if(bind(clisockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0) 
		error("ERROR on binding");
	
	listen(clisockfd, MAX_CLIENTS);

	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = max(sockfd, STDIN_FILENO);

	//adaug noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(clisockfd, &read_fds);
	fdmax = max(fdmax, clisockfd);

	// ma conectez la server
	if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) 
		error("ERROR connecting");

	// trimit datele de identificare catre server
	gettimeofday(&(this_client.cnct_time), NULL);

	n = send(sockfd,&this_client,sizeof(struct client), 0);
	if (n < 0) 
		error("ERROR writing to socket");

	while(1){

		tmp_fds = read_fds; 
		if ((no_fds = select(fdmax + 1, &tmp_fds, NULL, NULL, &timeout)) == -1) 
			error("ERROR in select");
		else
			if(no_fds == 0) {
				if(is_file_waiting) {
					//am de trimis continut dintr-un fisier
					count = read(file, cont.buf, BUFLEN);
					if(count < 0) 
						error("ERROR on reading file");
					else 
						if(count == 0) { // am terminat de trimis
							is_file_waiting = 0;
							memset(buffer,0,CLEN);
							sprintf(buffer, "filesent");
							n = send(file_sockfd,buffer,sizeof(buffer), 0);
							if (n < 0) 
								error("ERROR writing to socket");
						}
						else {
							memset(buffer, 0, CLEN);
							sprintf(buffer, "filecontent");
							n = send(file_sockfd,buffer,sizeof(buffer), 0);
							if (n < 0) 
								error("ERROR writing to socket");
							cont.len = count;
							n = send(file_sockfd,&cont,sizeof(struct filecontent), 0);
							if (n < 0) 
								error("ERROR writing to socket");
						}
				}
			}

		if(no_fds) {
			for(i = 0; i <= fdmax; i++) {
				if (FD_ISSET(i, &tmp_fds)) {
					if (i == clisockfd) {
						// a venit ceva pe socketul pe care se realizeaza conexiuni cu alti clienti
						cli_addr_len = sizeof(cli_addr);
						if ((newsockfd = accept(clisockfd, (struct sockaddr *)&cli_addr, &cli_addr_len)) == -1) {
							error("ERROR in accept");
						} 
						else {
							// incerc sa iau datele clientului nou conectat
							if ((n = recv(newsockfd, &new_client, sizeof(new_client), 0)) <= 0) {
								if (n == 0) {
									//conexiunea s-a inchis
								} 
								else {
									error("ERROR in recv");
								}
								close(i); 
							}
							else { // am reusit sa iau datele
								if(!exists_client(new_client.name, cli, nrc)) {
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
								case 1: {		// listclients
									memset(buffer,0,CLEN);
									sprintf(buffer, "listclients");
									n = send(sockfd,buffer,sizeof(buffer), 0);
									if (n < 0) 
										error("ERROR writing to socket");
									break;
								}
								case 2: {	// infoclient
									print = 1;	// informatiile primite se vor afisa
									memset(buffer,0,CLEN);
									sprintf(buffer, "infoclient");
									n = send(sockfd,buffer,sizeof(buffer), 0);
									if (n < 0) 
										error("ERROR writing to socket");

									scanf("%s", client_name);
									n = send(sockfd,client_name,strlen(client_name), 0);
									if (n < 0) 
										error("ERROR writing to socket");
									break;
								}
								case 3: {	// message
									scanf("%s", client_name);
									getchar();
									gets(msg);
						
									// fac cerere la server pentru 
									// informatii despre client 
									print = 0;
									memset(buffer,0,CLEN);
									sprintf(buffer, "infoclient");
									n = send(sockfd,buffer,sizeof(buffer), 0);
									if (n < 0) 
										error("ERROR writing to socket");
								
									n = send(sockfd,client_name,strlen(client_name), 0);
									if (n < 0) 
										error("ERROR writing to socket");							
									break;
								}
								case 4: {	// broadcast
									getchar();
									gets(msg);
									// fac cerere la server pt informatii despre toti 
									// clientii din sistem
									print = 0;
									memset(buffer,0,CLEN);
									sprintf(buffer, "broadcast");
									n = send(sockfd,buffer,sizeof(buffer), 0);
									if (n < 0) 
										error("ERROR writing to socket");
									break;
								}
								case 5: {	// sendfile
									scanf("%s", client_name);
									scanf("%s", filename);
									file = open(filename,O_RDONLY);
									if(file < 0)
										printf("Fisierul nu exista!\n");
									else {
										file_to_send = 1;
										// fac cerere la server pentru 
										// informatii despre client
										print = 0; 
										memset(buffer,0,CLEN);
										sprintf(buffer, "infoclient");
										n = send(sockfd,buffer,sizeof(buffer), 0);
										if (n < 0) 
											error("ERROR writing to socket");
										n = send(sockfd,client_name,strlen(client_name), 0);
										if (n < 0) 
											error("ERROR writing to socket");	
										}
										
									break;
								}
								case 6: { 	// history
									// ma pozitionez la inceputul fisierului
									lseek(history, 0L, SEEK_SET);
									memset(buffer, 0, BUFLEN);
									// ii afisez continutul
									while(read(history, buffer, BUFLEN)) {
										printf("%s", buffer);
										memset(buffer, 0, BUFLEN);
									}
									break;
								}
								case 7: {	// quit
									memset(buffer,0,CLEN);
									sprintf(buffer, "quit");
									n = send(sockfd,buffer,sizeof(buffer), 0);
									if (n < 0) 
										error("ERROR writing to socket");
									close(clisockfd);
									close(sockfd);
									for(j = 0; j < nrc; j++) {
										close(cli[j].sfd);
									}
									exit(0);
									break;
								}
								default: printf("Ati introdus o comanda gresita!");
							}
						}
						else if(i == sockfd) {
							// primesc date de la server
							// incerc sa preiau mesajul de la server
							if ((n = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0) {
								if (n == 0) {
									//conexiunea s-a inchis
									printf("client: server socket %d hung up\n", sockfd);
								} 
								else {
									error("ERROR in recv");
									exit(1);
								}
								close(sockfd); 
								exit(0);
							} 
							else {
								if(strcmp(buffer, "duplicate") == 0) {
									printf("Exista deja un client cu acest nume in sistem!\n");
									exit(0);
								}

								if(strcmp(buffer, "wrongclient") == 0) {
									printf("Clientul cautat nu exista!\n");
								}

								if(strcmp(buffer, "disconnect") == 0) {
									close(sockfd);
									close(clisockfd);
									for(j = 0; j < nrc; j++) {
										close(cli[j].sfd);
									}
									exit(0);
								}
								
								if(strcmp(buffer,"listclients") == 0) {
									if ((n = recv(sockfd, client_list, LIST_LEN, 0)) <= 0) {
										if (n == 0) {
										//conexiunea s-a inchis
											printf("client: server socket %d hung up\n", sockfd);
										} 
										else {
											error("ERROR in recv");
											exit(1);
										}
										close(sockfd); 
										exit(0);
									} 
									else {
										printf("Lista clientilor:\n%s\n", client_list);
									}
								}

								if(strcmp(buffer,"infoclient") == 0) {
									if ((n = recv(sockfd, &client_info, sizeof(struct client), 0)) <= 0) {
										if (n == 0) {
										//conexiunea s-a inchis
											printf("client: server socket %d hung up\n", sockfd);
										} 
										else {
											error("ERROR in recv");
											exit(1);
										}
										close(sockfd); 
										exit(0);
									} 
									else {
										if(print) {
											gettimeofday(&current_time, NULL);
											printf("%s \n %d \n %ld ms \n", client_info.name, client_info.portno, timediff(&client_info.cnct_time, &current_time));
										}
										else {
											// ma uit sa vad daca clientul intors sunt chiar eu
											if(strcmp(client_info.name, this_client.name) == 0) {
												if(file_to_send) {	// am primit comanda sendfile
													printf("Am deja acest fisier!");
												}
												else {	// am primit comanda message
													// in timestamp iau data si timpul curente
													now = time(NULL);
													timestamp = ctime(&now);
													timestamp[strlen(timestamp) - 1] = 0;
													printf("[%s][%s]%s\n", timestamp, this_client.name, msg);
												}
											}
											else {
												// caut sa vad daca am deja conexiune cu clientul cautat
												for(j = 0; j < nrc; j++) {
													if(strcmp(cli[j].name, client_info.name) == 0) {
														if(file_to_send) {	// am primit comanda sendfile
															//trimit numele fisierului
															memset(buffer,0,CLEN);
															sprintf(buffer, "sendfile");
															n = send(newsockfd,buffer,sizeof(buffer), 0);
															if (n < 0) 
																error("ERROR writing to socket");

															memset(buffer,0,LEN);
															sprintf(buffer, "%s", filename);
															n = send(newsockfd,buffer,sizeof(buffer), 0);
															if (n < 0) 
																error("ERROR writing to socket");
															file_sockfd = newsockfd;
															is_file_waiting = 1;
															file_to_send = 0;
														}
														else {	// am primit comanda message
															// ii trimit mesajul
															memset(buffer,0,CLEN);
															sprintf(buffer, "message");
															n = send(newsockfd,buffer,sizeof(buffer), 0);
															if (n < 0) 
																error("ERROR writing to socket");

															n = send(newsockfd,msg,sizeof(msg), 0);
															if (n < 0) 
																error("ERROR writing to socket");
														}
													break;
													}
												}
												if(j == nrc) {
													//ma conectez la client
													newsockfd = socket(AF_INET, SOCK_STREAM, 0);	
													if(newsockfd < 0) 
														error("ERROR opening socket");
													
													client_info.addr.sin_port = htons(client_info.portno);
													if (connect(newsockfd,(struct sockaddr *) &(client_info.addr),sizeof(client_info.addr)) < 0) 
														error("ERROR connecting");
													
													//il adaug in lista de clienti cu care am deschise conexiuni si 
													// in readfds
													FD_SET(newsockfd, &read_fds);
													fdmax = max(fdmax, newsockfd);
													client_info.sfd = newsockfd;
													memcpy(&(cli[nrc]), &client_info, sizeof(struct client));
													nrc++;

													// trimit datele de identificare catre client
													gettimeofday(&(this_client.cnct_time), NULL);
													n = send(newsockfd,&this_client,sizeof(struct client), 0);
													if (n < 0) 
														error("ERROR writing to socket");
													if(file_to_send) {	// am primit comanda sendfile
														//trimit numele fisierului
														memset(buffer,0,CLEN);
														sprintf(buffer, "sendfile");
														n = send(newsockfd,buffer,sizeof(buffer), 0);
														if (n < 0) 
															error("ERROR writing to socket");

														memset(buffer,0,LEN);
														sprintf(buffer, "%s", filename);
														n = send(newsockfd,buffer,sizeof(buffer), 0);
														if (n < 0) 
															error("ERROR writing to socket");
														file_sockfd = newsockfd;
														is_file_waiting = 1;
														file_to_send = 0;
													}
													else {	// am primit comanda message
														// ii trimit mesajul
														memset(buffer,0,CLEN);
														sprintf(buffer, "message");
														n = send(newsockfd,buffer,sizeof(buffer), 0);
														if (n < 0) 
															error("ERROR writing to socket");

														n = send(newsockfd,msg,sizeof(msg), 0);
														if (n < 0) 
															error("ERROR writing to socket");
													}							
												}
											}
										}
									}
								}
							}
						}
						else {
							// am primit date de la alt client
							memset(buffer, 0, CLEN);
							if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
								if (n == 0) {
									//conexiunea s-a inchis
									printf("socket %d hung up\n", i);
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
								if(strcmp(buffer, "message") == 0) {
									memset(msg, 0, MSG_LEN);
									if ((n = recv(i, msg, sizeof(msg), 0)) <= 0) {
										if (n == 0) {
										//conexiunea s-a inchis
											printf("socket %d hung up\n", i);
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
									else { // am primit mesaj, il afisez
										// in timestamp iau data si timpul curente
										now = time(NULL);
										timestamp = ctime(&now);
										timestamp[strlen(timestamp) - 1] = 0;
										printf("[%s][%s]%s\n", timestamp, clientname(i, cli, nrc), msg);
										memset(buffer, 0,LEN);
										sprintf(buffer, "[%s] ", clientname(i, cli, nrc));
										write(history, buffer, strlen(buffer));
										write(history, msg, strlen(msg));
										write(history, &("\n"), 1);
									}
								}

								if(strcmp(buffer, "sendfile") == 0) {
									if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
										if (n == 0) {
										//conexiunea s-a inchis
											printf("socket %d hung up\n", i);
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
									else  {
										sprintf(recv_filename, "%s_primit", buffer);
										recvfile = open(recv_filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
									}
									
								}

								if(strcmp(buffer, "filecontent") == 0) {
									if ((n = recv(i, &recv_cont, sizeof(struct filecontent), 0)) <= 0) {
										if (n == 0) {
										//conexiunea s-a inchis
											printf("socket %d hung up\n", i);
										} 
										else {
											error("ERROR in recv");
										}
										close(i);
										close(recvfile); 
										FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul i
										for(j = 0; j < nrc; j++) {
											if(cli[j].sfd == i) {
												remove_client(j, cli, nrc);
												nrc--;
												break;
											}
										}
									}
									else  {
										count = write(recvfile, recv_cont.buf, recv_cont.len);
										if(count < 0)
											error("ERROR on writing file");
									}
								}

								if(strcmp(buffer, "filesent") == 0) {
									printf("Am primit fisierul %s de la %s\n", recv_filename, clientname(i, cli, nrc));
									close(recvfile);
									memset(buffer, 0, 2*LEN);
									sprintf(buffer, "[%s] %s\n", clientname(i, cli, nrc), recv_filename);
									write(history, buffer, strlen(buffer));
								}
							}
						}
					}
				}
			}
		}
	}

	close(sockfd);
	close(clisockfd);
	return 0;
}



