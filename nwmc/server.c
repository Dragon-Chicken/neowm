#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

#define SOCK_PATH "nwmc_socket"

int s, s2;

void sigtermhandler(int sig) {
  printf("exiting...\n");
  close(s2);
  close(s);

  _exit(sig);
}

void sendret(int s, int *done, char ret) {
  char str[2] = {ret, '\0'};
  if (!(*done)) {
    if (send(s, str, sizeof(str), 0) < 0) {
      perror("send");
      *done = 1;
    }
  }
}

int getsize(int s, int *done) {
  char str[3];
  int n;
  if ((n = recv(s, str, sizeof(str), 0)) <= 0) {
    if (n < 0)
      perror("recv");
    *done = 1;
  }
  //printf("size = %d\n", (str[0] & 0x7f) | ((str[1] & 0x7f) << 7));
  return (str[0] & 0x7f) | ((str[1] & 0x7f) << 7);
}

int getdata(int s, int *done, int strlen) {
  printf("getdata\n");
  printf("strlen = %d\n", strlen);
  char *str = malloc(sizeof(char) * strlen);
  int n;
  if ((n = recv(s, str, strlen, 0)) <= 0) {
    if (n < 0)
      perror("recv");
    *done = 1;
  }
  str[n] = '\0';
  printf("[%s]\n", str);

  free(str);
  return n;
}

int main(void) {
  signal(SIGINT, sigtermhandler);

  int len;
  struct sockaddr_un remote, local = {
    .sun_family = AF_UNIX,
  };

  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  strcpy(local.sun_path, SOCK_PATH);
  unlink(local.sun_path);
  len = strlen(local.sun_path) + sizeof(local.sun_family);
  if (bind(s, (struct sockaddr *)&local, len) == -1) {
    perror("bind");
    exit(1);
  }

  if (listen(s, 5) == -1) {
    perror("listen");
    exit(1);
  }

  for(;;) {
    printf("Waiting for a connection...\n");
    socklen_t slen = sizeof(remote);
    if ((s2 = accept(s, (struct sockaddr *)&remote, &slen)) == -1) {
      perror("accept");
      exit(1);
    }

    printf("Connected.\n");

    int issize = 1;
    int size = 0;
    int done = 0;
    do {
      if (issize == 1) {
        size = getsize(s2, &done) + 1;
        issize = 0;
      } else {
        int datasize = getdata(s2, &done, size);
        printf("size = %d\n", size);
        printf("datasize = %d\n", datasize);
        if (datasize != size && datasize != -1) {
          sendret(s2, &done, 2);
        } else {
          sendret(s2, &done, 1);
          issize = 1;
        }
      }
    } while (!done);
    printf("closing connection\n");
    close(s2);
  }
  return 0;
}
