#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

static int PORT;
static int NUM_THREADS;
static int RUN_TIME;
static int SLEEP_TIME;
static int NUM_FILES = 5000;
static struct hostent *server;
static char* MODE;
static char* FIXED_FILE = "files/foo0.txt";
static int* requests;
static double* response_times;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

// every client runs this function
// creates socket, requests file, sleeps and repeats
// till RUN_TIME amount of seconds
// `index` parameter is for giving different seeds to different
// threads for the random file requesting
void getFile(int index)
{
    int sockfd, yes = 1;
    char buffer[256];
    struct sockaddr_in serv_addr;

    time_t init = time(NULL);
    int seed = index * time(NULL);

    // while execution time < RUN_TIME
    while (difftime(time(NULL), init) < RUN_TIME)
    {
        /* create socket, get sockfd handle */
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        //setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (sockfd < 0) 
        {
            perror("ERROR opening socket");
            continue;
        }

        bzero((char*)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;

        bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(PORT);

        /* connect to server */
        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
            error("ERROR connecting");
        printf("Client thread %d connected to the server\n", index);

        /* request created based on MODE */
        if(strcmp(MODE,"fixed") == 0)
            sprintf(buffer, "get %s", FIXED_FILE);
        else
        {
            int file_num = rand_r(&seed) % NUM_FILES;
            sprintf(buffer, "get files/foo%d.txt", file_num);
        }

        /* send user message to server */
        int bytes_sent = write(sockfd, buffer, strlen(buffer));
        if (bytes_sent < 0) 
        {
            perror("ERROR writing to socket");
            close(sockfd);
            continue;
        }
        bzero(buffer, 256);

        struct timeval start, end;
        gettimeofday(&start, NULL);

        // a check to ensure if the server actually sent the data or not
        int received = 0;
        while(1)
        {
            int bytes_recv = recv(sockfd, buffer, sizeof(buffer), 0);
            
            if (bytes_recv < 0)
            {
                perror("ERROR reading from socket");
                break;
            }
            else if (bytes_recv == 0)
            {
                if(received)
                {
                    gettimeofday(&end, NULL);
                    printf("File received by client %d\n", index);

                    // response time calculation 
                    requests[index]++;
                    response_times[index] += (double)(end.tv_usec - start.tv_usec)/1e6 + 
                                         (double)(end.tv_sec - start.tv_sec);
                }
                else
                {
                    printf("File requested by client %d not found on server\n", index);
                }
                break;
            }
            else
            {
                received = 1;
            }
        }
        // close the socket
        close(sockfd);
        // sleep for designated time
        sleep(SLEEP_TIME);
    }
}

int main(int argc, char *argv[])
{
    int i;
    if (argc != 7) {
       fprintf(stderr, "usage :  %s [host] [port] [num-threads] [run time] [sleep time] [mode]\n", argv[0]);
       exit(0);
    }

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    PORT = atoi(argv[2]);
    NUM_THREADS = atoi(argv[3]);
    RUN_TIME = atoi(argv[4]);
    SLEEP_TIME = atoi(argv[5]);
    MODE = (char*)malloc(strlen(argv[6]));
    strncpy(MODE, argv[6], strlen(argv[6]));

    // calloc initialises all bits to 0
    requests = calloc(NUM_THREADS, sizeof(int));
    response_times = calloc(NUM_THREADS, sizeof(double));

    // server    
    server = gethostbyname(argv[1]);
    if (server == NULL)
        error("ERROR no such host");

    pthread_t *tid = malloc(NUM_THREADS * sizeof(pthread_t));

    // create NUM_THREADS threads
    for(i = 0; i < NUM_THREADS; i++) 
        // second NULL is for giving arguments to getFile
        pthread_create(&tid[i], NULL, getFile, i);

    // wait for all NUM_THREADS threads to finish
    for(i = 0; i < NUM_THREADS; i++) 
        //change NULL for getting back return values
        pthread_join(tid[i], NULL);

    gettimeofday(&end_time, NULL);

    // throughput and response time calculations
    double total_time = (double)(end_time.tv_usec - start_time.tv_usec)/1e6 + 
                        (double)(end_time.tv_sec - start_time.tv_sec);

    int total_requests = 0;
    double sum_response_time = 0.0;

    for(i = 0; i < NUM_THREADS; i++)
    {
        total_requests += requests[i];
        sum_response_time += response_times[i];
    }

    printf("Throughput = %f req/s\n", total_requests/total_time);
    printf("Average Response Time = %f sec\n", sum_response_time/total_requests);
        
    // memory deallocation
    free(tid);
    free(requests);
    free(response_times);
    
    return 0;
}
