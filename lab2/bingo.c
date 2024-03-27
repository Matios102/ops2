#define _GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LIFE_SPAN 10
#define MAX_NUM 10

#define ERR(source)                                                  \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), \
     kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t children_left = 0;

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void *thread_func(void *args)
{
    uint8_t ni;
    unsigned msg_prio;
    mqd_t *pin = (mqd_t *)args;
    while (children_left)
    {
        if (mq_receive(*pin, (char *)&ni, 1, &msg_prio) < 1)
        {
            ERR("mq_receive");
        }
        if (0 == msg_prio)
            printf("mq: got timeout from %d.\n", ni);
        else
            printf("mq:%d is a bingo number!\n", ni);

        children_left--;
    }
    return NULL;
}

void sigchld_handler(int sig, siginfo_t *s, void *p)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void child_work(int n, mqd_t pin, mqd_t pout)
{
    int life;
    uint8_t ni;
    uint8_t my_bingo;
    srand(getpid());
    life = rand() % LIFE_SPAN + 1;
    my_bingo = (uint8_t)(rand() % MAX_NUM);
    while (life--)
    {
        if (TEMP_FAILURE_RETRY(mq_receive(pout, (char *)&ni, 1, NULL)) < 1)
            ERR("mq_receive");
        printf("[%d] Received %d\n", getpid(), ni);
        if (my_bingo == ni)
        {
            if (TEMP_FAILURE_RETRY(mq_send(pin, (const char *)&my_bingo, 1, 1)))
                ERR("mq_send");
            return;
        }
    }
    if (TEMP_FAILURE_RETRY(mq_send(pin, (const char *)&n, 1, 0)))
        ERR("mq_send");
}

void parent_work(mqd_t pout)
{
    srand(getpid());
    uint8_t ni;
    while (children_left)
    {
        ni = (uint8_t)(rand() % MAX_NUM);
        if (TEMP_FAILURE_RETRY(mq_send(pout, (const char *)&ni, 1, 0)))
            ERR("mq_send");
        sleep(1);
    }
    printf("[PARENT] Terminates \n");
}

void create_children(int n, mqd_t pin, mqd_t pout)
{
    while (n-- > 0)
    {
        switch (fork())
        {
        case 0:
            child_work(n, pin, pout);
            exit(EXIT_SUCCESS);
        case -1:
            perror("Fork:");
            exit(EXIT_FAILURE);
        }
        children_left++;
    }
}

void create_thread(pthread_t *tid, mqd_t *pin)
{
    pthread_create(tid, NULL, thread_func, (void *)pin);
}

void usage(void)
{
    fprintf(stderr, "USAGE: signals n k p l\n");
    fprintf(stderr, "100 >n > 0 - number of children\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n;
    if (argc != 2)
        usage();
    n = atoi(argv[1]);
    if (n <= 0 || n >= 100)
        usage();

    mqd_t pin, pout;
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 1;
    if ((pin = TEMP_FAILURE_RETRY(
             mq_open("/bingo_in", O_RDWR | O_CREAT, 0600, &attr))) ==
        (mqd_t)-1)
        ERR("mq open in");
    if ((pout = TEMP_FAILURE_RETRY(
             mq_open("/bingo_out", O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t)-1)
        ERR("mq open out");

    sethandler(sigchld_handler, SIGCHLD);
    create_children(n, pin, pout);
    pthread_t tid;
    create_thread(&tid, &pin);

    parent_work(pout);

    pthread_join(tid, NULL);
    mq_close(pin);
    mq_close(pout);
    if (mq_unlink("/bingo_in"))
        ERR("mq unlink");
    if (mq_unlink("/bingo_out"))
        ERR("mq unlink");
    return EXIT_SUCCESS;
}