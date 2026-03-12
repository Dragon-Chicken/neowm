#include "config.h"

#define SOCK_PATH "/tmp/nwmc_socket"

#define CONFIG_COMMANDS \
    TOK(vertical_gaps) \
    TOK(horizontal_gaps) \
    TOK(border_size) \
    TOK(number_of_desktops) \
    TOK(resize_amount) \
    TOK(border_focus_color) \
    TOK(border_normal_color) \
    TOK(bind)

typedef enum Token {
  tok_none,
#define TOK(name) tok_##name ,
CONFIG_COMMANDS
#undef TOK
} Token;

int s, s2;
pthread_t server_thread;
extern Config *conf;

void sendret(int s, int *done, char ret) {
  char str[2] = {ret, '\0'};
  if (done == NULL) {
    if (send(s, str, sizeof(str), 0) < 0) {
      perror("send");
    }
  } else {
    if (!(*done)) {
      if (send(s, str, sizeof(str), 0) < 0) {
        perror("send");
        *done = 1;
      }
    }
  }
}

int getsize(int s, int *done) {
  char str[3] = {0};
  int n;
  if ((n = recv(s, str, sizeof(str), 0)) <= 0) {
    if (n < 0)
      perror("recv");
    *done = 1;
  }
  //printf("size = %d\n", (str[0] & 0x7f) | ((str[1] & 0x7f) << 7));
  return (str[0] & 0x7f) | ((str[1] & 0x7f) << 7);
}

int getdata(int s, int *done, int strlen, char **cmd) {
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

  *cmd = str;

  return n;
}

char *splitstring(char *s, char *delim) {
  static char *olds;
  char *token;

  if (s == NULL)
    s = olds;

  // scan leading delimiters (find token start)
  int done = 0;
  while (*s != '\0') {
    int d = 0;
    for (d = 0; delim[d] != '\0'; d++)
      if (*s == delim[d])
        break;
    if (delim[d] == '\0')
      break;
    else
      s++;
  }

  if (*s == '\0') {
    olds = s;
    return NULL;
  }

  token = s;
  // find token end
  done = 0;
  while (*s != '\0' && !done) {
    s++;
    for (int d = 0; delim[d] != '\0'; d++) {
      if (*s == delim[d]) {
        done = 1;
        break;
      }
    }
  }

  // if this token goes to the end of the string
  for (int d = 0; delim[d] != '\0'; d++)
    if (*s == delim[d])
      *s = '\0';

  olds = s + 1;
  return token;
}

int splitlen(char *s, char *delim) {
  int tokens = 0;

  if (s == NULL)
    return 0;

  while (*s != '\0') {
    // scan leading delimiters (find token start)
    int done = 0;
    while (*s != '\0') {
      int d = 0;
      for (d = 0; delim[d] != '\0'; d++)
        if (*s == delim[d])
          break;

      if (delim[d] == '\0')
        break;
      else
        s++;
    }

    if (*s == '\0')
      break;

    // find token end
    done = 0;
    while (*s != '\0' && !done) {
      s++;
      for (int d = 0; delim[d] != '\0'; d++) {
        if (*s == delim[d]) {
          done = 1;
          break;
        }
      }
    }
    tokens++;
  }
  return tokens;
}

Key makekeybind(char *keybind, char *cmd, char *args) {
  int mod = 0;
  KeySym keysym = 0;
  int (*func)(Arg *) = NULL;
  Arg arg = {0};

  char *strptr = splitstring(keybind, " \t\n\r");
  while (strptr != NULL) {
    printf("[%s]\n", strptr);
    if (strcmp(strptr, "alt") == 0) {
      mod |= Mod1Mask;
    } else if (strcmp(strptr, "numlock") == 0) {
      mod |= Mod2Mask;
    } else if (strcmp(strptr, "altgr") == 0) {
      mod |= Mod3Mask;
    } else if (strcmp(strptr, "super") == 0) {
      mod |= Mod4Mask;
    } else if (strcmp(strptr, "scrolllock") == 0) {
      mod |= Mod5Mask;
    } else if (strcmp(strptr, "shift") == 0) {
      mod |= ShiftMask;
    } else if (strcmp(strptr, "control") == 0 || strcmp(strptr, "ctrl") == 0) {
      mod |= ControlMask;
    } else if (keysym == 0 && strlen(strptr) == 1) {
      keysym = XStringToKeysym(strptr);
    }

    strptr = splitstring(NULL, " \t\n\r");
  }
  
  printf("cmd = [%s]\n", cmd);

  if (strcmp(cmd, "spawn") == 0) {
    func = spawn;
  } else if (strcmp(cmd, "exit") == 0) {
    func = exitwm;
  } else if (strcmp(cmd, "kill_window") == 0) {
    func = killfocused;
  } else if (strcmp(cmd, "focus_switch") == 0) {
    func = focusswitch;
  } else if (strcmp(cmd, "resize_window") == 0) {
    func = resizeclient;
  } else if (strcmp(cmd, "switch_desktop") == 0) {
    func = focusdesktop;
  } else {
    printf("set none\n");
  }

  printf("arg len = %d\n", splitlen(args, " \t\n\r"));

  if (func != spawn) {
    printf("is not spawn\n");
    arg.i = strtol(args, NULL, 10);
  } else {
    int argslen = splitlen(args, " \t\n\r");
    arg.s = malloc(sizeof(char *) * (argslen + 1));

    strptr = splitstring(args, " \t\n\r");
    for (int i = 0; i < argslen; i++) {
      printf("[%s]\n", strptr);

      int strptrlen = strlen(strptr);
      arg.s[i] = malloc(sizeof(char *) * (strptrlen + 1));
      memcpy(arg.s[i], strptr, strptrlen);
      arg.s[i][strptrlen + 1] = '\0';

      strptr = splitstring(NULL, " \t\n\r");
    }
  }

  if (mod == 0) {
    printf("mod == 0\n");
  }
  if (keysym == 0) {
    printf("keysym == 0\n");
  }
  if (func == NULL) {
    printf("func == NULL\n");
  }

  if (mod == 0 || keysym == 0 || func == NULL) {
    printf("something was 0\n");
    return (Key){0, 0, NULL, {0}};
  }

  printf("#### token made ####\n");

  return (Key){mod, keysym, func, arg};
}

