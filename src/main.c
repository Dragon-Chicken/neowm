#include "main.h"

// debug
void printerr(char *errstr) {
  fprintf(stderr, "%s: error: %s", WM_NAME, errstr);
}

void printpath(unsigned int path, int depth) {
  for (int i = 0; i < depth; i++) {
    if ((path & (1 << i)) == 1) {
      printf("1");
    } else {
      printf("0");
    }
  }
}

int printbsptree(Client *c) {
  if (!c) {
    printf("client is invalid\n");
    return 0;
  }

  printf("win: ");
  if (c == headc) {
    printf("id=root    , ");
  } else {
    //printf("id=%-8b, ", c->path);
    printf("id=");
    printpath(c->path, c->depth);
  }
  printf(" (%.8b), ", c->path);
  printf("win=%-8lx, d=%-4d", c->win, c->depth);
  /*if (c->a)
    printf(", a->path=%-8lb, a->win=%-8lx", c->a->path, c->a->win);
  if (c->a)
    printf(", b->path=%-8lb, b->win=%-8lx", c->b->path, c->b->win);*/
  printf("\n");

  return 1;
}


// helpers
char keysymtostring(XKeyEvent *xkey) {
  return *XKeysymToString(XLookupKeysym(xkey, 0));
}

int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype) {
  // Atom == unsigned long
  int di;
  unsigned long ni;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da;
  if (XGetWindowProperty(dpy, c->win, prop, 0L, retatomlen, False, proptype,
      &da, &di, &ni, &dl, &p) == Success && p) {
    for (unsigned long i = 0; ni == retatomlen && i <= ni; i++) {
      retatom[i] = ((Atom *)p)[i];
    }
    /**retatom = *(Atom *)p;*/
    XFree(p);
    return 1;
  }
  return 0;
}


// events
void voidevent(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(void event)\n");
#endif
}

void keypress(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(keypress)\n");
#endif

  XKeyEvent *xkey = &ev->xkey;
  KeySym keysym = XLookupKeysym(xkey, 0);
  for (int i = 0; i < conf.keyslen; i++) {
    if (keysym == (conf.keys)[i].keysym &&
        CLEANMASK(conf.keys[i].mod) == CLEANMASK(xkey->state) &&
        conf.keys[i].func) {
      conf.keys[i].func(&conf.keys[i].args);
    }
  }

  // only here to make sure it's always possible to exit wm
  if (keysymtostring(xkey) == 'q' && xkey->state == Mod1Mask) {
    fprintf(stderr, "%s: exiting with no errors\n", WM_NAME);
    exitwm(0);
  }
}

void maprequest(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(maprequest)\n");
#endif

  XMapRequestEvent *mapreq = &ev->xmaprequest;
  XWindowAttributes wa;
  if (!XGetWindowAttributes(dpy, mapreq->window, &wa) || wa.override_redirect) {
    return;
  }
  XSelectInput(dpy, mapreq->window, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
  manage(mapreq->window, &wa);
}

void unmapnotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(unmapnotify)\n");
#endif

  Window unmapwin = ev->xunmap.window;
  unmanage(unmapwin);
}

void destroynotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(destroynotify)\n");
#endif

  Window destroywin = ev->xdestroywindow.window;
  unmanage(destroywin);
}

void enternotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(enternotify)\n");
#endif

  if (ev->xcrossing.window == root)
    return;

  Client *c = findclient(headc, ev->xcrossing.window);
  if (c)
    setfocus(c);
  else
    printf("can't find client in enternotify\n");
}

void focusin(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(focusin)\n");
#endif

  if (focused && ev->xfocus.window != focused->win) {
    setfocus(focused);
  }
}


// client
Client *createclient(void) {
  Client *c = (Client *)malloc(sizeof(Client));
  c->a = NULL;
  c->b = NULL;
  c->p = NULL;
  c->win = root;
  c->path = 0;
  c->depth = 0;
  c->split = 50;
  c->x = 0;
  c->y = 0;
  c->w = screenw;
  c->h = screenh;
  return c;
}

