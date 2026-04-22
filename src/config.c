#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <ctype.h>

#include <X11/XF86keysym.h>

#include "main.h"
#include "config.h"
#include "memory.h"

#define SOCK_PATH "/tmp/nwmc_socket"

// xmacro btw
#define CONFIG_COMMANDS \
    TOK(refresh_rate) \
    TOK(split_ratio) \
    TOK(vertical_gaps) \
    TOK(horizontal_gaps) \
    TOK(border_size) \
    TOK(number_of_desktops) \
    TOK(desktop_names) \
    TOK(minimum_size) \
    TOK(resize_amount) \
    TOK(move_amount) \
    TOK(focused_border_color) \
    TOK(border_color) \
    TOK(focused_border_color_floating) \
    TOK(border_color_floating) \
    TOK(bind)

typedef enum Token {
  tok_none,
#define TOK(name) tok_##name ,
CONFIG_COMMANDS
#undef TOK
} Token;

int s, s2;
pthread_t server_thread = 0;
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
  return (str[0] & 0x7f) | ((str[1] & 0x7f) << 7);
}

int getdata(int s, int *done, int strlen, char **cmd) {
  char *str = nwm_malloc(sizeof(char) * strlen);
  int n;
  if ((n = recv(s, str, strlen, 0)) <= 0) {
    if (n < 0)
      perror("recv");
    *done = 1;
  }

  str[n] = '\0';
  *cmd = str;

  return n;
}

void stripstr(char *s, char *delim, int len) {
  char *d = s;
  char *c = NULL;

  for (int i = 0; i < len; i++) {
    c = delim;
    while (*c) {
      while (*d == *c) {
        d++;
      }
      c++;
    }
    *s++ = *d++;
  }
}

