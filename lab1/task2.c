#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#define TEMP_FAILURE_RETRY(expression) \
    ({ long int _result; \
       do _result = (long int)(expression); \
       while (_result == -1L && errno == EINTR); \
       _result; })

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MSG_LEN 16


void child_work(int write_fd, int read_fd, int m);
void parent_work(int n, int m, int pipes[][2]);
void create_children(int n, int pipes[][2], int m);
void readArguments(int argc, char **argv, int *n, int *m);


int main (int argc, char **argv)
{
    int n, m;
    readArguments(argc, argv, &n, &m);
    printf("n: %d, m: %d\n", n, m);
    int pipes[2*n][2];
    for(int i = 0; i < 2*n; i++)
    {
        if(pipe(pipes[i]))
            ERR("pipe");
    }

    

    create_children(n, pipes, m);
    parent_work(n, m, pipes);
    return 0;
}

void readArguments(int argc, char **argv, int *n, int *m)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s n m\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    *n = atoi(argv[1]);
    *m = atoi(argv[2]);
    if (*n < 2 || *n > 5 || *m < 5 || *m > 10)
    {
        fprintf(stderr, "invalid n, m\n");
        exit(EXIT_FAILURE);
    }
}

void create_children(int n, int pipes[][2], int m)
{
    for(int i = 0; i < n; i++)
    {
        switch(fork())
        {
            case 0:
            {   //close unsued pipes
               for(int j = 0; j < 2*n; j++)
               {
                    if(j != 2*i && j != 2*i+1)
                    {
                        if(TEMP_FAILURE_RETRY(close(pipes[j][1])))
                            ERR("close");
                        
                        if(TEMP_FAILURE_RETRY(close(pipes[j][0])))
                            ERR("close");
                    }
               }
                child_work(pipes[2*i][1], pipes[2*i+1][0], m);
                exit(EXIT_SUCCESS);
            }
            case -1:
                ERR("fork");
        }
    }
}

void child_work(int write_fd, int read_fd, int m)
{
    char buf[10];
    int number;
    srand(getpid());
    for(int i = 0; i < m; i++)
    {
        if (TEMP_FAILURE_RETRY(read(read_fd, &buf, sizeof(buf)) != sizeof(buf)))
        {
            if (errno == EPIPE)
            {
                if(TEMP_FAILURE_RETRY(close(read_fd)))
                    ERR("close");
                if(TEMP_FAILURE_RETRY(close(write_fd)))
                    ERR("close");
                exit(EXIT_SUCCESS);
            }
            else
            {
                ERR("read");
            }
        }

        if(strcmp(buf, "new_round") != 0)
        {
            ERR("wrong message");
        }
        int amIDead = rand()%100;
        if(amIDead < 5)
        {
            number = -1;
        }
        else
        {
            number = rand() % m + 1;
        }
        if (TEMP_FAILURE_RETRY(write(write_fd, &number, sizeof(int)) != sizeof(int)))
        {
            if (errno == EPIPE)
            {
                if(TEMP_FAILURE_RETRY(close(write_fd)))
                    ERR("close");
                if(TEMP_FAILURE_RETRY(close(read_fd)))
                    ERR("close");
                exit(EXIT_SUCCESS);
            }
            else
            {
                ERR("write");
            }
        }
        if(number == -1)
        {
            if(TEMP_FAILURE_RETRY(close(write_fd)))
                ERR("close");
            if(TEMP_FAILURE_RETRY(close(read_fd)))
                ERR("close");
            exit(EXIT_SUCCESS);
        }
    }
    
}

void parent_work(int n, int m, int pipes[][2])
{
    char message[10] = "new_round";
    int total[n];
    int scores[n];
    int alive[n];
    for (int i = 0; i < n; ++i) {
        alive[i] = 1; // 1 - alive, 0 - dead
        scores[i] = 0;
        total[i] = 0;
    }

    while(m)
    {
        printf("new round: %d\n", m);
        for(int i = 0; i < n; i++)
        {
            if(alive[i] == 0)
            {
                continue;
            }

            if (TEMP_FAILURE_RETRY(write(pipes[2*i+1][1], &message, sizeof(message)) != sizeof(message)))
            {
                if (errno == EPIPE)
                {
                    if(TEMP_FAILURE_RETRY(close(pipes[2*i+1][1])))
                        ERR("close");
                    if(TEMP_FAILURE_RETRY(close(pipes[2*i][0])))
                        ERR("close");
                    printf("MAIN: Player %d is dead\n", i);
                    alive[i] = 0;
                }
                else
                {
                    ERR("write");
                }
            }

            if(alive[i] == 0)
            {
                continue;
            }

            int number;
            if (TEMP_FAILURE_RETRY(read(pipes[2*i][0], &number, sizeof(int))) != sizeof(int))
            {
                if (errno == EPIPE)
                {
                    if(TEMP_FAILURE_RETRY(close(pipes[2*i][1])))
                        ERR("close");
                    if(TEMP_FAILURE_RETRY(close(pipes[2*i+1][0])))
                        ERR("close");
                    continue;
                }
                else
                {
                    ERR("read");
                }
            }
            if(number == -1)
            {
                if(TEMP_FAILURE_RETRY(close(pipes[2*i][1])))
                    ERR("close");
                if(TEMP_FAILURE_RETRY(close(pipes[2*i+1][0])))
                    ERR("close");
                printf("Player %d is dead\n", i);
                alive[i] = 0;
                continue;
            }
            scores[i] = number;
            printf("Got number: %d, from player: %d \n", number, i);
        }


        int max_number = 0;
        for (int j = 0; j < n; j++) 
        {
            if (scores[j] > max_number) 
            {
                max_number = scores[j];
            }
        }


        int num_winners = 0;
        for (int j = 0; j < n; j++)
        {
            if (scores[j] == max_number) 
            {
                num_winners++;
            }
        }


        for (int j = 0; j < n; j++)
        {
            if (scores[j] == max_number) 
            {
                total[j] += (int)floor(n / num_winners);
            }
        }

        printf("\n");
        m--;
    }

    printf("Scoreboard:\n");
    for (int i = 0; i < n; i++)
    {
        printf("Player %d: %d points\n", i, total[i]);
    }
    
}
