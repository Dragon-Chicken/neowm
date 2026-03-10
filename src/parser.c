#include "parser.h"

char *strp(char *instr) {
  int whitespaces = 0;
  for (int i = 0; i < strlen(instr); i++) {
    if ((instr[i] >= 9 && instr[i] <= 13) || instr[i] == 32 || instr[i] == ';') {
      whitespaces++;
    }
  }

  char *newstr = malloc(sizeof(char) * (strlen(instr) - whitespaces));
  int newstri = 0;
  for (int i = 0; i < strlen(instr); i++) {
    if ((instr[i] >= 9 && instr[i] <= 13) || instr[i] == 32 || instr[i] == ';') {
    } else {
      newstr[newstri] = instr[i];
      newstri++;
    }
  }

  newstr[strlen(instr) - whitespaces] = 0;

  return newstr;
}

Bool iswhitespace(char ch) {
  if ((ch >= 9 && ch <= 13) || ch == 32) {
    return True;
  }
  return False;
}

Config *parseconf(char *path, int len) {

  FILE *fp;

  fp = fopen("nwm.conf", "r");
  if (!fp) {
    return NULL;
  }

  fseek(fp, 0L, SEEK_END);
  long filesize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  char *file = malloc(sizeof(char) * filesize);

  for (int i = 0; i < filesize; i++) {
    file[i] = getc(fp);
  }
  fclose(fp);

  char *token = NULL;
  int tokenstart = 0;
  Bool intoken = False;

  char *lasttok = NULL;

  for (int i = 0; i < filesize; i++) {
    if (intoken == False) {
      if (!iswhitespace(file[i])) {
        intoken = True;
        tokenstart = i;
      }
    } else if (iswhitespace(file[i])) {
      intoken = False;

      if (lasttok)
        free(lasttok);
      if (token)
        lasttok = token;

      token = malloc(sizeof(char) * (i - tokenstart + 1));
      memcpy(token, file + tokenstart, i - tokenstart + 1);
      token[i - tokenstart + 1] = 0;

      char *tofree = token;
      token = strp(token);
      free(tofree);

      printf("[%s]\n", token);
    }
  }

  /*char *lines[9];
  int linesi = 0;

  int linestart = 0;
  for (int i = 0; i < filesize; i++) {
    if (file[i] == ';') {
      if (linesi >= 9) {
        break;
      }
      lines[linesi] = malloc(sizeof(char) * (i - linestart + 1));
      for (int x = linestart; x <= i; x++) {
        lines[linesi][x - linestart] = file[x];
      }
      lines[linesi][i - linestart + 1] = '\0';
      linesi++;
      linestart = i;
    }
  }

  for (int l = 0; l < 10; l++) {
    printf("[%s]\n", lines[l]);
    char *tokens[3];
    int tokensi = 0;

    int tokenstart = 0;
    for (int i = 0; i <= strlen(lines[l]); i++) {
      if ((lines[l][i] >= 9 && lines[l][i] <= 13) || lines[l][i] == 32 || lines[l][i] == ';') {
        if (tokensi >= 3) {
          break;
        }
        tokens[tokensi] = malloc(sizeof(char) * (i - tokenstart + 1));
        for (int x = tokenstart; x <= i; x++) {
          tokens[tokensi][x - tokenstart] = lines[l][x];
        }
        tokens[tokensi][i - tokenstart + 1] = '\0';
        char *tofree = tokens[tokensi];
        tokens[tokensi] = strp(tokens[tokensi]);
        free(tofree);
        tokensi++;
        tokenstart = i;
      }
    }

    printf("{'%s', '%s', '%s'}\n", tokens[0], tokens[1], tokens[2]);

  }*/


  /*printf("%s", file);
  printf("\n");*/

  exitwm(NULL);

  // read conf file
  // split into lines (each ends with ;)
  // split conf into sections (settings and binds)
  // remove comments
  // split by :
  //    for settings section
  //    each setting is in the form `keyword : value`
  //
  //    for binds section
  //    each bind is in the form `keybind : action : args`
  // do whatver is needed to allocate
  // close conf file
  // return conf

  Config *conf = malloc(sizeof(Config));

  *conf = (Config){
    .vgaps = 20,
    .hgaps = 20,
    .bord_size = 4,
    .bord_foc_col = 0xffc4a7e7L,
    .bord_nor_col = 0xff26233aL,
    .num_of_desktops = 2,
    .resize_amount = 4,
    .keyslen = 15,
  };
  conf->keys = malloc(sizeof(Key) * conf->keyslen);

  conf->keys[0] = (Key){Mod1Mask, XStringToKeysym("q"), exitwm, {0}};

  char **arg = malloc(sizeof(char *) * 2);
  arg[0] = "st";
  arg[1] = NULL;
  conf->keys[1] = (Key){Mod1Mask, XStringToKeysym("a"), spawn, {.s = arg}};

  conf->keys[2] = (Key){Mod1Mask, XStringToKeysym("x"), killfocused, {0}};

  arg = malloc(sizeof(char *) * 4);
  arg[0] = "rofi";
  arg[1] = "-show";
  arg[2] = "drun";
  arg[3] = NULL;
  conf->keys[3] = (Key){Mod1Mask, XStringToKeysym("s"), spawn, {.s = arg}};

  arg = malloc(sizeof(char *) * 2);
  arg[0] = "polybar";
  arg[1] = NULL;
  conf->keys[4] = (Key){Mod1Mask, XStringToKeysym("d"), spawn, {.s = arg}};

  conf->keys[5] = (Key){Mod1Mask, XStringToKeysym("h"), focusswitch, {0}};
  conf->keys[6] = (Key){Mod1Mask, XStringToKeysym("l"), focusswitch, {1}};
  conf->keys[7] = (Key){Mod1Mask, XStringToKeysym("j"), focusswitch, {2}};
  conf->keys[8] = (Key){Mod1Mask, XStringToKeysym("k"), focusswitch, {3}};

  conf->keys[9]  = (Key){Mod1Mask|ShiftMask, XStringToKeysym("h"), resizeclient, {2}};
  conf->keys[10] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("j"), resizeclient, {1}};
  conf->keys[11] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("k"), resizeclient, {0}};
  conf->keys[12] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("l"), resizeclient, {3}};

  conf->keys[13] = (Key){Mod1Mask, XStringToKeysym("1"), focusdesktop, {0}};
  conf->keys[14] = (Key){Mod1Mask, XStringToKeysym("2"), focusdesktop, {1}};

  return conf;
}
