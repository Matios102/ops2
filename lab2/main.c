#define _GNU_SOURCE
#include <errno.h>
#include <mqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 255
#define T1 100
#define T2 5000
#define ERR(source)                                                            \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct timespec timespec_t;
volatile sig_atomic_t should_exit = 0;

void sigint_handler() { should_exit = 1; }

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void msleep(UINT milisec) {
  time_t sec = (int)(milisec / 1000);
  milisec = milisec - (sec * 1000);
  timespec_t req = {0};
  req.tv_sec = sec;
  req.tv_nsec = milisec * 1000000L;
  if (nanosleep(&req, &req))
{
		if (errno == EINTR)
			return;
    ERR("nanosleep");
}
}

void child_work(int N, mqd_t server_q) {
  char worker_q_name[BUF_SIZE]; //worker queue name
  sprintf(worker_q_name, "/result_queue_%d_%d", getppid(), getpid()); // /result_queue_{server_pid}_{worker_pid}
  mqd_t worker_q;
struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_msgsize = BUF_SIZE;
    attr.mq_maxmsg = 3;
		
  if ((worker_q = TEMP_FAILURE_RETRY(mq_open(
           worker_q_name, O_RDWR | O_NONBLOCK | O_CREAT, 0600, &attr))) ==
      (mqd_t)-1)
    ERR("mq open in");
  srand(getpid());
  printf("[%d] Worker ready!\n", getpid());
  while (1) {
    char buf[BUF_SIZE];
		unsigned  prio;
    if (TEMP_FAILURE_RETRY(mq_receive(server_q, buf, BUF_SIZE, &prio)) < 1) //receive task
      ERR("mq_receive");
		if (prio == 10) //server sent exit
			break;
    float a, b;
    sscanf(buf, "%f %f", &a, &b);
    printf("[%d] Recieved task %f %f\n", getpid(), a, b);
		UINT sleep_time = rand() % 1500 + 500;
		printf("[%d] sleeps for %d\n", getpid(), sleep_time);
    msleep(sleep_time);
    char result[BUF_SIZE];
    int len = sprintf(result, "%f %d", a + b, getpid());
    printf("[%d] Result sent: %f\n", getpid(), a+b);
    if (TEMP_FAILURE_RETRY(mq_send(worker_q, result, len + 1, 1)))
      ERR("mq_send");
  }
  mq_close(worker_q);
  if (mq_unlink(worker_q_name))
    ERR("mq unlink");
  printf("[%d] Worker exits!\n", getpid());
}

void parent_work(mqd_t server_q, pid_t *workers, int N) {
  srand(getpid());
  int i = 0;
  while (!should_exit) {
    msleep(rand() % (T2 - T1) + T1);
    struct mq_attr attr;
    mq_getattr(server_q, &attr); //get queue attributes
    if (attr.mq_curmsgs == attr.mq_maxmsg) { //check if queue is full
      printf("Queue full!!\n");
      continue;
    }
    char buf[BUF_SIZE];
    int len = sprintf(buf, "%f %f", (float)rand() / (float)RAND_MAX * 100,
                      (float)rand() / (float)RAND_MAX * 100);
    if (TEMP_FAILURE_RETRY(mq_send(server_q, buf, len, 1))) //send task
      ERR("mq_send");
    printf("New task queued: %s\n", buf);
    i++;
  }
	char buf[BUF_SIZE];
	int len = sprintf(buf, "EXIT!!"); //send exit message to workers with priority 10
	for (i = 0; i < N; i++)
	{
    if (TEMP_FAILURE_RETRY(mq_send(server_q, buf, len, 10)))
      ERR("mq_send");
	}
}

void create_children(int N, mqd_t server_q, pid_t *workers) {
  while (N-- > 0) {
    pid_t child_pid = fork();
    switch (child_pid) {
    case 0:
      child_work(N, server_q);
      exit(EXIT_SUCCESS);
    case -1:
      perror("Fork:");
      exit(EXIT_FAILURE);
    default:
      workers[N] = child_pid;
    }
  }
}

void thread_routine(union sigval sv) {

  mqd_t *mq_ptr = (mqd_t *)sv.sival_ptr;

  // Restore notification
  struct sigevent not ;
  memset(&not, 0, sizeof(not ));
  not .sigev_notify = SIGEV_THREAD;
  not .sigev_notify_function = thread_routine;
  not .sigev_notify_attributes = NULL; // Thread creation attributes
  not .sigev_value.sival_ptr = mq_ptr; // Thread routine argument
  if (mq_notify(*mq_ptr, &not ) < 0) {
    perror("mq_notify()");
    exit(1);
  }

  char buf[BUF_SIZE];

  // Empty the queue
  while (1) {
    ssize_t ret = mq_receive(*mq_ptr, buf, sizeof(buf), NULL);
    if (ret < 0) {
      if (errno == EAGAIN || errno == EBADF)
        break;
      exit(1);
    } else {
      pid_t child_pid;
      float val;
      sscanf(buf, "%f %d", &val, &child_pid);
      printf("Result from worker %d: [%f]\n", child_pid, val);
    }
  }
}

void subscribe_queues(int N, pid_t *workers, mqd_t* worker_queues) {
  int i;
  for (i = 0; i < N; i++) {
    char name_q[BUF_SIZE];
    sprintf(name_q, "/result_queue_%d_%d", getpid(), workers[i]);
    if ((worker_queues[i] = TEMP_FAILURE_RETRY(mq_open(name_q, O_RDWR))) == (mqd_t)-1)
      ERR("mq open in");
    struct sigevent not ;
    memset(&not, 0, sizeof(not ));
    not .sigev_notify = SIGEV_THREAD;
    not .sigev_notify_function = thread_routine;
    not .sigev_notify_attributes = NULL; // Thread creation attributes
    not .sigev_value.sival_ptr = &worker_queues[i];    // Thread routine argumen
    if (mq_notify(worker_queues[i], &not ) < 0) {
			ERR("mq notify");
    }
  }
}

int main(int argc, char **argv) {
  int N;
  if (argc != 2) {
    ERR("usage");
  }
  N = atoi(argv[1]);
  if (N < 2 || N > 20) {
    ERR("usage N");
  }
	sethandler(SIG_IGN, SIGCHLD);
	sethandler(sigint_handler, SIGINT);
	
  mqd_t server_q;
  char server_q_name[BUF_SIZE];
  sprintf(server_q_name, "/%d", getpid()); //server queue name /pid
struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_msgsize = BUF_SIZE;
    attr.mq_maxmsg = 3;
  if ((server_q = TEMP_FAILURE_RETRY(
           mq_open(server_q_name, O_RDWR | O_CREAT, 0600, &attr))) == (mqd_t)-1)
    ERR("mq open in");
  pid_t *workers = malloc(N * sizeof(pid_t));
  if (workers == NULL)
    ERR("malloc");
  printf("Server started\n");
  create_children(N, server_q, workers); //create workers and store their pids
	msleep(100);
	mqd_t* worker_queues = malloc(N*sizeof(mqd_t));
  subscribe_queues(N, workers, worker_queues); //set up notification for worker queues using threads
  parent_work(server_q, workers, N); //parent work
  int status;
  while (wait(&status) > 0) //wait for all children to finish
    ;
	int i;
	for (i = 0; i < N; i++)
	{
		mq_close(worker_queues[i]); //close worker queues
	}
	free(worker_queues);
  free(workers);
  printf("All processes finished\n");
  mq_close(server_q);

  if (mq_unlink(server_q_name))
    ERR("mq unlink");
  return 0;
}
