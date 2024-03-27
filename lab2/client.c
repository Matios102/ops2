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


void client_work(mqd_t sq, mqd_t mq) 
{

    size_t line_size = 8192;
	char* line = (char*)malloc(sizeof(char)*line_size);
	while(1)
	{
    size_t read = getline(&line, &line_size, stdin); //reads from stdin
    if(read < 0)
  		break;
	

    int a,b;
    sscanf(line, "%d %d", &a, &b); //parse message 

    int pid = getpid();

    char message[BUFF_SIZE];
    sprintf(message,"%d %d %d", a,b,pid); //create message to be sent to server

    if (TEMP_FAILURE_RETRY(mq_send(sq, message, BUFF_SIZE, 1)))
                ERR("mq_send");
	

	struct timespec ts;
	if(clock_gettime(CLOCK_REALTIME, &ts) < 0)
		ERR("clock_gettime()");

	char res_message[BUFF_SIZE];

	ts.tv_nsec += 100 * 1000000; //server response timeout 100 ms
	if (mq_timedreceive(mq, res_message, BUFF_SIZE, NULL, &ts) < 0)
    {
		if(errno == ETIMEDOUT)
		{
			printf("Timed out\n");
			break;
		}

		char name[20];
		sprintf(name, "/%d", getpid());
		mq_close(mq);
		if(mq_unlink(name))
			ERR("client mq unlink");
		ERR("mq_timedreceive");
	}

	int res;

	sscanf(res_message, "%d", &res);

	printf("CLINET: res: %d\n", res);
	}

	free(line);
}

void read_args( int argc, char **argv, char *sq_name)
{
    if (argc != 2)
    {
        exit(EXIT_FAILURE);
    }

    strcpy(sq_name, argv[1]);
}

int main(int argc, char **argv)
{
    mqd_t mq, sq;
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = BUFF_SIZE;
    pid_t pid = getpid();

    char mq_name[20];
    char sq_name[20];

    read_args(argc, argv, sq_name);    

    
    sprintf(mq_name, "/%d", pid);
    if ((mq = TEMP_FAILURE_RETRY(
             mq_open(mq_name, O_RDWR | O_CREAT, 0600, &attr))) ==
        (mqd_t)-1)
        ERR("mq open mq");
    
    if((sq = TEMP_FAILURE_RETRY(
             mq_open(sq_name, O_RDWR, 0600, &attr))) == (mqd_t)-1)
        ERR("mq open sq");
    
    client_work(sq, mq);

    mq_close(mq);
	printf("CLIENT CLOSING\n");

    if (mq_unlink(mq_name))
        ERR("client mq unlink");

    return EXIT_SUCCESS;
}