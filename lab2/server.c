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
#define BUFF_SIZE 8192
#define ERR(source)                                                  \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), \
     kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t server_working = 1; //server working flag

void sigint_hendler() {server_working = 0;}
void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void thread_work_sum(union sigval sv)
{
	mqd_t *mq_ptr = (mqd_t*)sv.sival_ptr; //server quque descriptor


	struct sigevent not; //notification structure
	memset(&not, 0, sizeof(not)); 
	not.sigev_notify = SIGEV_THREAD;
	not.sigev_notify_function = thread_work_sum;
	not.sigev_notify_attributes = NULL; //thread creation attributes
	not.sigev_value.sival_ptr = mq_ptr;// thread routine argument
	
	if(mq_notify(*mq_ptr, &not) < 0)//setting up notification
		ERR("mq notify");

	char receive[BUFF_SIZE]; //received message from client

	while(1)
	{
		ssize_t ret = mq_receive(*mq_ptr, receive, BUFF_SIZE, NULL);
		if(ret < 0)
		{
			if(errno = EAGAIN)
				break;

			ERR("mq_receive");
			exit(1);
		}
		
		int a,b, pid;
		sscanf(receive, "%d %d %d", &a, &b, &pid);//parsing reeived message to a,b and clients pid
		
		char client_q_name[BUFF_SIZE];
		sprintf(client_q_name, "/%d", pid);//clients queue name


		char output[BUFF_SIZE]; //result message
		sprintf(output, "%d", a+b);
		mqd_t client_q; //cient queue descriptor
		if((client_q = TEMP_FAILURE_RETRY(mq_open(client_q_name, O_RDWR))) == (mqd_t)-1)
			ERR("server sum mq open");

		if(TEMP_FAILURE_RETRY(mq_send(client_q, output, BUFF_SIZE, 1)))
			ERR("server sum mq send");
	}

}

void thread_work_div(union sigval sv)
{
	mqd_t *mq_ptr = (mqd_t*)sv.sival_ptr; //server quque descriptor


	struct sigevent not;
	memset(&not, 0, sizeof(not));
	not.sigev_notify = SIGEV_THREAD;
	not.sigev_notify_function = thread_work_sum;
	not.sigev_notify_attributes = NULL; //thread creation attributes
	not.sigev_value.sival_ptr = mq_ptr;// thread routine argument

	if(mq_notify(*mq_ptr, &not) < 0)//setting up notification
		ERR("mq notify");

	char receive[BUFF_SIZE]; //received message from client

	while(1)
	{
		ssize_t ret = mq_receive(*mq_ptr, receive, BUFF_SIZE, NULL);
		if(ret < 0)
		{
			if(errno = EAGAIN)
				break;

			ERR("mq_receive");
			exit(1);
		}

		int a,b, pid;
		sscanf(receive, "%d %d %d", &a, &b, &pid);//parsing reeived message for a,b and clients pid

		char client_q_name[BUFF_SIZE];
		sprintf(client_q_name, "/%d", pid);//clients queue name


		char output[BUFF_SIZE]; //result message
		sprintf(output, "%d", a/b);
		mqd_t client_q; //cient queue descriptor
		if((client_q = TEMP_FAILURE_RETRY(mq_open(client_q_name, O_RDWR))) == (mqd_t)-1)
			ERR("server sum mq open");

		if(TEMP_FAILURE_RETRY(mq_send(client_q, output, BUFF_SIZE, 1)))
			ERR("server sum mq send");
	}

}

