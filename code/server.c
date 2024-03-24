#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include <sys/wait.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include <sys/select.h>


char root[256]; // root directory of code
int active_users = 0; // number of active users

// create a struct called user with fields username, password, wd (working directory)
struct user {
	char username[256];
	char password[256];
	char wd[256];
	int logIn;
	int socket; // socket of the user
};

// create an array of users
struct user users[10];

// function to authenticate the user
int user_auth(int client_socket, char username[256]) {

	// checking if the user is already logged in
	for (int i = 0; i < active_users; i++) {
		if (strcmp(users[i].username, username) == 0) {
			printf("User already logged in\n");
			char buffer[256];
			bzero(buffer, sizeof(buffer));
			strcpy(buffer, "400 User already logged in.");
			send(client_socket, buffer, sizeof(buffer), 0);
			return 0;
		}
	}

	// switching to the root directory
	chdir(root);

	// open the file ../users.csv
	FILE *file = fopen("../users.csv", "r");
	if (file == NULL) {
		perror("fopen");
		exit(-1);
	}
	// read the file line by line
	char line[256]; // assuming (<username>,<password>) cannot exceed 256 characters
	while (fgets(line, sizeof(line), file)) {
		// split the line into username and password
		char *token = strtok(line, ",");
		char *username_file = token;
		token = strtok(NULL, ",");
		char *password_file = token;
		
		// if the username is valid, request for password
		if (strcmp(username, username_file) == 0) {
			printf("\nSuccessful username verification\n");
			char buffer[256];
			bzero(buffer, sizeof(buffer));
			strcpy(buffer, "331 Username OK, need password.");
			send(client_socket, buffer, sizeof(buffer), 0);
			
			// now receive the password from the client, if any other command is received, return error
			bzero(buffer, sizeof(buffer));
			recv(client_socket, buffer, sizeof(buffer), 0);
			printf("\n[%d]> %s\n", client_socket, buffer);

			if (strncmp(buffer, "PASS ", 5) == 0) {
				char *password = buffer + 5; // extract the password from the buffer
				password[strcspn(password, "\n")] = 0; // remove trailing newline char from password
				password_file[strcspn(password_file, "\n")] = 0; // remove trailing newline char from password_file
				if (strcmp(password, password_file) == 0) {
					printf("Successful login\n");
					char buffer[256];
					bzero(buffer, sizeof(buffer));
					strcpy(buffer, "230 User logged in, proceed.");
					send(client_socket, buffer, sizeof(buffer), 0);
					fclose(file);

					// adding the user to the users array
					strcpy(users[active_users].username, username);
					strcpy(users[active_users].password, password);

					// cwd is dir/code but we assign cwd as dir/server/<username> for this user
					char cwd[256];
					getcwd(cwd, sizeof(cwd));
					char *last_slash = strrchr(cwd, '/'); // find the last slash in the cwd
					*last_slash = 0; // removing the "code" in current wd
					strcat(cwd, "/server/");
					strcat(cwd, username);
					strcpy(users[active_users].wd, cwd);
					users[active_users].logIn = 1;
					users[active_users].socket = client_socket;
					active_users++;
					return 1;
				}
				else {
					printf("Incorrect password\n");
					break; // if the password is incorrect, break the loop
				}
			}
			printf("Invalid command\n");
			break; // if the command is invalid, break the loop
		}
	}
	
	char buffer[256];
	printf("HERE\n");
	bzero(buffer, sizeof(buffer));
	strcpy(buffer, "530 Not logged in.");
	send(client_socket, buffer, sizeof(buffer), 0);
	fclose(file);
	return 0;
}




