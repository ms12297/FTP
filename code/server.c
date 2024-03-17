#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include <sys/wait.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>


int active_users = 0; // number of active users

// create a struct called user with fields username, password, wd (working directory)
struct user {
	char username[256];
	char password[256];
	char wd[256];
};

// create an array of users
struct user users[10];

// function to authenticate the user
int user_auth(int client_socket, char username[256]) {
	// reading csv and storing user
	// then verifying auth, user first then password
	// print username
	// printf("Username: %s\n", username);
	return 1; // return 0 for error
}


int main()
{
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
	server_addr.sin_port = htons(6000); // specified port
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

	// client address
	struct sockaddr_in cliaddr;
    bzero(&cliaddr,sizeof(cliaddr));
    unsigned int len = sizeof(cliaddr);

	while(1) {
		//accept
		int client_sd = accept(server_sd,(struct sockaddr *) &cliaddr, &len);	

		// print connection details
		printf("Connection established with user %d\n", client_sd);
		printf("Their port: %d\n", ntohs(cliaddr.sin_port));
		
		int pid = fork(); //fork a child process
		if(pid == 0) { //if it is the child process
		 	char buffer[256];
			int userIdx = -1; // initializing
			while(1)
			{
				bzero(buffer,sizeof(buffer)); //clear the buffer
				int bytes = recv(client_sd,buffer,sizeof(buffer),0); //receive message from client
				if (bytes == 0) {
					printf("\nConnection [%d] closed\n", client_sd);
					close(client_sd);
					break;
				}
				printf("\n[%d]> %s\n", client_sd, buffer);

				// if the command is "USER <username>", authenticate the user
				if (strncmp(buffer, "USER ", 5) == 0) {
					char *username = buffer + 5;
					username[strcspn(username, "\n")] = 0; // remove trailing newline char from username
					if (user_auth(client_sd, username) == 1) {
						userIdx = active_users - 1;
						// setting cwd as this user's working directory
						chdir(users[userIdx].wd);
						// printf("LOGGED IN\n");
						// printf("Working directory: %s\n", users[userIdx].wd);
						strcpy(buffer, "230 User logged in, proceed.");
						send(client_sd, buffer, sizeof(buffer), 0);
					}
				}

				// if the command is "BYE!", close the connection
				else if(strcmp(buffer, "QUIT") == 0) {
					printf("Connection [%d] closed\n", client_sd);
					bzero(buffer,sizeof(buffer)); //clear the buffer
					strcpy(buffer, "221 Service closing control connection.");
					send(client_sd,buffer,sizeof(buffer),0); //send message to client
					close(client_sd);
					break;
				}

				else { // if the command is invalid
					bzero(buffer, sizeof(buffer));
					printf("Invalid command\n");
					strcpy(buffer, "202 Command not implemented.");
					send(client_sd, buffer, sizeof(buffer), 0);
				}
				
			}
		}
		// nothing else to be done in the parent process so loop back to accept another client
	}

	//close
	close(server_sd);
	return 0;
}