void copyclientdata(Client *a, Client *b, Bool win, Bool path, Bool ab) {
  if (win) {
    a->win = b->win;
    a->x = b->x;
    a->y = b->y;
    a->w = b->w;
    a->h = b->h;
  }
  if (path) {
    a->path = b->path;
    a->depth = b->depth;
    a->p = b->p;
  }
  if (ab) {
    a->a = b->a;
    a->b = b->b;
  }
}

unsigned int findpath(unsigned int path, int depth, bool dir) {
  printf("finding path\n");

  if (dir == 0)
    depth = 32;

  if (!(depth & 1)) {
    depth--;
  }
  for (int i = depth - 1; i >= 0; i -= 2) {
    printf("path: %.8b\n", path);
    printf("i: %d\n", i);
    printf("1 << i: %.8b\n", 1 << i);
    if ((path >> i) & 1) {
      printf("path at i 1\n");
      path = path & ~(1 << i);
      if (!dir)
        break;
    } else {
      printf("path at i 0\n");
      path = path | (1 << i);
      if (dir)
        break;
    }
  }
  return path;
}

Client *findclientindir(Client *incl, int dir) {
  unsigned int destpath = 0;
  Client *cl;

  switch (dir) {
    case 0:
      destpath = findpath(incl->path, incl->depth, 1);
      break;
    case 1:
      destpath = findpath(incl->path, incl->depth, 0);
      break;
    case 2:
      destpath = findpath(incl->path >> 1, incl->depth - 1, 0);
      destpath <<= 1;
      destpath |= incl->path & 1;
      break;
    case 3:
      destpath = findpath(incl->path >> 1, incl->depth - 1, 1);
      destpath <<= 1;
      destpath |= incl->path & 1;
      break;
  }

  //printf("destpath: %.16b\n", destpath);

  if (!gototree(headc, &cl, destpath, 32, findclientpath)) {
    printf("can't find client\n");
    return NULL;
  }

  return cl;
}

int mapwins(Client *c) {
  /*c->w = screenw + swoff - (conf.hgaps*2) - (conf.bord_size*2);
  c->h = screenh + shoff - (conf.vgaps*2) - (conf.bord_size*2);*/

  //printf("mapping wins\n");

  //printf("path: %.16b\n", c->path);
  //printf("1 << c->depth-1: %.16b\n", 1 << (c->depth - 1));
  if (c->p) {
    c->w = c->p->w;
    c->h = c->p->h;
    c->x = c->p->x;
    c->y = c->p->y;
  } else {
    //printf("no parent\n");
    c->w = screenw + swoff - (conf.hgaps*2) - (conf.bord_size*2);
    c->h = screenh + shoff - (conf.vgaps*2) - (conf.bord_size*2);
    c->x = 0 + sxoff + conf.hgaps;
    c->y = 0 + syoff + conf.vgaps;
    return 0;
  }

  /*printf("c->p->split: %d\n", c->p->split);

  printf("c->w: %d\n", c->w);
  printf("(100.0f - c->p->split)/100.0f: %f\n", (100.0 - c->p->split)/100.0);
  printf("c->w * the above: %f\n", c->w * (100.0 - c->p->split)/100.0);*/

  if ((c->path & (1 << (c->depth - 1))) == 0) {
    if (c->depth & 1) {
      c->w *= (100.0 - c->p->split)/100.0;
      if (c->p->w * (100.0 - c->p->split)/100.0 > c->w)
        c->w += 1;
      c->w -= conf.vgaps/2 + conf.bord_size;

      c->x += (c->p->w * c->p->split) / 100.0;
      c->x += (conf.vgaps / 2) + conf.bord_size;
    } else {
      c->h *= (100.0 - c->p->split)/100.0;
      if (c->p->h * (100.0 - c->p->split)/100.0 > c->h)
        c->h += 1;
      c->h -= conf.hgaps/2 + conf.bord_size;

      c->y += (c->p->h * c->p->split) / 100.0;
      c->y += conf.hgaps/2 + conf.bord_size;
    }
  } else {
    if (c->depth & 1) {
      c->w *= (c->p->split)/100.0;
      c->w -= conf.vgaps/2 + conf.bord_size;
    } else {
      c->h *= (c->p->split)/100.0;
      c->h -= conf.hgaps/2 + conf.bord_size;
    }
  }

  /*printf("c->w: %d\n", c->w);
  printf("c->h: %d\n", c->h);
  printf("c->x: %d\n", c->x);
  printf("c->y: %d\n", c->y);*/

  //XSetWindowBorder(dpy, c->win, (c == focused ? conf.bord_foc_col : conf.bord_nor_col));

  /*if (c->win == root)
    return 1;*/

  //for (int i = 0; i < c->depth; i++) {
    // find size
    /*if ((i & 1) == 1) {
      c->h /= 2;
      c->h -= conf.vgaps/2 + conf.bord_size;
    } else {
      c->w /= 2;
      c->w -= conf.hgaps/2 + conf.bord_size;
    }*/

    //printf("c->path=%.8lb\n", c->path);
    //printf("c->depth=%d\n", c->depth);
    //printf("1UL << i=%.8lb\n", 1UL << i);
    
    // find pos
    /*if ((c->path & (1 << i)) == 0) {
      printf("0\n");
      if ((i & 1) == 0) {
        c->x += c->w + conf.hgaps + conf.bord_size*2;
      } else {
        c->y += c->h + conf.vgaps + conf.bord_size*2;
      }
    }*/
  //}
  return 0;
}

