#include <stdio.h> // header for input and output from console : printf, perror
#include<string.h> // strcmp
#include<sys/socket.h> // for socket related functions
#include<arpa/inet.h> // htons
#include <netinet/in.h> // structures for addresses
#include<unistd.h> // contains fork() and unix standard functions
#include<stdlib.h>
#include<unistd.h> // header for unix specic functions declarations : fork(), getpid(), getppid()
#include<stdlib.h> // header for general fcuntions declarations: exit()
#include<stdint.h> // for uint8_t

// PORT function with args as ip, data_port, and fields array to be filled
void PORT(char *ip, int data_port, char *fields) {

	int h1, h2, h3, h4, p1, p2;
	sscanf(ip, "%d.%d.%d.%d", &h1, &h2, &h3, &h4); // parsing the ip address

	// calculating p1 and p2 from the port number and casting to char
	p1 = data_port / 256;
	p2 = data_port % 256;

	// storing ints as characters in fields, comma separated
	sprintf(fields, "%d,%d,%d,%d,%d,%d", h1, h2, h3, h4, p1, p2);
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
	server_addr.sin_port = htons(9021); // specified port
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // loopback address

	//connect
    if(connect(server_sd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0) {
        perror("connect");
        exit(-1);
    }

	// storing the port with which client connects to server
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	getsockname(server_sd, (struct sockaddr *) &client_addr, &client_len);
	int data_port = ntohs(client_addr.sin_port) + 1; // initial data port = control port + 1 (N+1)
	
	char buffer[256];
	printf("Connected to server\n");

	while(1)
	{
		// changing cwd to client folder
		chdir("../client");

		printf("ftp> ");
		fgets(buffer,sizeof(buffer),stdin); //get input from user
		buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it

		// if command is "USER", but login is 1, set login to 0
		if (strncmp(buffer, "USER ", 5) == 0 ) {
			send(server_sd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer)); //clear the buffer
			recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
			printf("%s\n",buffer);
		}

		else if (strncmp(buffer, "PASS ", 5) == 0) {
			send(server_sd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer)); //clear the buffer
			recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
			printf("%s\n",buffer);
		}

		else if (strcmp(buffer, "QUIT") == 0) { //if user types QUIT then close the connection
			send(server_sd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer)); //clear the buffer
			recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
			printf("%s\n",buffer);
			close(server_sd);
			break;
		}

		// !LIST, !PWD, and !CWD locally through system() command
		else if (strcmp(buffer, "!LIST") == 0) {
			system("ls");
		}

		else if (strcmp(buffer, "!PWD") == 0) {
			system("pwd");
		}

		else if (strncmp(buffer, "!CWD ", 5) == 0) {
			char *dir = buffer + 5;
			dir[strcspn(dir, "\n")] = 0; // remove trailing newline char from dir
			char command[256];
			if (chdir(dir) == 0) { // cannot use system() command because it will change the working directory of the spawned shell, not the program
				printf("Directory changed to ");
				printf("%s\n", getcwd(command, sizeof(command))); // not using system() as it cannot recognize the change in directory of the program
			}
			else {
				printf("No such file or directory.\n");
			}					
		}

		else if (strcmp(buffer, "PWD") == 0) {
			send(server_sd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer)); //clear the buffer
			recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
			printf("%s\n",buffer);
		}

		else if (strncmp(buffer, "CWD ", 4) == 0) {
			send(server_sd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer)); //clear the buffer
			recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
			printf("%s\n",buffer);
		}

		else if (strcmp(buffer, "LIST") == 0 || strncmp(buffer, "STOR ", 5) == 0 || strncmp(buffer, "RETR ", 5) == 0) {
			// storing initial command in new buffer
			char initial_command[256];
			bzero(initial_command, sizeof(initial_command));
			strcpy(initial_command, buffer);

			// calling PORT
			char fields[64];
			bzero(fields, sizeof(fields));
			PORT("127.0.0.1", data_port, fields);
			
			// sending "PORT h1,h2,h3,h4,p1,p2" command to server
			char port_command[256];
			bzero(port_command, sizeof(port_command));
			sprintf(port_command, "PORT %s", fields);
			send(server_sd, port_command, strlen(port_command), 0);
			bzero(buffer, sizeof(buffer));
			recv(server_sd, buffer, sizeof(buffer), 0);
			printf("%s\n", buffer);

			// creating new socket for data transfer
			int data_sd = socket(AF_INET, SOCK_STREAM, 0);
			if (data_sd < 0) {
				perror("socket:");
				exit(-1);
			}

			// setting up the data connection
			struct sockaddr_in data_addr;
			bzero(&data_addr, sizeof(data_addr));
			data_addr.sin_family = AF_INET;
			data_addr.sin_port = htons(data_port);
			data_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

			data_port++; // incrementing data port for next data transfer

			// bind
			if (bind(data_sd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
				perror("bind:");
				exit(-1);
			}

			// // print port we are listening on, verification
			// printf("Listening on port %d\n", ntohs(data_addr.sin_port));

			// listen
			if (listen(data_sd, 5) < 0) {
				perror("listen:");
				exit(-1);
			}

			// sending initial command to server
			send(server_sd, initial_command, strlen(initial_command), 0);
			bzero(buffer, sizeof(buffer));
			recv(server_sd, buffer, sizeof(buffer), 0); // receiving 150 File okay here
			printf("%s\n", buffer);

			// if 550 file not found error, close data socket and continue
			if (strncmp(buffer, "550", 3) == 0) {
				close(data_sd);
				continue;
			}

			// accept
			struct sockaddr_in client_data_addr;
			socklen_t client_data_len = sizeof(client_data_addr);
			int client_data_sd = accept(data_sd, (struct sockaddr *)&client_data_addr, &client_data_len);
			if (client_data_sd < 0) {
				perror("accept:");
				exit(-1);
			}

			// if initial command is LIST or RETR, receiving data from server
			if (strncmp(initial_command, "LIST", 4) == 0) {

			}

			else if (strncmp(initial_command, "RETR", 4) == 0) {
				
		
			}

			else if (strncmp(initial_command, "STOR", 4) == 0) {

			}

			// closing the data socket
			close(client_data_sd);
			close(data_sd);

		}

		// if command is anything else, send anyway to receive error
		else {
			send(server_sd,buffer,strlen(buffer),0);
			bzero(buffer,sizeof(buffer)); //clear the buffer
			recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
			printf("%s\n",buffer);
		}
	}

	close(server_sd);
	return 0;
}
