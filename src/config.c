#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <ctype.h>

#include <X11/XF86keysym.h>

#include "config.h"
#include "main.h"

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
    TOK(normal_border_color) \
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
  //printf("size = %d\n", (str[0] & 0x7f) | ((str[1] & 0x7f) << 7));
  return (str[0] & 0x7f) | ((str[1] & 0x7f) << 7);
}

int getdata(int s, int *done, int strlen, char **cmd) {
  //printf("getdata\n");
  //printf("strlen = %d\n", strlen);
  char *str = malloc(sizeof(char) * strlen);
  int n;
  if ((n = recv(s, str, strlen, 0)) <= 0) {
    if (n < 0)
      perror("recv");
    *done = 1;
  }
  str[n] = '\0';
  //printf("[%s]\n", str);

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
  //printf("start bind\n");
  int mod = 0;
  KeySym keysym = 0;
  int (*func)(Arg *) = NULL;
  Arg arg = {0};
  Bool btn = False;

  // get the keybinds (super shfit q, alt enter, etc)

  for (int i = 0; keybind[i]; i++) {
    keybind[i] = tolower(keybind[i]);
  }

  char *strptr = splitstring(keybind, " \t\n\r");

  while (strptr != NULL) {
    //printf("[%s]\n", strptr);

    if (strlen(strptr) == 1) {
      keysym = XStringToKeysym(strptr);

    } else if (strcmp(strptr, "alt") == 0) {
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

    } else if (strcmp(strptr, "xf86audioraisevolume") == 0) {
      keysym = XF86XK_AudioRaiseVolume;
    } else if (strcmp(strptr, "xf86audiolowervolume") == 0) {
      keysym = XF86XK_AudioLowerVolume;
    } else if (strcmp(strptr, "xf86audiomute") == 0) {
      keysym = XF86XK_AudioMute;
    } else if (strcmp(strptr, "print") == 0) {
      keysym = XK_Print;
    } else if (strcmp(strptr, "space") == 0) {
      keysym = XK_space;
    } else if (strcmp(strptr, "enter") == 0) {
      keysym = XK_Return;

    } else if (strcmp(strptr, "button1") == 0) {
      keysym = Button1; btn = True;
    } else if (strcmp(strptr, "button2") == 0) {
      keysym = Button2; btn = True;
    } else if (strcmp(strptr, "button3") == 0) {
      keysym = Button3; btn = True;
    } else if (strcmp(strptr, "button4") == 0) {
      keysym = Button4; btn = True;
    } else if (strcmp(strptr, "button5") == 0) {
      keysym = Button5; btn = True;
    }

    strptr = splitstring(NULL, " \t\n\r");
  }
  
  //printf("cmd = [%s]\n", cmd);

  // get the command (spawn, exitwm, kill_window, etc)
  if (strcmp(cmd, "spawn") == 0) {
    //printf("set spawn\n");
    func = spawn;
  } else if (strcmp(cmd, "exit") == 0) {
    func = exitwm;
  } else if (strcmp(cmd, "float_toggle") == 0) {
    func = floattoggle;
  } else if (strcmp(cmd, "kill_window") == 0) {
    func = killfocused;
  } else if (strcmp(cmd, "focus_toggle") == 0) {
    func = focustoggle;
  } else if (strcmp(cmd, "focus_window") == 0) {
    func = focuswindow;
  } else if (strcmp(cmd, "swap_window") == 0) {
    func = swapwindow;
  } else if (strcmp(cmd, "move_window") == 0) {
    func = movewindow;
  } else if (strcmp(cmd, "drag_move_window") == 0) {
    func = dragmovewindow;
  } else if (strcmp(cmd, "drag_resize_window") == 0) {
    func = dragresizewindow;
  } else if (strcmp(cmd, "resize_window") == 0) {
    func = resizewindow;
  } else if (strcmp(cmd, "focus_desktop") == 0) {
    func = focusdesktop;
  } else if (strcmp(cmd, "move_desktop") == 0) {
    func = movedesktop;
  } else {
    //printf("set none\n");
  }

  //printf("arg len = %d\n", splitlen(args, " \t\n\r"));

  //printf("args = [%s]\n", args);

  // spawn is the only case with more than one argument
  if (func == resizewindow || func == movewindow || func == focuswindow || func == swapwindow) {
    if (strcmp(args, "up") == 0) {
      arg.i = dirup;
    } else if (strcmp(args, "down") == 0) {
      arg.i = dirdown;
    } else if (strcmp(args, "left") == 0) {
      arg.i = dirleft;
    } else if (strcmp(args, "right") == 0) {
      arg.i = dirright;
    } else {
      arg.i = strtol(args, NULL, 10);
    }

  } else if (func != spawn) {
    arg.i = strtol(args, NULL, 10);
  } else {
    int argslen = splitlen(args, " \t\n\r");
    arg.s = malloc(sizeof(char *) * (argslen + 1));
    strptr = splitstring(args, " \t\n\r");
    for (int i = 0; i < argslen; i++) {
      // allocates memory because "args" may be freed
      int strptrlen = strlen(strptr);
      arg.s[i] = malloc(sizeof(char *) * (strptrlen + 1));
      memcpy(arg.s[i], strptr, strptrlen);
      arg.s[i][strptrlen] = '\0';

      strptr = splitstring(NULL, " \t\n\r");
    }

    arg.s[argslen] = '\0';
  }

  // something went wrong if any of these are 0
  if (mod == 0) {
    //printf("mod == 0\n");
  }
  if (keysym == 0) {
    //printf("keysym == 0\n");
  }
  if (func == NULL) {
    //printf("func == NULL\n");
  }

  if (mod == 0 || keysym == 0 || func == NULL) {
    //printf("something was 0\n");
    //return (Key){0, 0, NULL, {0}};
  }

  //printf("#### token made ####\n");

  return (Key){mod, keysym, func, arg, btn};
}