void manage(Window w, XWindowAttributes *wa) {
  if (!w) {
    printerr("window is null\n");
    return;
  }

  Client *newc = createclient();
  newc->win = w;

  newc->x = wa->x;
  newc->y = wa->y;
  newc->w = wa->width;
  newc->h = wa->height;
  //newc->split = 50;
  newc->p = headc;

  Atom wtype;
  if (getwinprop(newc, netatom[NetWMWindowType], &wtype, 1, XA_ATOM)) {
    if (wtype == netatom[NetWMWindowTypeDock]) {
      unsigned long strut[12];
      if (getwinprop(newc, netatom[NetWMStrutPartial], strut, LENGTH(strut), XA_CARDINAL)) {
        // left edge
        sxoff += strut[0];
        swoff -= strut[0];
        // right edge
        swoff -= strut[1];
        // top edge
        syoff += strut[2];
        shoff -= strut[2];
        // bottom edge
        shoff -= strut[3];
      }

      XMapWindow(dpy, newc->win);
      // may need to retile here?
      // yes, it needs to resize everything because the dock removed the space
      free(newc);
      return;
    }
  }

  XSetWindowBorderWidth(dpy, newc->win, conf.bord_size);

  if (focused == NULL) {
    printf("focused is null\n");
    newc->path = 0;
    newc->depth = 0;
  } else {
    printf("focused->depth:%d\n", focused->depth);
    newc->path = focused->path;
    newc->depth = focused->depth;

    if (focused->p) {
      newc->p = focused->p;
    }
  }

  if (gototree(headc, &newc, newc->path, newc->depth, addtotree)) {
    printf("added to tree?\n");
    focused = newc;
    looptree(headc, mapwins);
    looptree(headc, updateborders);
    looptree(headc, printbsptree);
    looptree(headc, drawwindows);
  } else {
    printf("failed to add to tree\n");
  }
}

int fixchildren(Client *c) {
  // this is for a special case in unmanage()

  if (!c)
    return 0;
  
  if (c->a) {
    c->a->depth--;
    c->a->path = c->path | (1 << (c->a->depth - 1));
    c->a->p = c;
  }
  if (c->b) {
    c->b->depth--;
    c->b->path = c->path & ~(1 << (c->b->depth - 1));
    c->b->p = c;
  }

  return 1;
}

