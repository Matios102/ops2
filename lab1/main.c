#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

#define TEMP_FAILURE_RETRY(exp)                                                \
  ({                                                                           \
    typeof(exp) _rc;                                                           \
    do {                                                                       \
      _rc = (exp);                                                             \
    } while (_rc == -1 && errno == EINTR);                                     \
    _rc;                                                                       \
  })

// MAX_BUFF must be in one byte range
#define MAX_BUFF 200

int sethandler(void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1 == sigaction(sigNo, &act, NULL))
    return -1;
  return 0;
}

void sigchld_handler(int sig) {
  pid_t pid;
  for (;;) {
    pid = waitpid(0, NULL, WNOHANG);
    if (0 == pid)
      return;
    if (0 >= pid) {
      if (ECHILD == errno)
        return;
      ERR("waitpid:");
    }
  }
}

int int_len(int n) {
  int count = 0;
  do {
    n /= 10;
    ++count;
  } while (n != 0);
  return count;
}

void usage(char *name) {
  fprintf(stderr, "USAGE: %s\n", name);
  exit(EXIT_FAILURE);
}

void children_work(int read_fd, int write_fd) {
  srand(getpid());
  printf("PID: %d\n read_end: %d, write_end %d\n", getpid(), read_fd, write_fd);
  char c, buf[MAX_BUFF + 1];
  for (;;) {
	int status = read(read_fd, &c, 1);
	if (status < 0 && errno == EINTR)
		continue;
	if (status < 0)
            ERR("read header from R");
        if (0 == status)
            break; // pipe was closed on write end
        if (TEMP_FAILURE_RETRY(read(read_fd, buf, c)) < c)
            ERR("read data from R");
	int number = atoi(buf);
	if (number == 0)
		return; // exit 
        buf[(int)c] = 0;
        printf("PID: %d: %s\n",getpid(), buf);	
	number += (rand() % 21) - 10;
	int num_len = int_len(number);
	char write_buf[MAX_BUFF+1];
	write_buf[0] = num_len+1;
	sprintf(write_buf+1, "%d", number);
	if (TEMP_FAILURE_RETRY(write(write_fd, write_buf, num_len+2)) < 0)
		ERR("write");
  }
}

void create_children(int *pipe1, int *pipe2, int *pipe3) {
  // create p2
  switch (fork()) {
  case 0:
    // close pipe between p3 and parent, close write-end of p2 and parent, close
    // read-end of p2 and o3 pipe
    if (close(pipe3[0]) || close(pipe3[1]) || close(pipe1[1]) ||
        close(pipe2[0]))
      ERR("close pipe");
    children_work(pipe1[0], pipe2[1]);
    // close rest of the pipes
    if (close(pipe1[0]) || close(pipe2[1]))
      ERR("close pipe");
    exit(EXIT_SUCCESS);
  case -1:
    ERR("fork");
  }
  // create p3
  switch (fork()) {
  case 0:
    // close pipe between p2 and parent, close write-end of p2 and parent, close
    // read-end of p2 and o3 pipe
    if (close(pipe1[0]) || close(pipe1[1]) || close(pipe3[0]) ||
        close(pipe2[1]))
      ERR("close pipe");
    children_work(pipe2[0], pipe3[1]);
    // close rest of the pipes
    if (close(pipe3[1]) || close(pipe2[0]))
      ERR("close pipe");
    exit(EXIT_SUCCESS);
  case -1:
    ERR("fork");
  }
}
int main(int argc, char **argv) {
  if (argc != 1)
    usage(argv[1]);

  int pipe1[2], pipe2[2], pipe3[2];

  if (pipe(pipe1) || pipe(pipe2) || pipe(pipe3))
    ERR("pipe");
	printf("pipe1: r: %d w: %d pipe2: r: %d w: %d pipe3: r: %d w: %d\n", pipe1[0], pipe1[1], pipe2[0], pipe2[1], pipe3[0], pipe3[1]);
  if (sethandler(SIG_IGN, SIGPIPE))
   ERR("Setting SIGINT handler");
  if (sethandler(sigchld_handler, SIGCHLD))
    ERR("Setting parent SIGCHLD:");
  create_children(pipe1, pipe2, pipe3);
  if (close(pipe3[1]) || close(pipe1[0]) || close(pipe2[0]) || close(pipe2[1]))
    ERR("close pipe");
  char c, buf[3];
  *buf = 2;
  sprintf(buf + 1, "%d", 1);
  if (TEMP_FAILURE_RETRY(write(pipe1[1], buf, 3)) < 0)
    ERR("write 1");
  children_work(pipe3[0], pipe1[1]);
  if (close(pipe1[1]) || close(pipe3[0]))
    ERR("close pipe");
  int status;
  while (wait(&status) > 0)
    ;
}