void handlekeybind(char *str, char **keybind, char **cmd, char **args, int *ret) {
  if (*keybind) {
    if (*cmd) {
      *args = str;

      Key key = makekeybind(*keybind, *cmd, *args);
      if (key.func == NULL) {
        printerr("invalid key\n");
        return;
      }

      // if key
      if (key.btn == False) {
        conf->keyslen++;
        Key *oldkeys = conf->keys;
        conf->keys = malloc(sizeof(Key) * conf->keyslen);
        if (oldkeys)
          memcpy(conf->keys, oldkeys, sizeof(Key) * conf->keyslen-1);

        // add new Key to the end
        conf->keys[conf->keyslen - 1] = key;

        if (oldkeys)
          free(oldkeys);
        *ret = 0;

      // if button
      } else {
        conf->btnslen++;
        Key *oldbtns = conf->btns;
        conf->btns = malloc(sizeof(Key) * conf->btnslen);
        if (oldbtns)
          memcpy(conf->btns, oldbtns, sizeof(Key) * conf->btnslen-1);

        // add new Key(button) to the end
        conf->btns[conf->btnslen - 1] = key;

        if (oldbtns)
          free(oldbtns);
        *ret = 0;
      }

      bindkeys();

      free(*args);
      free(*cmd);
      free(*keybind);
      *args = NULL;
      *cmd = NULL;
      *keybind = NULL;

      remapwins();
    } else *cmd = str;
  } else {
    // clear bindings
    if (strcmp(str, "clear") == 0) {
      free(conf->keys);
      conf->keys = NULL;
      conf->keyslen = 0;
      *ret = 0;
      unbindkeys();
    } else {
      *keybind = str;
    }
  }
}