void unmanage(Window w) {
  // this function has bugs!
  // may be fixed with the function above (fixchildren)?

  if (!w)
    return;

  //printbsptree(headc);

  Client *c = findclient(headc, w);
  if (!c)
    return;

  printf("found client: ");
  printpath(c->path, c->depth);
  printf("\n");

  // to delete all clients
  if (c == headc) {
    printf("deleted head\n");
    free(headc);
    headc = createclient();
    focused = NULL;
    return;
  }

  // can't delete this node if it has both children
  if (c->a && c->b) {
    printerr("client is invalid (has both children)\n");
    return;
  }

  Client *prevc = c->p;
  /*printf("c->path %lb\n", c->path);
  printf("c->depth %d\n", c->depth);
  printf("c->path >> 1 %lb\n", c->path >> 1);
  printf("c->depth - 1 %d\n", c->depth - 1);*/
  /*if (!gototree(headc, &prevc, c->path, c->depth - 1, findclientpath)) {
    printf("can't find prevc\n");
    return;
  }*/

  if (!prevc) {
    printf("c doesn't have parent?\n");
    return;
  }
  /*printf("pass 0\n");
  printbsptree(headc);
  printf("prevc win:%lx id:%lb\n", prevc->win, prevc->path);*/

  printf("prevc: ");
  printpath(prevc->path, prevc->depth);
  printf("\n");

  if (c == prevc) {
    printf("##########\nc == prevc (this is an error state in unmanage)\n##########\n");
    return;
  }
  //printf("pass -1\n");

  /*if (prevc->a == NULL)
    printf("!prevc->a\n");
  if (prevc->b == NULL)
    printf("!prevc->b\n");*/

  if (!prevc->a && !prevc->b)
    return;
  //printf("pass 1\n");

  if (focused == prevc->a || focused == prevc->b)
    focused = prevc;
  //printf("pass 2\n");

  if (prevc->a == c) {
    copyclientdata(prevc, prevc->b, True, False, True);
  } else {
    copyclientdata(prevc, prevc->a, True, False, True);
  }


  looptree(prevc, fixchildren);
  printf("freeing c\n");
  free(c);
  printf("mapping wins (after deletion)\n");
  looptree(headc, mapwins);
  looptree(headc, drawwindows);
  looptree(headc, printbsptree);
  /*printf("afterfree\n");
  printbsptree(headc);*/
}


// bsp
int looptree(Client *c, int (*func)(Client *)) {
  if (!c)
    return 0;

  func(c);

  if (c->a)
    looptree(c->a, func);
  if (c->b)
    looptree(c->b, func);
  return 0;
}

Client *findclient(Client *c, Window win) {
  Client *retc = NULL;
  if (c->win == win) {
    return c;
  } else {
    if (c->a)
      retc = findclient(c->a, win);
    if (c->b && !retc)
      retc = findclient(c->b, win);
  }
  return retc;
}

int findclientpath(Client *c, Client **retc) {
  *retc = c;
  return 1;
}

int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **)) {
  if (depth < 0) {
    printf("depth < 0\n");
    return 0;
  }
  if (!c) {
    printf("c is null\n");
    return 0;
  }
  if (depth <= 0 || !c->a || !c->b) {
    //printf("running func\n");
    return func(c, retc);
  }


  if (path & 1)
    return gototree(c->a, retc, path >> 1, depth - 1, func);
  else
    return gototree(c->b, retc, path >> 1, depth - 1, func);
}

int addtotree(Client *c, Client **newc) {
  if (!c) {
    printf("c is null\n");
    return 0;
  }
  if (c->win != root) {
    printf("c != root\n");
    c->b = *newc;
    c->a = createclient();
    copyclientdata(c->a, c, True, True, False);
    c->win = root;
    c->a->depth++;
    c->b->depth++;
    c->a->path = c->a->path | (1 << c->depth);
    c->b->path = c->b->path & ~(1 << c->depth);
    if (c == focused)
      focused = *newc;
    printf("c->a win:%lx path:%b\n", c->a->win, c->a->path);
    printf("c->b win:%lx path:%b\n", c->b->win, c->b->path);
    c->a->p = c;
    c->b->p = c;
  } else {
    printf("c == root\n");
    copyclientdata(c, *newc, True, True, False);
    free(*newc);
    c->p = NULL;
    *newc = c; // should prob do this because it's freeing memory that does NOT belong to this function
  }
  return 1;
}


// keypress
int focusswitch(Arg *arg) {
  /*printf("focus dir: %d\n", arg->i);
  printf("focused path: %.16b or ", focused->path);
  printpath(focused->path, focused->depth);
  printf("\n");
  printf("focused depth: %d\n", focused->depth);*/

  if (!focused->p) {
    printf("focused doesn't have a parent\n");
    return 0;
  }

  focused = findclientindir(focused, arg->i);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));

  looptree(headc, updateborders);
  return 1;
}