void thread_work_mod(union sigval sv)
{
	mqd_t *mq_ptr = (mqd_t*)sv.sival_ptr; //server quque descriptor
	struct sigevent not;
	memset(&not, 0, sizeof(not));
	not.sigev_notify = SIGEV_THREAD;
	not.sigev_notify_function = thread_work_sum;
	not.sigev_notify_attributes = NULL; //thread creation attributes
	not.sigev_value.sival_ptr = mq_ptr;// thread routine argument

	if(mq_notify(*mq_ptr, &not) < 0)//setting up notification
		ERR("mq notify");

	char receive[BUFF_SIZE]; //received message from client

	while(1)
	{
		ssize_t ret = mq_receive(*mq_ptr, receive, BUFF_SIZE, NULL);
		if(ret < 0)
		{
			if(errno = EAGAIN)
				break;

			ERR("mq_receive");
			exit(1);
		}

		int a,b, pid;
		sscanf(receive, "%d %d %d", &a, &b, &pid);//parsing reeived message for a,b and clients pid

		char client_q_name[BUFF_SIZE];
		sprintf(client_q_name, "/%d", pid);//clients queue name


		char output[BUFF_SIZE]; //result message
		sprintf(output, "%d", a%b);
		mqd_t client_q; //cient queue descriptor
		if((client_q = TEMP_FAILURE_RETRY(mq_open(client_q_name, O_RDWR))) == (mqd_t)-1)
			ERR("server sum mq open");

		if(TEMP_FAILURE_RETRY(mq_send(client_q, output, BUFF_SIZE, 1)))
			ERR("server sum mq send");
	}

}


int main()
{
    mqd_t q_sum, q_div, q_mod;
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = BUFF_SIZE;
    pid_t pid = getpid();

    char q_sum_name[20], q_div_name[20], q_mod_name[20];
    
    sprintf(q_sum_name, "/%d_s", pid); //names for server queues 
    sprintf(q_div_name, "/%d_d", pid);
    sprintf(q_mod_name, "/%d_m", pid);

	//open server queues for sum, div and mod
    if ((q_sum = TEMP_FAILURE_RETRY(
             mq_open(q_sum_name, O_RDWR | O_CREAT, 0600, &attr))) ==
        (mqd_t)-1)
        ERR("mq open PID_s");
    
    if ((q_div = TEMP_FAILURE_RETRY(
             mq_open(q_div_name, O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t)-1)
        ERR("mq open PID_d");

    if ((q_mod = TEMP_FAILURE_RETRY(
             mq_open(q_mod_name, O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t)-1)
        ERR("mq open PID_m");

    printf("\nSERVER: %s %s %s\n", q_sum_name, q_div_name, q_mod_name);
	

	struct sigaction sa; //signal handler for SIGINT
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_hendler;
	if(sigaction(SIGINT, &sa, NULL) < 0)
		ERR("sigaction()");
	
	struct sigevent sum;
	memset(&sum,0,sizeof(sum));
	sum.sigev_notify = SIGEV_THREAD;
	sum.sigev_notify_function = thread_work_sum;
	sum.sigev_notify_attributes = NULL; //thread creation attributes
	sum.sigev_value.sival_ptr = &q_sum; //thread routine argument - sum queue descriptor
	if(mq_notify(q_sum, &sum) < 0)
		ERR("server q_sum mq_notify");
	
	struct sigevent div;
	memset(&sum,0,sizeof(div));
	sum.sigev_notify = SIGEV_THREAD;
	sum.sigev_notify_function = thread_work_div;
	sum.sigev_notify_attributes = NULL; //thread creation attributes
	sum.sigev_value.sival_ptr = &q_div; //thread routine argument - sum queue descriptor
	if(mq_notify(q_div, &div) < 0)
		ERR("server q_div mq_notify");


	struct sigevent mod;
	memset(&sum,0,sizeof(mod));
	sum.sigev_notify = SIGEV_THREAD;
	sum.sigev_notify_function = thread_work_mod;
	sum.sigev_notify_attributes = NULL; //thread creation attributes
	sum.sigev_value.sival_ptr = &q_mod; //thread routine argument - sum queue descriptor
	if(mq_notify(q_mod, &mod) < 0)
		ERR("server q_mod mq_notify");

	//block SIGUSR1 and SIGINT
	sigset_t set, oldset; 
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGINT);
	if(sigprocmask(SIG_BLOCK, &set, &oldset) < 0)
		ERR("sigprocmask()");

	while(sigsuspend(&oldset) < 0) //wait for SIGUSR1 or SIGINT
	{
		if(!server_working)
			break;
	}
	
	printf("\nSERVER %d closing\n", pid);	

	//close and unlink server queues
    mq_close(q_sum);
    mq_close(q_div);
    mq_close(q_mod);
    if (mq_unlink(q_sum_name))
        ERR("sever q_sum unlink");
    if (mq_unlink(q_div_name))
        ERR("server q_div unlink");

    if (mq_unlink(q_mod_name))
        ERR("server q_mod unlink");
    
    return EXIT_SUCCESS;
}