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
	int logIn;
};

// create an array of users
struct user users[10];

// function to authenticate the user
int user_auth(int client_socket, char username[256]) {
	// open the file ../users.csv
	FILE *file = fopen("../users.csv", "r");
	if (file == NULL) {
		perror("fopen");
		exit(-1);
	}
	// read the file line by line
	char line[256];
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
	bzero(buffer, sizeof(buffer));
	strcpy(buffer, "530 Not logged in.");
	send(client_socket, buffer, sizeof(buffer), 0);

	fclose(file);
	return 0;
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
