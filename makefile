all: client server

client: multi-client.c
	gcc -w -o multi-client multi-client.c -lpthread

server: server-mt.c
	g++ -w -o server-mt server-mt.c -lpthread

clean: 
	rm multi-client
	rm server-mp