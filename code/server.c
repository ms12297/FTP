#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include <sys/wait.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include<unistd.h>
#include<stdlib.h>

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
		printf("[%s:%d] Connected\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));	
		
		int pid = fork(); //fork a child process
		if(pid == 0) { //if it is the child process
		 	char buffer[256];
			while(1)
			{
				bzero(buffer,sizeof(buffer)); //clear the buffer
				int bytes = recv(client_sd,buffer,sizeof(buffer),0); //receive message from client
				if (bytes == 0) {
					printf("\nConnection [%d] closed\n", client_sd);
					close(client_sd);
					break;
				}

				// printing the message received from and to be sent to client
				printf("[%s:%d] Received message: %s\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port),buffer);

				if (strcmp(buffer,"BYE!")==0) { //if user types BYE! then close the connection
					printf("[%s:%d] Disconnected\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
					close(client_sd);
					exit(0); // terminate this client's process
				}

				printf("[%s:%d] Sending message: %s\n", inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port),buffer);
				int count = send(client_sd,buffer,sizeof(buffer),0);
			}
		}
		// nothing else to be done in the parent process so loop back to accept another client
	}

	//close
	close(server_sd);
	return 0;
}
