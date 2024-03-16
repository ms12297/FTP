CC=gcc

all: code/server code/client

server: server.c
	$(CC) -o server server.c

client: client.c
	$(CC) -o client client.c

clean:
	rm -f code/server code/client
