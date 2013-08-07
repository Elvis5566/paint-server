#include<stdio.h>
#include<string.h>  //strlen
#include<stdlib.h>  //strlen
#include<sys/socket.h>
#include<arpa/inet.h>   //inet_addr
#include<unistd.h>  //write
#include<pthread.h> //for threading , link with lpthread
#include <syslog.h>


const int SIZE_BUF = 2000;
const int DEFAULT_PORT = 5566;
struct Client
{
    int socket;
    char ip[INET_ADDRSTRLEN];
    struct Client *next;
    // struct Client 
};

//the thread function
void *connection_handler(void *);
int sendAll(int s, char *buf, int *len);


struct Client* clientList;
int clientNum = 0;
int main(int argc , char *argv[])
{
    int socket_desc , client_sock , c;
    struct sockaddr_in server , client;
    char IPdotdec[INET_ADDRSTRLEN];

    openlog("PaintServer",LOG_CONS | LOG_PID, LOG_USER);
    
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        syslog(LOG_ERR, "Could not create socket");
    }
    syslog(LOG_INFO, "Socket created");

    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    syslog(LOG_INFO, "port: %d\n", port);
    printf("port: %d\n", port);
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    

    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        syslog(LOG_ERR, "bind failed. Error");
        return 1;
    }
    syslog(LOG_INFO, "bind done");
    
    //Listen
    listen(socket_desc , 2);
    
    //Accept and incoming connection
    syslog(LOG_INFO, "Waiting for incoming connections...");
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    


    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
    {

        syslog(LOG_INFO, "Connection accepted");
        
        inet_ntop(AF_INET, &client.sin_addr, IPdotdec, INET_ADDRSTRLEN);

        syslog(LOG_INFO, "client IP: %s\n", IPdotdec);

        struct Client *newClient = malloc(sizeof(struct Client));
        newClient->next = clientList;
        strcpy(newClient->ip, IPdotdec);
        newClient->socket = client_sock;
        clientList = newClient;
        ++clientNum;

        pthread_t sniffer_thread;
        
        if(pthread_create(&sniffer_thread , NULL ,  connection_handler , (void*) newClient) < 0)
        {
            syslog(LOG_ERR, "could not create thread");
            return 1;
        }
        
        syslog(LOG_INFO, "Handler assigned");
    }
    
    if (client_sock < 0)
    {
        syslog(LOG_ERR, "accept failed");
        return 1;
    }

    closelog();
    return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *client_desc)
{
    //Get the socket descriptor
    struct Client *client = (struct Client*) client_desc; 
    int sock = client->socket;
    int read_size;
    char client_message[SIZE_BUF];
    
    syslog(LOG_INFO, "client %s connected.\n", client->ip);
    syslog(LOG_INFO, "number of clients: %d\n", clientNum);

    printf("client %s connected.\n", client->ip);

    //Receive a message from client
    while((read_size = recv(sock , client_message , SIZE_BUF , 0)) > 0)
    {
        syslog(LOG_INFO, "recv from: %s length: %d\n", client->ip, read_size);

        if (read_size < SIZE_BUF) {
            struct Client *ptr = clientList;

            while (ptr) {         
                if (ptr->socket != sock) {
                    sendAll(ptr->socket, client_message, &read_size);
                }
                ptr = ptr->next;
            }

        }
    }
    
    if(read_size == 0)
    {
        syslog(LOG_INFO, "Client %s disconnected.\n", client->ip);
        printf("Client %s disconnected.\n", client->ip);
    }
    else if(read_size == -1)
    {
        syslog(LOG_ERR, "recv failed");
    }
    

    // free client
    if (clientList == client)
    {
        clientList = clientList->next;
    }

    struct Client *ptr = clientList, *pre = clientList;

    while (ptr) {
        if (ptr == client) {
            pre->next = client->next;
            
            break;
        }

        pre = ptr;
        ptr = ptr->next;
    }
    --clientNum;

    free(client_desc);

    syslog(LOG_INFO, "number of clients: %d\n", clientNum);
    return 0;
}

int sendAll(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while (total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
} 