int resizeclient(Arg *arg) {
  int dir = arg->i;

  /*printf("resizing client\n");
  printf("dir = %d\n", dir);*/

  if (!focused->p) {
    printf("focused doesn't have a parent\n");
    return 0;
  }

  Client *ca = focused->p->a;
  Client *cb = focused->p->b;

  if (((dir >= 2) && (ca->x == cb->x)) ||
      ((dir <= 1) && (ca->y == cb->y))) {
    Client *tempfoc = focused;
    focused = focused->p;
    if (resizeclient(arg)) {
      XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
      focused = tempfoc;
      return 1;
    }
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
    focused = tempfoc;
  }

  if (dir >= 2)
    dir -= 2;

  ca->p->split += conf.resize_amount * ((dir * 2) - 1);

  if (ca->p->split >= 90) {
    ca->p->split = 90;
  } else if (ca->p->split <= 10) {
    ca->p->split = 10;
  }

  looptree(headc, mapwins);
  looptree(headc, drawwindows);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
  return 1;
}

/*this code needs to be worked on
 *there is some posix stuff that dwm does that this code DOES NOT DO
 *god know if this is fixed ^
 *prob fixed*/
int spawn(Arg *arg) {
#ifdef NWM_DEBUG
  printf("spawning: %s\n", arg->s[0]);
#endif

  struct sigaction sa;

  if (fork() == 0) {
    // close connection to the x server for the child
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(arg->s[0], arg->s);
    exit(0); // kills child process
  }

  return 1;
}

int killfocused(Arg *arg) {
  if (!focused)
    return 0;

  if (!sendevent(focused, wmatom[WMDelete])) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, focused->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }

  return 1;
}

int exitwm(Arg *arg) {
  XCloseDisplay(dpy);
  exit(arg->i);
  return 1;
}


// x11
int sendevent(Client *c, Atom proto) {
  int n;
  Atom *protocols;
  int exists = 0;
  XEvent ev;

  if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
    while (!exists && n--)
      exists = protocols[n] == proto;
    XFree(protocols);
  }

  if (exists) {
    ev.type = ClientMessage;
    //ev.xclient.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, c->win, False, NoEventMask, &ev);
  }

  return exists;
}

void setfocus(Client *c) {
  if (!c) {
    return;
  }
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  focused = c; // make sure focus is set
  XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
  XChangeProperty(dpy, root, netatom[NetActiveWindow],
      XA_WINDOW, 32, PropModeReplace,
      (unsigned char *) &(c->win), 1);
  sendevent(c, wmatom[WMTakeFocus]); // god know what this does
  looptree(headc, updateborders);
}

int updateborders(Client *c) {
  if (!c)
    return 0;

  XSetWindowBorder(dpy, c->win, (c == focused ? conf.bord_foc_col : conf.bord_nor_col));
  return 1;
}

int drawwindows(Client *c) {
  if (!c)
    return 0;

  if (c->win != root) {
    /*printf("win: path=%-8lb, win=%-8lx, x=%-4d, y=%-4d, w=%-4d, h=%-4d\n",
        c->path, c->win, c->x, c->y, c->w, c->h);*/
    XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    XMapWindow(dpy, c->win);
    return 1;
  }
  return 0;
}


