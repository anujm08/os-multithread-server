#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <queue>
#include <string.h>
#include <unistd.h>

using namespace std;

int BUFFER_SIZE = 1024;
queue<int> requests;

pthread_mutex_t lock;
pthread_cond_t queueHasSpace;
pthread_cond_t queueHasRequests;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void serveFile(int sock)
{
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE); 
    
    /* Read file requesst from client */
    int bytes_read = read(sock, buffer, sizeof(buffer));
    if (bytes_read < 0)
    {
        perror("ERROR reading from socket");
        return;
    }
    /* extract filename */
    char* filename = (char*)malloc(strlen(buffer) - 3);
    strncpy(filename, buffer + 4, strlen(buffer) - 3);

    /* Open the requested file */
    FILE *fp = fopen(filename, "rb");
    if(fp == NULL)  // handle this in client
    {
        perror("ERROR file not found");
        return;
    }
    
    /* Send requested file */
    printf("Sending file %s to client\n", filename);

    // TODO check error handling, program should not terminate on error in reading/writing
    while(1)
    {
        int bytes_read = fread(buffer, sizeof(char), sizeof(buffer), fp);
        if(bytes_read > 0)
        {
            int bytes_sent = send(sock, buffer, bytes_read, 0);
            if (bytes_sent < bytes_read) 
            {
                printf("Sockfd writing error on %d\n", sock);
                perror("ERROR writing to socket");
                break;
            }
        }
        if(bytes_read == 0)
        {
            printf("File %s successfully sent to client\n",filename);
            break;
        }
        if(bytes_read < 0)
        {
            perror("ERROR reading from file");
            break;
        }
        //sleep(1);
    }
    fclose(fp);
}

// function which server calls to serve the requsted file to the client
void* serverThread(void* arg)
{
    int sock;
    while (1)
    {
        pthread_mutex_lock(&lock);

        while (requests.size() == 0)
            pthread_cond_wait(&queueHasRequests, &lock);
        
        sock = requests.front();
        requests.pop();
        pthread_cond_signal(&queueHasSpace);
        pthread_mutex_unlock(&lock);
        
        serveFile(sock);
        
        close(sock);
    }
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, yes = 1;
    socklen_t clilen;
    int numthreads, maxmqueuesize;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 4 || argc > 4) {
        printf("usage :  %s [port] [num-threads] [queue-size]\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    numthreads = atoi(argv[2]);
    maxmqueuesize = atoi(argv[3]);

    //initialize lock
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        error("ERROR initialising mutex");
        return 1;
    }

    //initialize conditional variables
    if (pthread_cond_init(&queueHasSpace, NULL) != 0)
    {
        error("ERROR initialising conditional variable");
        return 1;
    }
    if (pthread_cond_init(&queueHasRequests, NULL) != 0)
    {
        error("ERROR initialising conditional variable");
        return 1;
    }


    // create threads
    pthread_t* tid = (pthread_t*) malloc(numthreads*sizeof(pthread_t));
    for(int i = 0; i < numthreads; i++)
        pthread_create(&tid[i], NULL, &serverThread, NULL);

    /* create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (sockfd <= 0) 
        error("ERROR opening socket");

    /* fill in port number to listen on. IP address can be anything (INADDR_ANY) */

    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /* bind socket to this port number on this machine */

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    
    /* listen for incoming connection requests */
    listen(sockfd, 2);
    clilen = sizeof(cli_addr);

	while (1)
    {
        pthread_mutex_lock(&lock);
        while(maxmqueuesize != 0 && requests.size() >= maxmqueuesize)
            pthread_cond_wait(&queueHasSpace, &lock);
        pthread_mutex_unlock(&lock);
        
        /* accept a new request, create a newsockfd */
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
        
        if (newsockfd < 0) 
        {
            perror("ERROR on accept");
            continue;
        }
        printf("New client connected\n");

        pthread_mutex_lock(&lock);
        requests.push(newsockfd);

        pthread_cond_signal(&queueHasRequests);
        pthread_mutex_unlock(&lock);
	}

    // join the threads
    for(int i = 0; i < numthreads; i++)
        pthread_join(tid[i], NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&queueHasSpace);
    pthread_cond_destroy(&queueHasRequests);

    return 0; 
}
