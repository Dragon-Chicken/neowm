#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK_PATH "/tmp/nwmc_socket"

void sendsize(int s, char *str) {
  printf("sendsize\n");

  int len = strlen(str);
  char strsize[3] = {(len & 0x7f) | (1 << 7), ((len >> 7) & 0x7f) | (1 << 7), '\0'};

  printf("sending %d\n", (strsize[0] & 0x7f) | ((strsize[1] & 0x7f) << 7));
  if (send(s, strsize, strlen(strsize)+1, 0) == -1) {
    perror("send");
    exit(1);
  }
}

void senddata(int s, char *str) {
  if (send(s, str, strlen(str)+1, 0) == -1) {
    perror("send");
    exit(1);
  }
}

int getret(int s) {
  int len;

  char str[2];
  
  if ((len=recv(s, str, sizeof(str), 0)) > 0) {
    printf("got [%d]\n", str[0]);
  } else {
    if (len < 0)
      perror("recv");
    else
      printf("Server closed connection\n");
    exit(1);
  }

  return str[0];
}

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    printf("too few arguments\n");
    exit(1);
  }

  for (int i = 0; i < argc; i++) {
    printf("[%s]\n", argv[i]);
  }

  int s, len;
  struct sockaddr_un remote = {
    .sun_family = AF_UNIX,
  };

  if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  printf("Trying to connect...\n");

  strcpy(remote.sun_path, SOCK_PATH);
  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(s, (struct sockaddr *)&remote, len) == -1) {
    perror("connect");
    exit(1);
  }

  printf("Connected.\n");

  for (int i = 1; i < argc; i++) {
    sendsize(s, argv[i]);
    do {
      senddata(s, argv[i]);
    } while (getret(s) == 2);
  }

  close(s);

  return 0;
}
