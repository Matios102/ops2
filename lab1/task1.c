#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEMP_FAILURE_RETRY(expression) \
    ({ long int _result; \
       do _result = (long int)(expression); \
       while (_result == -1L && errno == EINTR); \
       _result; })

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t terminate = 0;

void work(int read_fd, int write_fd);
void send(int write_fd, int number);
int receive(int read_fd);
void create_children_and_pipes(int pipes[3][2]);

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sigchld_handler(int sig)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (0 == pid)
            return;
        if (0 >= pid)
        {
            if (ECHILD == errno)
                return;
            ERR("waitpid:");
        }
    }
}

void sigint_handler(int sig)
{
    terminate = 1;
}

void create_children_and_pipes(int pipes[3][2])
{
    for(int i = 0; i < 2; i++)
    {
        switch(fork())
        {
            case 0:
                if(TEMP_FAILURE_RETRY(close(pipes[i][1])))
                    ERR("close");
                if(TEMP_FAILURE_RETRY(close(pipes[i+1][0])))
                    ERR("close");

                work(pipes[i][0], pipes[i+1][1]);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
        }
    }
}

void work(int read_fd, int write_fd)
{
    srand(getpid());
    int number;
    while ((number = receive(read_fd)) != 0)
    {
        if (number == 0 || terminate)
            break;

        number += -10 + rand() % 21;
        if (TEMP_FAILURE_RETRY(write(write_fd, &number, sizeof(int))) != sizeof(int)) {
            if (errno == EPIPE) {
                // Broken pipe, exit the child process
                break;
            } else {
                // Other write error
                ERR("write");
            }
        }
    }
    
    // Check for end-of-file (EOF) on reading from pipe
    if (number == 0 || errno == EPIPE) {
        // Close the reading end of the pipe before exiting
        if (TEMP_FAILURE_RETRY(close(read_fd)))
            ERR("close");
        exit(EXIT_SUCCESS);
    }
    else {
        // Handle other errors
        ERR("read");
    }
}


void send(int write_fd, int number)
{
    if (TEMP_FAILURE_RETRY(write(write_fd, &number, sizeof(int))) != sizeof(int))
        ERR("write");
}


int receive(int read_fd)
{
    int receive;
    if (TEMP_FAILURE_RETRY(read(read_fd, &receive, sizeof(int))) != sizeof(int))
        ERR("read");
    printf("PID: %d, Received: %d\n", getpid(), receive);
    return receive;
}

int main(int argc, char const *argv[])
{
    int pipes[3][2];
    for(int i = 0; i < 3; i++)
    {
        if(pipe(pipes[i]))
            ERR("pipe");
    }
    if(sethandler(sigchld_handler, SIGCHLD) || sethandler(sigint_handler, SIGINT))
        ERR("Setting signal handler:");

    create_children_and_pipes(pipes);
    if(TEMP_FAILURE_RETRY(close(pipes[0][0])))
        ERR("close");

    send(pipes[2][1], 1);
    work(pipes[2][0], pipes[0][1]);

    if(TEMP_FAILURE_RETRY(close(pipes[0][1])))
        ERR("close");

    return 0;
}
