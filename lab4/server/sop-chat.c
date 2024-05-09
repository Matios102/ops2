#include "l4-common.h"

#define BACKLOG_SIZE 10
#define MAX_CLIENT_COUNT 4
#define MAX_EVENTS 10

#define NAME_OFFSET 0
#define NAME_SIZE 64
#define MESSAGE_OFFSET NAME_SIZE
#define MESSAGE_SIZE 448
#define BUFF_SIZE (NAME_SIZE + MESSAGE_SIZE)

void usage(char *program_name) {
    fprintf(stderr, "USAGE: %s port key\n", program_name);
    exit(EXIT_FAILURE);
}
volatile sig_atomic_t do_work = 1;
void sigint_handler(int sig) {do_work = 0;}



void doServer(int tcp_socket, char* key)
{
    char buf[BUFF_SIZE];
    int clients_fd[MAX_CLIENT_COUNT];

    for(int i =0; i < MAX_CLIENT_COUNT; i++)
        clients_fd[i] = 0;

    int client_count = 0;
    int epoll_descriptor;
    if((epoll_descriptor = epoll_create1(0)) < 0)
        ERR("epoll_create1");

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = tcp_socket;

    if(epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_socket, &event) == -1)
        ERR("epoll_ctl");

    int nfds;
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while(do_work)
    {
        printf("epoll do be waiting\n");
        if((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {
            printf("epoll aint waiting nomo %d\n", nfds);
            for(int i = 0; i < nfds;i++)
            {
                if(events[i].data.fd == tcp_socket) //trying to add new client
                {
                    if(client_count < MAX_CLIENT_COUNT) //adding new client
                    {
                        struct sockaddr client_address;
                        socklen_t client_addrlen = sizeof(client_address);
                        int accepted_client = accept(tcp_socket, &client_address, &client_addrlen);
                        if(accepted_client < 0)
                        {
                            ERR("accept");
                        }

                        if(read(accepted_client, buf, BUFF_SIZE) != BUFF_SIZE)
                            ERR("read");
                    
                        char* client_name = buf + NAME_OFFSET;
                        char* client_key = buf + MESSAGE_OFFSET;



                        if(strcmp(key, client_key) != 0)
                        {
                            printf("Authorization failed\n");
                        }
                        else
                        {
                            printf("Client name: %s, client key: %s\n", client_name, client_key);

                            if(write(accepted_client, buf, BUFF_SIZE) != BUFF_SIZE)
                                ERR("write");
                            
                            for(int j = 0; j < MAX_CLIENT_COUNT; j++)
                            {
                                if(clients_fd[j] == 0)
                                {
                                    clients_fd[j] = accepted_client;
                                    client_count++;
                                    break;
                                }
                            }
                            event.data.fd = accepted_client;
                            if(epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, accepted_client, &event) == -1)
                                ERR("epoll_ctl");
                        }

                    }
                    else
                    {
                        printf("dropping that ho, she ugly\n");
                        struct sockaddr client_address;
                        socklen_t client_addrlen = sizeof(client_address);
                        int accepted_client = accept(tcp_socket, &client_address, &client_addrlen);
                        if(accepted_client < 0)
                        {
                            ERR("accept");
                        }
                        if(TEMP_FAILURE_RETRY(close(accepted_client)) < 0)
                            ERR("close");
                    }
                }
                else //reading from already estavlished client
                {
                    ssize_t size;
                    printf("READING FROM CLIENT\n");
                    if((size = read(events[i].data.fd, buf, BUFF_SIZE)) != BUFF_SIZE)
                    {
                        if(size == 0 || errno == ECONNRESET)
                        {
                            printf("DROPPING\n");
                            for(int j =0; j < MAX_CLIENT_COUNT; j++)
                            {
                                if(clients_fd[j] == events[i].data.fd)
                                {
                                    clients_fd[j] = 0;
                                    client_count--;
                                    break;
                                }
                            }
                            if(epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1)
                                ERR("epoll_ctl");

                            if(TEMP_FAILURE_RETRY(close(events[i].data.fd)) < 0)
                                ERR("close");
                            break;
                        }
                    }

                    
                    char* client_name = buf + NAME_OFFSET;
                    char* client_message = buf + MESSAGE_OFFSET;

                    printf("Client %s: %s\n", client_name, client_message);

                    for(int j = 0; j < client_count; j++)
                    {
                        if(clients_fd[j] == events[i].data.fd)
                            continue;

                        if(write(clients_fd[j], buf, BUFF_SIZE) != BUFF_SIZE)
                            ERR("write");
                    }

                }
            }
        }
    }

    for(int i = 0; i < client_count; i++)
    {
        if(TEMP_FAILURE_RETRY(close(clients_fd[i])) < 0)
            ERR("close");
    }
}
int main(int argc, char **argv) {
    char *program_name = argv[0];
    int tcp_socket;
    if (argc != 3) {
        usage(program_name);

    }

    uint16_t port = atoi(argv[1]);
    if (port == 0){
        usage(argv[0]);
    }

    char *key = argv[2];

    if(sethandler(SIG_IGN,SIGPIPE))
        ERR("Setting SIGPIPE");

    if(sethandler(sigint_handler, SIGINT))
        ERR("sethandler");


    tcp_socket = bind_tcp_socket(port, BACKLOG_SIZE);
    doServer(tcp_socket, key);

    if(TEMP_FAILURE_RETRY(close(tcp_socket)) < 0)
        ERR("close");

    printf("Server has terminated\n");
    return EXIT_SUCCESS;
}