int handletoken(Token *token, char *str, char **keybind, char **cmd, char **args) {
  int ret = 1;

  if (*token != tok_none && *token != tok_bind) {
    ret = 0;
  }

  if (*token == tok_none) {
#define TOK(name) \
    if (strcmp(str, #name) == 0) { \
      *token = tok_##name; \
    }
    CONFIG_COMMANDS
#undef TOK
    printerr("invalid input\n");
    return 3;
  } else if (*token == tok_bind) {
    //printf("%s\n", str);
    if (*keybind) {
      if (*cmd) {
        *args = str;
        Key *oldkeys = conf->keys;
        conf->keyslen++;
        conf->keys = malloc(sizeof(Key) * conf->keyslen);
        if (conf->keys) {
          memcpy(conf->keys, oldkeys, sizeof(Key) * conf->keyslen-1);
        } else {
          printerr("FAILED TO ALLOCATE MEMORY FOR KEYBIND\n");
          exitwm(NULL);
        }
        printf("keybind: [%s]\n", *keybind);
        printf("cmd: [%s]\n", *cmd);
        printf("args: [%s]\n", *args);
        conf->keys[conf->keyslen - 1] = makekeybind(*keybind, *cmd, *args);
        if (conf->keys[conf->keyslen - 1].func == NULL) {
          printerr("invalid key\n");
          free(conf->keys);
          conf->keys = oldkeys;
          ret = 4;
        } else {
          free(oldkeys);
          ret = 0;
        }

        free(*args);
        free(*cmd);
        free(*keybind);
        *args = NULL;
        *cmd = NULL;
        *keybind = NULL;

        flushx11();
      } else *cmd = str;
    } else *keybind = str;
  } else {
    switch (*token) {
      case tok_vertical_gaps:
        conf->vgaps = strtol(str, NULL, 10);
        break;
      case tok_horizontal_gaps:
        conf->hgaps = strtol(str, NULL, 10);
        break;
      case tok_border_size:
        conf->bord_size = strtol(str, NULL, 10);
        break;
      case tok_number_of_desktops:
        conf->num_of_desktops = strtol(str, NULL, 10);
        break;
      case tok_resize_amount:
        conf->resize_amount = strtol(str, NULL, 10);
        break;
      case tok_border_focus_color:
        conf->bord_foc_col = strtol(str, NULL, 16);
        break;
      case tok_border_normal_color:
        conf->bord_nor_col = strtol(str, NULL, 16);
        break;

      default: // just here to hide the "enumeration value not handled" warning
        ret = 5;
        break;
    }
  }

  if (*token != tok_bind) {
    free(str);
  }

  if (ret == 0 && *token != tok_bind) {
    flushx11();
  }

  return ret;
}

int handleconnection(void) {
  char *str = NULL;

  char *keybind = NULL;
  char *cmd = NULL;
  char *args = NULL;

  Token token = tok_none;

  int issize = 1;
  int size = 0;
  int done = 0;
  int ret = 1;
  do {
    if (issize == 1) {
      size = getsize(s2, &done) + 1;
      if (size == 1) {
        done = 1;
      }
      issize = 0;
    } else {
      int datasize = getdata(s2, &done, size, &str);
      /*printf("size = %d\n", size);
      printf("datasize = %d\n", datasize);*/
      if (datasize != size && datasize != -1) {
        sendret(s2, &done, 2);
        free(str);
      } else {
        sendret(s2, &done, 1);
        issize = 1;
        if (ret == 0 || ret == 2) {
          ret = 2;
          token = tok_none;
          free(str);
        } else {
          ret = handletoken(&token, str, &keybind, &cmd, &args);
        }
      }
    }
  } while (!done);

  return ret;
}

void *serverthread(void *arg) {
  (void)arg;

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

    sendret(s2, NULL, handleconnection() + 1);

    printf("closing connection\n");
    close(s2);
  }
}

int startserver(void) {
  pthread_create(&server_thread, NULL, serverthread, NULL);
  close(s2);
  close(s);
  return 0;
}

int killserver(void) {
  printf("killing server\n");
  pthread_cancel(server_thread);
  pthread_join(server_thread, NULL);
  printf("server killed\n");
  return 0;
}