// others
void setup(void) {
  int screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  screenw = XDisplayWidth(dpy, screen);
  screenh = XDisplayHeight(dpy, screen);
  // for docks/bars
  sxoff = 0;
  syoff = 0;

  // https://tronche.com/gui/x/xlib/events/processing-overview.html
  XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|FocusChangeMask|EnterWindowMask|PropertyChangeMask);

  headc = createclient();
  focused = NULL;

  for (int i = 0; i < LASTEvent; i++)
    handler[i] = voidevent;
  handler[KeyPress] = keypress;
  handler[MapRequest] = maprequest;
  handler[DestroyNotify] = destroynotify;
  handler[UnmapNotify] = unmapnotify;
  handler[EnterNotify] = enternotify;
  handler[FocusIn] = focusin;

  // temp
  conf = (Config){
    .vgaps = 20,
    .hgaps = 20,
    .bord_size = 4,
    .bord_foc_col = 0xffc4a7e7L,
    .bord_nor_col = 0xff26233aL,
    .resize_amount = 4,
    .keyslen = 13,
  };
  conf.keys = malloc(sizeof(Key) * conf.keyslen);

  conf.keys[0] = (Key){Mod1Mask, XStringToKeysym("q"), exitwm, {0}};

  char **arg = malloc(sizeof(char *) * 2);
  arg[0] = "st";
  arg[1] = NULL;
  conf.keys[1] = (Key){Mod1Mask, XStringToKeysym("a"), spawn, {.s = arg}};

  conf.keys[2] = (Key){Mod1Mask, XStringToKeysym("x"), killfocused, {0}};

  arg = malloc(sizeof(char *) * 4);
  arg[0] = "rofi";
  arg[1] = "-show";
  arg[2] = "drun";
  arg[3] = NULL;
  conf.keys[3] = (Key){Mod1Mask, XStringToKeysym("s"), spawn, {.s = arg}};

  arg = malloc(sizeof(char *) * 2);
  arg[0] = "polybar";
  arg[1] = NULL;
  conf.keys[4] = (Key){Mod1Mask, XStringToKeysym("d"), spawn, {.s = arg}};

  conf.keys[5] = (Key){Mod1Mask, XStringToKeysym("h"), focusswitch, {0}};
  conf.keys[6] = (Key){Mod1Mask, XStringToKeysym("l"), focusswitch, {1}};
  conf.keys[7] = (Key){Mod1Mask, XStringToKeysym("j"), focusswitch, {2}};
  conf.keys[8] = (Key){Mod1Mask, XStringToKeysym("k"), focusswitch, {3}};

  conf.keys[9]  = (Key){Mod1Mask|ShiftMask, XStringToKeysym("h"), resizeclient, {2}};
  conf.keys[10] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("j"), resizeclient, {1}};
  conf.keys[11] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("k"), resizeclient, {0}};
  conf.keys[12] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("l"), resizeclient, {3}};

  // grab input
  for (int i = 0; i < conf.keyslen; i++) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, conf.keys[i].keysym), conf.keys[i].mod,
          root, True, GrabModeAsync, GrabModeAsync);
  }

  // startup script
  arg = malloc(sizeof(char *) * 2);
  arg[0] = "/home/ethan/neowm/startup";
  arg[1] = NULL;
  spawn(&(Arg){.s = arg}); // temp
}

void setupatoms(void) {
  Atom utf8string;
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);

  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);

  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMStrutPartial] = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);

  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeNormal] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
  netatom[NetWMWindowTypePopup] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);

  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMStateAbove] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);

  Window WmCheckWin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char*) &WmCheckWin, 1);
  XChangeProperty(dpy, WmCheckWin, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char*) &WmCheckWin, 1);
  XChangeProperty(dpy, WmCheckWin, netatom[NetWMName], utf8string, 8,
      PropModeReplace, (unsigned char*) WM_NAME, strlen(WM_NAME));

  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
      PropModeReplace, (unsigned char *) netatom, NetLast);
}

int xerror(Display *dpy, XErrorEvent *ee) {
  // from dwm
  switch (ee->error_code) {
    case BadWindow:
      printerr("BadWindow\n");
      return 0;
  }
  fprintf(stderr, "%s: fatal error: request code=%d, error code=%d\n",
      WM_NAME, ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee);
}

int xerrordummy(Display *dpy, XErrorEvent *ee) {
  return 0;
}

int main() {
  XEvent ev;

  if (!(dpy = XOpenDisplay(NULL))) {
    printerr("failed to open display\n");
    exitwm(0);
  }
  //printf("Default screen: %d\nScreen width: %d\nScreen height: %d\n", screen, screenw, screenh);

  setup();
  setupatoms();
  xerrorxlib = XSetErrorHandler(xerror);

  for (;;) {
    XNextEvent(dpy, &ev);
#ifdef NWM_DEBUG
    printf("event rec of type %d ", ev.type);
#endif

    handler[ev.type](&ev);
  }

  printerr("exiting with no errors\n");
  exitwm(0);
  return 0;
}