int main()
{
	// storing root directory
	getcwd(root, sizeof(root));

	//socket
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	if(server_sd<0) {
		perror("socket:");
		exit(-1);
	}
	//setsock
	int value  = 1;
	setsockopt(server_sd,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9021); // control channel
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // loopback address

	//bind
	if(bind(server_sd, (struct sockaddr*)&server_addr,sizeof(server_addr))<0) {
		perror("bind failed");
		exit(-1);
	}
	//listen
	if(listen(server_sd,5)<0) {
		perror("listen failed");
		close(server_sd);
		exit(-1);
	}

	// DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
	fd_set all_sockets;
	fd_set ready_sockets;

	// zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	// adds one socket (the current socket) to the fd set of all sockets
	FD_SET(server_sd,&all_sockets);

	// client address
	struct sockaddr_in cliaddr;
    bzero(&cliaddr,sizeof(cliaddr));
    unsigned int len = sizeof(cliaddr);

	int userIdx = -1; // initializing the user index

	while(1) {

		// so that is why each iteration of the loop, we copy the all_sockets set into that temp fd_set
		ready_sockets = all_sockets;

		if(select(FD_SETSIZE,&ready_sockets,NULL,NULL,NULL)<0)
		{
			perror("select error");
			exit(EXIT_FAILURE);
		}

		for(int fd = 0 ; fd < FD_SETSIZE; fd++)
		{
			// check to see if that fd is SET
			if(FD_ISSET(fd,&ready_sockets))
			{
				// if fd is our server socket, this means it is telling us there is a NEW CONNECTION that we can accept
				if(fd==server_sd)
				{
					// accept that new connection
					int client_sd = accept(server_sd,(struct sockaddr *) &cliaddr, &len);
					
					// print connection details
					printf("\nConnection established with user %d\n", client_sd);
					printf("Their port: %d\n", ntohs(cliaddr.sin_port));

					// sending welcome message
					char buffer[256];
					bzero(buffer,sizeof(buffer));
					strcpy(buffer, "220 Service ready for new user.");
					send(client_sd,buffer,sizeof(buffer),0);
					
					// add the newly accepted socket to the set of all sockets that we are watching
					FD_SET(client_sd,&all_sockets);
				}

				// handling client connection
				else
				{
					char buffer[256];
					bzero(buffer,sizeof(buffer));
					int bytes = recv(fd,buffer,sizeof(buffer),0);
					if (bytes == 0) {
						printf("\nConnection [%d] closed\n", fd);
						close(fd);
						// removing the socket from the list of file descriptors that we are watching
						FD_CLR(fd,&all_sockets);
						// removing the user from the users array
						for (int i = 0; i < active_users; i++) {
							if (users[i].socket == fd) {
								for (int j = i; j < active_users - 1; j++) {
									users[j] = users[j + 1];
								}
								active_users--;
							}
						}
						break;
					}
					printf("\n[%d]> %s\n", fd, buffer);

					int logIn = 0; // bool for if the current user is logged in
					// matching this fd with the user
					for (int i = 0; i < active_users; i++) {
						if (users[i].socket == fd) {
							userIdx = i;
							chdir(users[i].wd);
							if (users[i].logIn == 1) {
								logIn = 1;
							}
						}
					}

					// // print all the active users
					// printf("Active users: ");
					// for (int i = 0; i < active_users; i++) {
					// 	printf("%s ", users[i].username);
					// 	printf("%d ", users[i].socket);
					// }
					// printf("\n");

					// // print current fd
					// printf("\nCurrent fd: %d\n", fd);

					// if the command is "USER <username>", authenticate the user
					if (strncmp(buffer, "USER ", 5) == 0) {
						if (logIn == 1) {
							printf("User already logged in\n");
							bzero(buffer, sizeof(buffer));
							strcpy(buffer, "230 User logged in, proceed.");
							send(fd, buffer, sizeof(buffer), 0);
						}
						else {
							char *username = buffer + 5;
							username[strcspn(username, "\n")] = 0; // remove trailing newline char from username
							if (user_auth(fd, username) == 1) {
								// setting userIdx
								for (int i = 0; i < active_users; i++) {
									if (strcmp(users[i].username, username) == 0) {
										userIdx = i;
									}
								}
								// setting cwd as this user's working directory
								chdir(users[userIdx].wd);
							}
						}
					}

					else if (strncmp(buffer, "PASS ", 5) == 0) {
						bzero(buffer, sizeof(buffer));
						strcpy(buffer, "503 Bad sequence of commands.");
						send(fd, buffer, sizeof(buffer), 0);
					}

					// if the command is "BYE!", close the connection
					else if(strcmp(buffer, "QUIT") == 0) {
						printf("Connection [%d] closed\n", fd);
						bzero(buffer,sizeof(buffer)); //clear the buffer
						strcpy(buffer, "221 Service closing control connection.");
						send(fd,buffer,sizeof(buffer),0); //send message to client
						close(fd);
						// removing the socket from the list of file descriptors that we are watching
						FD_CLR(fd,&all_sockets);
						// removing the user from the users array
						for (int i = 0; i < active_users; i++) {
							if (users[i].socket == fd) {
								for (int j = i; j < active_users - 1; j++) {
									users[j] = users[j + 1];
								}
								active_users--;
							}
						}
						break;
					}

					else if (strcmp(buffer, "PWD") == 0) {
						bzero(buffer, sizeof(buffer));
						if (logIn == 1) {
							printf("Working directory: %s\n", users[userIdx].wd);
							strcpy(buffer, "257 ");
							strcat(buffer, users[userIdx].wd);
							send(fd, buffer, sizeof(buffer), 0);
						}
						else {
							printf("Not logged in\n");
							strcpy(buffer, "530 Not logged in.");
							send(fd, buffer, sizeof(buffer), 0);
						}
					}

					else if (strncmp(buffer, "CWD ", 4) == 0) {
						if (logIn == 1) {
							char *new_wd = buffer + 4;
							// new_wd[strcspn(new_wd, "\n")] = 0; // remove trailing newline char from dir
							if (chdir(new_wd) == 0) {
								char dir[256];
								getcwd(dir, sizeof(dir));
								printf("Changing directory to: %s\n", dir);
								bzero(users[userIdx].wd, sizeof(users[userIdx].wd));
								strcpy(users[userIdx].wd, dir);
								bzero(buffer, sizeof(buffer));
								strcpy(buffer, "200 directory changed to ");
								strcat(buffer, dir);
								send(fd, buffer, sizeof(buffer), 0);
							}
							else {
								bzero(buffer, sizeof(buffer));
								printf("Failed to change working directory\n");
								strcpy(buffer, "550 No such file or directory.");
								send(fd, buffer, sizeof(buffer), 0);
							}
						}
						else {
							bzero(buffer, sizeof(buffer));
							printf("Not logged in\n");
							strcpy(buffer, "530 Not logged in.");
							send(fd, buffer, sizeof(buffer), 0);
						}
					}

					else if (strncmp(buffer, "PORT ", 5) == 0) {
						if (logIn == 1) {
							int pid = fork(); // create a child process to handle the data connection
							if (pid == 0) {
								// extract the IP and port from the PORT h1,h2,h3,h4,p1,p2 command
								int h1, h2, h3, h4, p1, p2;
								char *fields = buffer + 5;
								char *token = strtok(fields, ",");
								h1 = atoi(token);
								token = strtok(NULL, ",");
								h2 = atoi(token);
								token = strtok(NULL, ",");
								h3 = atoi(token);
								token = strtok(NULL, ",");
								h4 = atoi(token);
								token = strtok(NULL, ",");
								p1 = atoi(token);
								token = strtok(NULL, ",");
								p2 = atoi(token);

								// ip string and data_port int
								char ip[256];
								bzero(ip, sizeof(ip));
								sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
								int data_port = p1 * 256 + p2;

								// print the IP and port
								printf("IP: %s\n", ip);
								printf("Port: %d\n", data_port);

								// send acknowledgement to client
								bzero(buffer, sizeof(buffer));
								strcpy(buffer, "200 PORT command successful.");
								send(fd, buffer, sizeof(buffer), 0);

								// create the data connection
								int data_sd = socket(AF_INET, SOCK_STREAM, 0);
								if (data_sd < 0) {
									perror("socket:");
									exit(-1);
								}

								// allowing reuse of address
								setsockopt(data_sd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

								// the local port
								struct sockaddr_in local_addr;
								bzero(&local_addr, sizeof(local_addr));
								local_addr.sin_family = AF_INET;
								local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
								local_addr.sin_port = htons(9020); // the data port server will connect from

								// bind to the local port i.e. 9020
								if (bind(data_sd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
									perror("bind");
									exit(-1);
								}

								// the server (client) address to connect to
								struct sockaddr_in data_addr;
								bzero(&data_addr, sizeof(data_addr));
								data_addr.sin_family = AF_INET;
								data_addr.sin_port = htons(data_port);
								data_addr.sin_addr.s_addr = inet_addr(ip);

								// receive the command on control connection
								bzero(buffer, sizeof(buffer));
								recv(fd, buffer, sizeof(buffer), 0);
								printf("\n[%d]> %s\n", fd, buffer);

								// processing received command (LIST, RETR, STOR)
							    if (strncmp(buffer, "LIST", 4) == 0) {
									
									// here, we tried simply using system("ls > temp.txt") to redirect ls output to a file but it included the temp.txt as well
									// so we shifted to creating a pipe instead and redirecting the output of ls to the write end of the pipe

									// storing the output of fork+exec(ls) command in a pipe
									int pipefd[2];
									pipe(pipefd);
									int pid = fork();
									if (pid == 0) {
										close(pipefd[0]); // close the read end of the pipe
										dup2(pipefd[1], 1); // redirect stdout to the write end of the pipe
										close(pipefd[1]); // close the write end of the pipe
										execlp("ls", "ls", NULL); // execute the ls command
									}
									else {
										close(pipefd[1]); // close the write end of the pipe
										wait(NULL); // wait for the child process to finish

										// send acknowledgement to client
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "150 File status okay; about to open data connection.");
										send(fd, buffer, sizeof(buffer), 0);

										printf("Connecting to Client Transfer Socket\n");
										// connect to the client
										if (connect(data_sd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
											perror("connect");
											exit(-1);
										}
										printf("Connection Successful\n");

										// reading from the read end of the pipe and sending to the client
										bzero(buffer, sizeof(buffer)); // clear the buffer
										ssize_t bytes_read; // number of bytes read
										while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) { // while any bytes are read
											send(data_sd, buffer, bytes_read, 0);
											bzero(buffer, sizeof(buffer));
										}
										close(pipefd[0]); // close the read end of the pipe

										// close the data connection
										close(data_sd);

										// send acknowledgement to client
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "226 Transfer complete.");
										send(fd, buffer, sizeof(buffer), 0);

										printf("Transfer Complete\n");
									}
								}

								else if (strncmp(buffer, "RETR ", 5) == 0) {
									// open the file
									char *filename = buffer + 5;
									filename[strcspn(filename, "\n")] = 0; // remove trailing newline char from filename
									FILE *file = fopen(filename, "r");
									if (file == NULL) {
										perror("fopen");
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "550 No such file or directory.");
										send(fd, buffer, sizeof(buffer), 0);
									}
									else {
										// send acknowledgement to client
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "150 File status okay; about to open data connection.");
										send(fd, buffer, sizeof(buffer), 0);

										printf("Connecting to Client Transfer Socket\n");
										// connect to the client
										if (connect(data_sd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
											perror("connect");
											exit(-1);
										}
										printf("Connection Successful\n");

										// reading chunks and sending them to the client
										bzero(buffer, sizeof(buffer));
										size_t bytes_read; // number of bytes read, size_t type returned by fread, same as int?
										while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) { // while any bytes are read
											send(data_sd, buffer, bytes_read, 0);
											bzero(buffer, sizeof(buffer));
										}
										fclose(file);

										// close the data connection
										close(data_sd);

										// send acknowledgement to client
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "226 Transfer complete.");
										send(fd, buffer, sizeof(buffer), 0);

										printf("Transfer Complete\n");
									}

								}

								else if (strncmp(buffer, "STOR ", 5) == 0) {
									// creating file to write to
									char *filename = buffer + 5;
									filename[strcspn(filename, "\n")] = 0; // remove trailing newline char from filename
									FILE *file = fopen(filename, "w");
									if (file == NULL) {
										perror("fopen");
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "550 No such file or directory.");
										send(fd, buffer, sizeof(buffer), 0);
									}

									else {
										// send acknowledgement to client
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "150 File status okay; about to open data connection.");
										send(fd, buffer, sizeof(buffer), 0);

										printf("Connecting to Client Transfer Socket\n");
										// connect to the client
										if (connect(data_sd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
											perror("connect");
											exit(-1);
										}
										printf("Connection Successful\n");

										// receiving data from client and appending to file
										bzero(buffer, sizeof(buffer));
										size_t bytes_received; // number of bytes received
										while ((bytes_received = recv(data_sd, buffer, sizeof(buffer), 0)) > 0) { // while any bytes are received
											fwrite(buffer, 1, bytes_received, file);
											bzero(buffer, sizeof(buffer));
										}
										fclose(file);

										// close the data connection
										close(data_sd);										
										
										// send acknowledgement to client
										bzero(buffer, sizeof(buffer));
										strcpy(buffer, "226 Transfer complete.");
										send(fd, buffer, sizeof(buffer), 0);

										printf("Transfer Complete\n");
									}
								}
								// terminate the forked child process
								exit(0);
							}
							else {
								// wait for the child process to finish
								wait(NULL);
							}

						}
						else {
							bzero(buffer, sizeof(buffer));
							printf("Not logged in\n");
							strcpy(buffer, "530 Not logged in.");
							send(fd, buffer, sizeof(buffer), 0);
						}
					}

					else { // if the command is invalid
						bzero(buffer, sizeof(buffer));
						printf("Invalid command\n");
						strcpy(buffer, "202 Command not implemented.");
						send(fd, buffer, sizeof(buffer), 0);
					}
				}
			}
		}
	}

	//close
	close(server_sd);
	return 0;
}