int handletoken(Token *token, char *str, char **keybind, char **cmd, char **args) {
  int ret = 1;

  if (*token != tok_none && *token != tok_bind) {
    // token will be parsed correctly
    ret = 0;
  }

  // handle the token types
  // (none, a keybind, or a setting)
  if (*token == tok_none) {
#define TOK(name) \
    if (strcmp(str, #name) == 0) { \
      *token = tok_##name; \
    }
    CONFIG_COMMANDS
#undef TOK
    
    // the above should have set the right token
    // if it couldn't get it then the token is invalid
    if (*token == tok_none) {
      printerr("invalid input\n");
      return 3;
    }
  } else if (*token == tok_bind) {
    // first time make a keybind, then cmd, then args
    handlekeybind(str, keybind, cmd, args, &ret);
  } else if (*token == tok_desktop_names) {

    int numofstr = splitlen(str, " \t\n\r");
    char *strptr = splitstring(str, " \t\n\r");
    char **desktopnames = malloc(sizeof(char *) * numofstr);

    int desktopslen = 0;
    for (int i = 0; i < numofstr; i++) {
      // allocates memory because "str" may be freed
      int strptrlen = strlen(strptr);
      desktopnames[i] = malloc(sizeof(char *) * (strptrlen + 1));
      memcpy(desktopnames[i], strptr, strptrlen);
      desktopnames[i][strptrlen] = '\0';
      desktopslen += strptrlen;
      strptr = splitstring(NULL, " \t\n\r");
    }
    conf->desktop_names = malloc(sizeof(char) * (desktopslen + numofstr + 1));

    printf("conf->desktop_names len = %d\n", (desktopslen + numofstr + 1));
    printf("desktopslen = %d\n", desktopslen);

    // copy into conf->desktop_names
    desktopslen = 0;
    for (int i = 0; i < numofstr; i++) {
      int strptrlen = strlen(desktopnames[i]);
      printf("desktopnames[%d] = [%s]\n", i, desktopnames[i]);
      printf("strptrlen = %d\n", strptrlen);
      strcpy(conf->desktop_names + desktopslen, desktopnames[i]);
      printf("conf->desktop_names[%d] = [%s]\n", desktopslen, conf->desktop_names + desktopslen);
      desktopslen += strptrlen;
      conf->desktop_names[desktopslen] = '\0';
      desktopslen++;
      printf("desktopslen = %d\n", desktopslen);
    }

    printf("conf->desktop_names = [%s]\n", conf->desktop_names);
    printf("desktopslen = %d\n", desktopslen);
    printf("numofstr = %d\n", numofstr);

    conf->desktop_names_len = desktopslen;
    setdesktops();

  } else {
    // strtol returns 0 if it fails to find a number
    // so random rubbish will just be 0 (and is also valid xd)
    switch (*token) {
      case tok_refresh_rate:
        conf->refreshrate = strtol(str, NULL, 10);
        break;
      case tok_split_ratio:
        conf->split_ratio = (strtol(str, NULL, 10))/100.0f;
        break;
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
        setdesktops();
        break;
      case tok_resize_amount:
        conf->resize_amount = (strtol(str, NULL, 10))/100.0f;
        break;
      case tok_move_amount:
        conf->move_amount = strtol(str, NULL, 10);
        break;
      case tok_minimum_size:
        conf->min_size = (strtol(str, NULL, 10));
        break;
      case tok_focused_border_color:
        conf->bord_foc_col = strtol(str, NULL, 16);
        break;
      case tok_normal_border_color:
        conf->bord_nor_col = strtol(str, NULL, 16);
        break;

      default: // just here to hide the "enumeration value not handled" warning
        ret = 5;
        break;
    }
  }

  // if it's not a bind then it's ok to free this memory
  if (*token != tok_bind) {
    free(str);
  }

  // if there was no error and this isn't a keybind then
  // flush (refresh) x11
  if (ret == 0 && *token != tok_bind) {
    remapwins();
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
  int ret = 1;

  // request data till the client sends an empty message or an error
  int done = 0;
  do {
    // first client sends size of message
    // then sends the message
    if (issize == 1) {
      size = getsize(s2, &done) + 1;
      if (size == 1) {
        done = 1;
      }
      issize = 0;
    } else {
      int datasize = getdata(s2, &done, size, &str);

      printf("nwmc command:\n[%s]\n", str);
      // if the datasize is not what the client sent before
      // tell client to send the data again
      if (datasize != size && datasize != -1) {
        sendret(s2, &done, 2);
        free(str);
      } else {
        sendret(s2, &done, 1);
        issize = 1;
        // if this condition is true that means the client is sending too many arguments
        // so set the return value to 2 (too many arguments)
        // and also don't bother parsing the data
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
    //printf("Waiting for a connection...\n");
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