void parse(Token *token, char *str, Key *key, int *keystate) {
  if (*token == tok_none) {
#define TOK(name) \
    if (strcmp(str, #name) == 0) { \
      *token = tok_##name; \
    }
    CONFIG_COMMANDS
#undef TOK
    return;
  }

  // settings tokens
  if (*token == tok_number_of_desktops) {
    conf->num_of_desktops = strtol(str, NULL, 10);
    setdesktops();
  } else if (*token == tok_desktop_names) {
    int len = strlen(str) + 1; // +1 to count null

    if (conf->desktop_names)
      nwm_free(conf->desktop_names);

    conf->desktop_names = nwm_malloc(sizeof(char) * len);
    memcpy(conf->desktop_names, str, len);
    char *sptr = strtok(conf->desktop_names, " \t\n\r");
    while (sptr) {
      sptr = strtok(NULL, " \t\n\r");
    }
    stripstr(conf->desktop_names, " \t\n\r", len);
    conf->desktop_names_len = len;

    setdesktops();
  }

  switch (*token) {
    // borders
    case tok_border_size:
      conf->bord_size = strtol(str, NULL, 10); break;

    case tok_focused_border_color:
      conf->bord_foc_col = strtol(str, NULL, 16); break;
    case tok_border_color:
      conf->bord_col = strtol(str, NULL, 16); break;
    case tok_focused_border_color_floating:
      conf->bord_foc_col_float = strtol(str, NULL, 16); break;
    case tok_border_color_floating:
      conf->bord_col_float = strtol(str, NULL, 16); break;

    case tok_refresh_rate:
      conf->refreshrate = strtol(str, NULL, 10); break;
    case tok_minimum_size:
      conf->min_size = strtol(str, NULL, 10); break;

    case tok_vertical_gaps:
      conf->vgaps = strtol(str, NULL, 10); break;
    case tok_horizontal_gaps:
      conf->hgaps = strtol(str, NULL, 10); break;
    case tok_move_amount:
      conf->move_amount = strtol(str, NULL, 10); break;

    // floats
    case tok_split_ratio:
      conf->split_ratio = strtol(str, NULL, 10)/100.0f; break;
    case tok_resize_amount:
      conf->resize_amount = strtol(str, NULL, 10)/100.0f; break;

    default:
      break;
  }

  // bind
  if (*token != tok_bind) { // doing to remove indents...
    remapwins();
    return;
  }

  if (*keystate == 0) {
    char *sptr = strtok(str, " \t\n\r");
    key->mod = 0;
    while (sptr) {
      if (strcmp(sptr, "clear") == 0) {
        unbindkeys();
        clearconfigbinds();
        *keystate = 0;
        return;
      }

      if (strlen(sptr) == 1) {
        key->keysym = XStringToKeysym(sptr);
      } else if (strcmp(sptr, "alt") == 0) {
        key->mod |= Mod1Mask;
      } else if (strcmp(sptr, "numlock") == 0) {
        key->mod |= Mod2Mask;
      } else if (strcmp(sptr, "altgr") == 0) {
        key->mod |= Mod3Mask;
      } else if (strcmp(sptr, "super") == 0) {
        key->mod |= Mod4Mask;
      } else if (strcmp(sptr, "scrolllock") == 0) {
        key->mod |= Mod5Mask;
      } else if (strcmp(sptr, "shift") == 0) {
        key->mod |= ShiftMask;
      } else if (strcmp(sptr, "control") == 0 || strcmp(sptr, "ctrl") == 0) {
        key->mod |= ControlMask;

      } else if (strcmp(sptr, "xf86audioraisevolume") == 0) {
        key->keysym = XF86XK_AudioRaiseVolume;
      } else if (strcmp(sptr, "xf86audiolowervolume") == 0) {
        key->keysym = XF86XK_AudioLowerVolume;
      } else if (strcmp(sptr, "xf86audiomute") == 0) {
        key->keysym = XF86XK_AudioMute;
      } else if (strcmp(sptr, "print") == 0) {
        key->keysym = XK_Print;
      } else if (strcmp(sptr, "space") == 0) {
        key->keysym = XK_space;
      } else if (strcmp(sptr, "enter") == 0) {
        key->keysym = XK_Return;

      } else if (strcmp(sptr, "button1") == 0) {
        key->keysym = Button1; key->btn = True;
      } else if (strcmp(sptr, "button2") == 0) {
        key->keysym = Button2; key->btn = True;
      } else if (strcmp(sptr, "button3") == 0) {
        key->keysym = Button3; key->btn = True;
      } else if (strcmp(sptr, "button4") == 0) {
        key->keysym = Button4; key->btn = True;
      } else if (strcmp(sptr, "button5") == 0) {
        key->keysym = Button5; key->btn = True;
      }

      sptr = strtok(NULL, " \t\n\r");
    }
  } else if (*keystate == 1) {
    key->func = NULL;
    if (strcmp(str, "spawn") == 0) {
      key->func = spawn;
    } else if (strcmp(str, "exit") == 0) {
      key->func = exitwm;
    } else if (strcmp(str, "float_toggle") == 0) {
      key->func = floattoggle;
    } else if (strcmp(str, "kill_window") == 0) {
      key->func = killfocused;
    } else if (strcmp(str, "focus_toggle") == 0) {
      key->func = focustoggle;
    } else if (strcmp(str, "focus_window") == 0) {
      key->func = focuswindow;
    } else if (strcmp(str, "swap_window") == 0) {
      key->func = swapwindow;
    } else if (strcmp(str, "move_window") == 0) {
      key->func = movewindow;
    } else if (strcmp(str, "center_window") == 0) {
      key->func = centerwindow;
    } else if (strcmp(str, "drag_move_window") == 0) {
      key->func = dragmovewindow;
    } else if (strcmp(str, "drag_resize_window") == 0) {
      key->func = dragresizewindow;
    } else if (strcmp(str, "resize_window") == 0) {
      key->func = resizewindow;
    } else if (strcmp(str, "focus_desktop") == 0) {
      key->func = focusdesktop;
    } else if (strcmp(str, "move_desktop") == 0) {
      key->func = movedesktop;
    } else {
      printerr("is not function\n");
    }
  } else if (*keystate == 2) {
    if (key->func != spawn) {
      if (strcmp(str, "up") == 0) {
        key->args.i = dirup;
      } else if (strcmp(str, "down") == 0) {
        key->args.i = dirdown;
      } else if (strcmp(str, "left") == 0) {
        key->args.i = dirleft;
      } else if (strcmp(str, "right") == 0) {
        key->args.i = dirright;
      } else {
        key->args.i = strtol(str, NULL, 10);
      }
    } else {
      int len = strlen(str) + 1; // +1 to count null

      int num_of_args = 0;

      char *sptr = strtok(str, " \t\n\r");
      while (sptr) {
        sptr = strtok(NULL, " \t\n\r");
        num_of_args++;
      }

      stripstr(str, " \t\n\r", len);
      key->args.s = nwm_malloc(sizeof(char **) * num_of_args + 1);

      for (int i = 0; i < num_of_args; i++) {
        while (!*str) // go till not null
          str++;

        len = strlen(str) + 1;
        key->args.s[i] = nwm_malloc(sizeof(char *) * len);
        memcpy(key->args.s[i], str, len);

        while (*str) // go till null
          str++;
      }
      key->args.s[num_of_args] = NULL;
    }

    if (!key->btn) {
      conf->keyslen++;
      Key *tofree = conf->keys;
      conf->keys = nwm_malloc(sizeof(Key) * conf->keyslen);
      if (tofree) {
        memcpy(conf->keys, tofree, sizeof(Key) * (conf->keyslen - 1));
        nwm_free(tofree);
      }
      conf->keys[conf->keyslen - 1] = *key;

      *keystate = 0;
    } else {
      conf->btnslen++;
      Key *tofree = conf->btns;
      conf->btns = nwm_malloc(sizeof(Key) * conf->btnslen);
      if (tofree) {
        memcpy(conf->btns, tofree, sizeof(Key) * (conf->btnslen - 1));
        nwm_free(tofree);
      }
      conf->btns[conf->btnslen - 1] = *key;

      *keystate = 0;
    }
    bindkeys();
  }

  *keystate += 1;

  //exitwm(0);
}

int handleconnection(void) {
  char *str = NULL;

  Token token = tok_none;

  Bool issize = True;
  int size = 0;
  int ret = 1; // 0 == ok, 1 == error, 2 == resend data

  Key key = {0};
  int keystate = 0;

  // request data till the client sends an empty message or an error
  int done = 0;
  do {
    // first client sends size of message
    // then sends the message
    if (issize) {
      size = getsize(s2, &done) + 1;
      if (size == 1)
        done = 1;
      issize = False;

    } else {
      int datasize = getdata(s2, &done, size, &str);
#ifdef NWM_DEBUG
      printf("nwmc command:[%s]\n", str);
#endif

      ret = 0;

      if (datasize != size && datasize != -1) {
        printerr("wtf is going on (server error)\n");
        exitwm(0);
      }

      parse(&token, str, &key, &keystate);

      sendret(s2, &done, 1);
      issize = True;

      nwm_free(str);
      str = NULL;
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

  // create the socket
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

  // handle clients
  for(;;) {
    // wait for client connection
    socklen_t slen = sizeof(remote);
    if ((s2 = accept(s, (struct sockaddr *)&remote, &slen)) == -1) {
      perror("accept");
      exit(1);
    }
    // handle client connection and
    // send return value back to the nwmc client
    sendret(s2, NULL, handleconnection() + 1);
    // close connection
    close(s2);
  }
}

int startserver(void) {
#ifdef NWM_DEBUG
  printf("startserver\n");
#endif
  pthread_create(&server_thread, NULL, serverthread, NULL);
  close(s2);
  close(s);
  return 0;
}

int killserver(void) {
#ifdef NWM_DEBUG
  printf("killserver\n");
#endif
  if (!server_thread) {
    return 0;
  }
  pthread_cancel(server_thread);
  pthread_join(server_thread, NULL);
  return 0;
}
