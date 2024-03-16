#include <stdio.h> // header for input and output from console : printf, perror
#include<string.h> // strcmp
#include<sys/socket.h> // for socket related functions
#include<arpa/inet.h> // htons
#include <netinet/in.h> // structures for addresses
#include<unistd.h> // contains fork() and unix standard functions
#include<stdlib.h>
#include<unistd.h> // header for unix specic functions declarations : fork(), getpid(), getppid()
#include<stdlib.h> // header for general fcuntions declarations: exit()


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

	//connect
    if(connect(server_sd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0) {
        perror("connect");
        exit(-1);
    }
	
	//accept
	char buffer[256];
	printf("Connected to server\n");

	while(1)
	{
		printf("Enter a message: ");
		fgets(buffer,sizeof(buffer),stdin); //get input from user
		buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it

		if(strcmp(buffer,"BYE!")==0) { //if user types BYE! then close the connection
			// printf("closing the connection to server \n");
			int end = send(server_sd,buffer,strlen(buffer),0);
			close(server_sd);
			break;
		}
		if(send(server_sd,buffer,strlen(buffer),0)<0) { //send message to server
			perror("send");
			exit(-1);
		}
		bzero(buffer,sizeof(buffer)); //clear the buffer
		int bytes = recv(server_sd,buffer,sizeof(buffer),0); //receive message from server
		printf("Server response: %s\n",buffer);
	}

	close(server_sd);
	return 0;
}
