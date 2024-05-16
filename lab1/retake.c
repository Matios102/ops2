#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define w_fd 1
#define r_fd 0
#define BUFF_SIZE 100

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define UNUSED(x) (void)(x)
volatile sig_atomic_t do_work = 1;

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file\n", name);
    exit(EXIT_FAILURE);
}

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sigalarm_handler(int sig) {do_work = 0; UNUSED(sig);}

void sleep_ms(unsigned int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void wait_for_children(int n)
{
    int status;
    for (int i = 0; i < n; i++)
    {
        if (wait(&status) == -1)
        {
            ERR("Wait:");
        }
    }
}

void child_work(int read_fd, int write_fd)
{
    if (sethandler(sigalarm_handler, SIGALRM))
        ERR("Seting SIGALRM:");
    srand(getpid());
    int k = 3 + rand()%7;
    printf("Student: %d\n", (int)getpid());
    char buf[BUFF_SIZE];
    if (read(read_fd, buf, BUFF_SIZE) != BUFF_SIZE)
        ERR("read from individual pipe");
    char message[BUFF_SIZE];
    sprintf(message, "HERE!");
    printf("Student %d: %s\n", (int)getpid(), message);
    if (write(write_fd, message, BUFF_SIZE) < 0)
        ERR("write to shared pipe");

    int stage = 1;
    while(stage < 5)
    {
        int t = 100 + rand()%401;
        sleep_ms(t);
        int q = 1 + rand()%20;
        int result = k+q;
        sprintf(message, "%d %d", result, (int)getpid());
        if (write(write_fd, message, BUFF_SIZE) < 0)
            ERR("write to shared pipe");
        int status;
        if ((status = read(read_fd, buf, BUFF_SIZE)) != BUFF_SIZE)
        {
            if(status < 0)
                if(errno == EINTR)
                    break;

            if(status == 0)
                break;
            
            ERR("read from individual pipe");
        }
        
        int child_pid, stage_parent;
        char response[BUFF_SIZE];
        sscanf(buf, "Student %d %s stage %d", &child_pid, response, &stage_parent);

        if(!strcmp(response, "finished"))
        {
            stage++;
        }
    }
    if(stage != 5)
    {
        printf("Student %d: Oh no, I haven't finished stage %d. I need more time\n", (int)getpid(), stage);
    }
    else
        printf("Student %d: I NAILED IT!\n", (int)getpid());
}

void parent_work(int* write_fds, int read_fd, int n, int* child_pids)
{
    printf("Teacher: %d\n", (int)getpid());
    srand(getpid());
    char buf[BUFF_SIZE];
    char message[BUFF_SIZE];
    int stages[n];
    int scores[n];

    for(int i = 0; i < n; i++)
    {
        sprintf(message, "Is %i here?", child_pids[i]);
        printf("Teacher: %s\n", message);
        if (write(write_fds[i], message, BUFF_SIZE) < 0)
            ERR("write to individual student pipe");

        if (read(read_fd, buf, BUFF_SIZE) != BUFF_SIZE)
            ERR("read:");
        
        stages[i] = 1;
        scores[i] = 0;
    }

    int finished_counter = 4 * n;
    alarm(2);
    while(finished_counter && do_work)
    {
        if (read(read_fd, buf, BUFF_SIZE) != BUFF_SIZE)
        {
            if(errno == EINTR)
                break;
            
            ERR("read from shared pipe");
        }
            
        int result, child_pid;
        sscanf(buf, "%d %d", &result, &child_pid);
        int d = 1 + rand()%20;
        int index;
        for(int i = 0; i < n; i++)
        {   
            if(child_pid == child_pids[i])
            {
                index = i;
                break;
            }
        }
        if(result >= d)
        {
            sprintf(message, "Student %d finished stage %d", child_pid, stages[index]);
            if (write(write_fds[index], message, BUFF_SIZE) < 0)
                ERR("write to individual student pipe");
            printf("Teacher: %s\n", message);
            switch(stages[index])
            {
                case 1:
                    scores[index] += 3;
                    break;
                case 2:
                    scores[index] += 6;
                    break;
                case 3:
                    scores[index] += 7;
                    break;
                case 4:
                    scores[index] += 5;
                    break;
            }
            stages[index]++;
            finished_counter--;
        }
        else
        {
            if(!do_work)
                break;
            sprintf(message, "Student %d needs to fix stage %d", child_pid, stages[index]);
            if (write(write_fds[index], message, BUFF_SIZE) < 0)
                ERR("write to individual student pipe");
            printf("Teacher: %s\n", message);
        }
    }
    printf("Teacher END OF TIME!\n");
    for(int i = 0; i < n; i++)
    {
        if (close(write_fds[i]) == -1)
            ERR("close");
    }
    for(int i = 0; i < n; i ++)
    {
        printf("Teacher: %d -- %d\n", child_pids[i], scores[i]);
    }
    printf("IT'S FINALLY OVER!\n");
    
}

void create_children(int n, int* child_pid, int* fds, int student_w_pipe)
{
    int tmpfd[2];
    for(int i = 0; i < n; i++)
    {
        if (pipe(tmpfd))
            ERR("pipe");
        int chpid;
        switch(chpid = fork())
        {
            case 0:
                if (close(tmpfd[w_fd]) == -1)
                    ERR("close");
                child_work(tmpfd[r_fd], student_w_pipe);
                exit(EXIT_SUCCESS);
            
            case -1:
                ERR("Fork:");

            default:
                child_pid[i] = chpid;
                if (close(tmpfd[r_fd]) == -1)
                    ERR("close");
                fds[i] = tmpfd[w_fd];
        }
    }
}

int main(int argc, char** argv)
{
    if(argc != 2)
        usage(argv[0]);
    
    int n = atoi(argv[1]);
    if (n <= 0)
    {
        fprintf(stderr, "Number of children must be positive.\n");
        usage(argv[0]);
    }

    int child_pid[n];
    int fds[n];
    int shared_fd[2];
    if (pipe(shared_fd) == -1)
        ERR("pipe");

    if (sethandler(sigalarm_handler, SIGALRM))
        ERR("Seting SIGALRM:");

    create_children(n, child_pid, fds, shared_fd[w_fd]);
    parent_work(fds, shared_fd[r_fd], n, child_pid);
    wait_for_children(n);


    if (close(shared_fd[r_fd]) == -1)
        ERR("close");
    if (close(shared_fd[w_fd]) == -1)
        ERR("close");

    exit(EXIT_SUCCESS);
}
