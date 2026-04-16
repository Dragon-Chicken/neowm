#include "main.h"
#include "config.h"

Atom wmatom[WMLast];
Atom netatom[NetLast];
Atom utf8string;

Cursor cursor;

Window *wins;
Desktop *desktops;
long deski;
int old_num_of_desktops;
Config *conf;
Display *dpy;
Window root;

// for net client list ewmh
int totalwins = 0;

int screenw, screenh;

// for bars and stuff
int sxoff, syoff;
int swoff, shoff;

// mouse stuff
int mousex, mousey;
int mxoff, myoff;

int mousedrag = 0;
int ignoreenter = 0;
Time lasttime = 0;

void (*handler[LASTEvent])(XEvent*);
int (*xerrorxlib)(Display *, XErrorEvent *);

// debug
#ifdef NWM_DEBUG
void printpath(unsigned int path, int depth) {
  //printf("printpath\n");
  for (int i = 0; i < depth; i++) {
    if ((path & (1 << i)) == 1) {
      printf("1");
    } else {
      printf("0");
    }
  }
}

int printbsptree(Client *c) {
  printf("printbsptree\n");
  if (!c) {
    printf("client is invalid\n");
    return FAIL;
  }

  printf("win: ");
  if (c == desktops[deski].headc) {
    printf("id=root    , ");
  } else {
    //printf("id=%-8b, ", c->path);
    printf("id=");
    printpath(c->path, c->depth);
  }
  printf(" (%.8b), ", c->path);
  printf("win=%-8lx, d=%-4d", c->win, c->depth);
  if (c->a)
    printf(", a->path=%-8b, a->win=%-8lx", c->a->path, c->a->win);
  if (c->a)
    printf(", b->path=%-8b, b->win=%-8lx", c->b->path, c->b->win);
  if (c->p)
    printf(", p->path=%-8b, p->win=%-8lx", c->p->path, c->p->win);
  printf("\n");

  return SUC;
}

void printll(Client *c) {
  printf("printll\n");
  while (c) {
    printf("win: ");
    printf("win=%-8lx, d=%-4d", c->win, c->depth);
    printf("\n");
    c = c->a;
  }
}
#endif


// helpers
void printerr(char *errstr) {
#ifdef NWM_DEBUG
  printf("printerr\n");
#endif
  //fprintf(stderr, "%s: error: %s", WM_NAME, errstr);
  printf("%s: error: %s", WM_NAME, errstr);
}

int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype) {
#ifdef NWM_DEBUG
  printf("getwinprop\n");
#endif

  // Atom == unsigned long
  int format;
  unsigned long nitems;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da;

  // XGetWindowProperty returns 
  if (XGetWindowProperty(dpy, c->win, prop, 0L, retatomlen, False, proptype,
      &da, &format, &nitems, &dl, &p) == Success && p) {

    if (nitems == 0)
      return FAIL;

    for (unsigned long i = 0; i <= nitems; i++)
      retatom[i] = ((Atom *)p)[i];

    XFree(p);
    return SUC;
  }
  return FAIL;
}

void remapwins(void) {
#ifdef NWM_DEBUG
  printf("remapwins\n");
#endif
  looptree(desktops[deski].headc, tilewins);
  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, updateborders);
  loopll(desktops[deski].floating, updateborders);
  XSync(dpy, False); // updates x11

  printf("done\n");
}

void bindkeys(void) {
#ifdef NWM_DEBUG
  printf("bindkeys\n");
#endif

  for (int i = 0; i < conf->keyslen; i++) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, conf->keys[i].keysym), conf->keys[i].mod,
          root, True, GrabModeAsync, GrabModeAsync);
  }

  for (int i = 0; i < conf->num_of_desktops; i++) {
    looptree(desktops[i].headc, grabbuttons);
    loopll(desktops[i].floating, grabbuttons);
  }
}

void unbindkeys(void) {
#ifdef NWM_DEBUG
  printf("unbindkeys\n");
#endif

  for (int i = 0; i < conf->keyslen; i++) {
    XUngrabKey(dpy, XKeysymToKeycode(dpy, conf->keys[i].keysym), conf->keys[i].mod, root);
  }


  for (int i = 0; i < conf->num_of_desktops; i++) {
    looptree(desktops[i].headc, ungrabbuttons);
    loopll(desktops[i].floating, ungrabbuttons);
  }
}

int grabbuttons(Client *c) {
#ifdef NWM_DEBUG
  printf("grabbuttons\n");
#endif

  if (conf->btns) {
    for (int i = 0; i < conf->btnslen; i++) {
      XGrabButton(dpy, conf->btns[i].keysym, conf->btns[i].mod, c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
    }
  }

  return SUC;
}

int ungrabbuttons(Client *c) {
#ifdef NWM_DEBUG
  printf("ungrabbuttons\n");
#endif

  if (conf->btns) {
    for (int i = 0; i < conf->btnslen; i++) {
      XUngrabButton(dpy, conf->btns[i].keysym, conf->btns[i].mod, c->win);
    }
  }

  return SUC;
}

void setdesktops(void) {
#ifdef NWM_DEBUG
  printf("setdesktops\n");
#endif

  /*printf("num of desktops = %d\n", conf->num_of_desktops);
  printf("old num of desktops = %d\n", old_num_of_desktops);*/

  if (conf->num_of_desktops != old_num_of_desktops) {

    if (conf->num_of_desktops < 1)
      conf->num_of_desktops = 1;

    Desktop *newdesktops = malloc(sizeof(Desktop) * conf->num_of_desktops);

    if (desktops) {
      if (conf->num_of_desktops > old_num_of_desktops) {
        //printf("conf->num_of_desktops > old_num_of_desktops\n");
        memcpy(newdesktops, desktops, sizeof(Desktop) * old_num_of_desktops);
        //printf("?\n");
        for (int i = old_num_of_desktops; i < conf->num_of_desktops; i++) {
          newdesktops[i].headc = createclient();
          newdesktops[i].floating = NULL;
          newdesktops[i].focused = NULL;
          newdesktops[i].tilefoc = NULL;
          newdesktops[i].active = 0;
        }
        //printf("?\n");
      } else {
        //printf("NOT > conf->num_of_desktops > old_num_of_desktops\n");
        memcpy(newdesktops, desktops, sizeof(Desktop) * conf->num_of_desktops);
      }

      free(desktops);
    }

    old_num_of_desktops = conf->num_of_desktops;
    desktops = newdesktops;

    //printf("done\n");

    //focusdesktop(0);
  }

  /*for (int i = 0; i < conf->num_of_desktops; i++) {
    if (desktops[i].headc)
      printf("desktops[i].headc->win = %lx\n", desktops[i].headc->win);
  }

  printf("what\n");

  printf("num of desktops = %d\n", conf->num_of_desktops);

  printf("desktop names len = %d\n", conf->desktop_names_len);*/

  /*XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
      PropModeReplace, (const unsigned char *)&conf->num_of_desktops, 1);
  XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
      PropModeReplace, (const unsigned char *)&deski, 1);*/
  XChangeProperty(dpy, root, netatom[NetDesktopNames], utf8string, 8,
      PropModeReplace, (const unsigned char *)conf->desktop_names, conf->desktop_names_len);

  XSync(dpy, False); // updates x11

  //printf("done 2\n");
}

void warptowin(Client *c) {
#ifdef NWM_DEBUG
  printf("warptowin\n");
#endif
  if (c) {
    ignoreenter = 1;
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, c->x + (c->w/2), c->y + (c->h/2));
  }
}


// events
void voidevent(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(void event)\n");
#endif
  (void)ev;
}

void keypress(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(keypress)\n");
#endif

  XKeyEvent *xkey = &ev->xkey;
  KeySym keysym = XLookupKeysym(xkey, 0);
  for (int i = 0; i < conf->keyslen; i++) {
    if (keysym == (conf->keys)[i].keysym &&
        CLEANMASK(conf->keys[i].mod) == CLEANMASK(xkey->state) &&
        conf->keys[i].func) {
      conf->keys[i].func(&conf->keys[i].args);
    }
  }
}

void buttonpress(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(buttonpress)\n");
#endif
  XButtonEvent *xbtn = &ev->xbutton;

  mousex = xbtn->x_root;
  mousey = xbtn->y_root;
  mxoff = 0;
  myoff = 0;

  XAllowEvents(dpy, ReplayPointer, xbtn->time);

  for (int i = 0; i < conf->btnslen; i++) {
    if (xbtn->button == (conf->btns)[i].keysym &&
        CLEANMASK(conf->btns[i].mod) == CLEANMASK(xbtn->state) &&
        conf->btns[i].func) {
      if (conf->btns[i].func == dragmovewindow) {
        mousedrag = 1;
      } else if (conf->btns[i].func == dragresizewindow) {
        mousedrag = 2;
      } else {
        conf->btns[i].func(&conf->btns[i].args);
      }
    }
  }

  XGrabPointer(dpy, root, True, ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
}

void buttonrelease(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(buttonrelease)\n");
#endif
  (void)ev;
  //XButtonEvent *xbtn = &ev->xbutton;


  XUngrabPointer(dpy, CurrentTime);
  mousedrag = 0;

  mxoff = 0;
  myoff = 0;

  /*mxoff = xbtn->x_root - mousex;
  myoff = xbtn->y_root - mousey;*/

}

void motionnotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(motionnotify)\n");
#endif
  if (mousedrag == 0) {
    return;
  }

  XMotionEvent *xmot = &ev->xmotion;

  if ((xmot->time - lasttime) <= (1000 / 180))
    return;
  lasttime = xmot->time;

  mxoff = xmot->x_root - mousex;
  myoff = xmot->y_root - mousey;
  mousex = xmot->x_root;
  mousey = xmot->y_root;

  Arg arg = {.i = 1};

  if (mousedrag == 1)
    dragmovewindow(&arg);
  if (mousedrag == 2)
    dragresizewindow(&arg);
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
  manage(mapreq->window);
}

void unmapnotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(unmapnotify)\n");
#endif

// NEED TO FIND A WAY TO TELL IF THE UNMAPNOTIFY EVENT WAS SENT BY THE WM OR NOT
// THIS IS NEEDED FOR focusdesktop()
// I think this is fixed?

  Window unmapwin = ev->xunmap.window;
  unmanage(unmapwin);
}

void destroynotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(destroynotify)\n");
#endif

  //printf("desktops[deski].focused->win: %lx\n", desktops[deski].focused->win);

  Window destroywin = ev->xdestroywindow.window;
  unmanage(destroywin);
}

void enternotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(enternotify)\n");
#endif

  if (ev->xcrossing.window == root)
    return;

  if (desktops[deski].focused && ev->xcrossing.window == desktops[deski].focused->win)
    return;

  if (ignoreenter) {
    ignoreenter = 0;
    return;
  }

  Client *c = findclient(desktops[deski].headc, ev->xcrossing.window);
  if (!c) {
    // find client in floating linked list
    c = findclientll(desktops[deski].floating, ev->xcrossing.window);
  }

  if (c)
    setfocus(c);
#ifdef NWM_DEBUG
  else
    printf("can't find client in enternotify\n");
#endif
}

void focusin(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(focusin)\n");
#endif

  if (ev->xfocus.window == root) {
    return;
  }

  // don't allow window to take focus
  if (desktops[deski].focused && ev->xfocus.window != desktops[deski].focused->win) {
    setfocus(desktops[deski].focused);
  }
}

void clientmessage(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(clientmessage)\n");
#endif

  XClientMessageEvent *xclient = &ev->xclient;

  // polybar sends these
  if (xclient->message_type == netatom[NetCurrentDesktop]) {
    //printf("new desktop = %ld\n", xclient->data.l[0]);
    focusdesktop(&(Arg){xclient->data.l[0]});
  }
}


// client
Client *createclient(void) {
#ifdef NWM_DEBUG
  printf("createclient\n");
#endif
  Client *c = (Client *)malloc(sizeof(Client));
  c->a = NULL;
  c->b = NULL;
  c->p = NULL;
  c->win = root;
  c->path = 0;
  c->depth = 0;
  c->split = 0.5f;
  c->x = 0;
  c->y = 0;
  c->w = screenw;
  c->h = screenh;
  c->minw = 0;
  c->minh = 0;
  c->maxw = screenw;
  c->maxh = screenh;
  c->floating = False;
  return c;
}

void copyclient(Client *a, Client *b, Bool win, Bool pos, Bool path, Bool ab) {
#ifdef NWM_DEBUG
  printf("copyclient\n");
#endif
  if (win) {
    a->win = b->win;
    a->minw = b->minw;
    a->minh = b->minh;
    a->maxw = b->maxw;
    a->maxh = b->maxh;
  }
  if (pos) {
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
    a->split = b->split;
  }
}

unsigned int findpath(unsigned int path, int depth, Bool dir) {
#ifdef NWM_DEBUG
  printf("finding path\n");
#endif

  if (dir == 0)
    depth = 32;
  if (!(depth & 1))
    depth--;

  for (int i = depth - 1; i >= 0; i -= 2) {
    /*printf("path: %.8b\n", path);
    printf("i: %d\n", i);
    printf("1 << i: %.8b\n", 1 << i);*/
    if ((path >> i) & 1) {
      //printf("path at i 1\n");
      path = path & ~(1 << i);
      if (!dir)
        break;
    } else {
      //printf("path at i 0\n");
      path = path | (1 << i);
      if (dir)
        break;
    }
  }

  return path;
}

Client *findclientindir(Client *incl, enum Dir dir) {
#ifdef NWM_DEBUG
  printf("findclientdir\n");
#endif

  unsigned int destpath = 0;
  Client *cl;

  switch (dir) {
    case dirup:
      destpath = findpath(incl->path >> 1, incl->depth - 1, 1);
      destpath <<= 1;
      destpath |= incl->path & 1;
      break;
    case dirdown:
      destpath = findpath(incl->path >> 1, incl->depth - 1, 0);
      destpath <<= 1;
      destpath |= incl->path & 1;
      break;
    case dirleft:
      destpath = findpath(incl->path, incl->depth, 1);
      break;
    case dirright:
      destpath = findpath(incl->path, incl->depth, 0);
      break;
  }

  // makes it go to the a node rather than b node
  // have to fix eventually
  //
  //printf("path = %.16b\n", incl->path);
  //printf("path = %.16b\n", destpath);
  //destpath |= 0b1 << incl->depth;
  //printf("path = %.16b\n", destpath);



  //exitwm(0);

  if (!gototree(desktops[deski].headc, &cl, destpath, 32, findclientpath)) {
    printerr("can't find client\n");
    return NULL;
  }

  return cl;
}

long getsizehints(Client *newc) {
  XSizeHints sizehints;
  long supret;
  if(!XGetWMNormalHints(dpy, newc->win, &sizehints, &supret)) {
    return FAIL;
  }

  if (supret & PSize) {
    newc->w = sizehints.base_width;
    newc->h = sizehints.base_height;
  }
  if (supret & PMinSize) {
    newc->minw = sizehints.min_width;
    newc->minh = sizehints.min_height;
  }
  if (supret & PMaxSize) {
    newc->maxw = sizehints.max_width;
    newc->maxh = sizehints.max_height;
  }

  return supret;
}

int fixchildren(Client *c) {
#ifdef NWM_DEBUG
  printf("fixchildren\n");
#endif

  if (!c)
    return FAIL;

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

  return SUC;
}

int addtoclientlist(Client *c, Window *wins, int numofwins) {
#ifdef NWM_DEBUG
  printf("addtoclientlist\n");
#endif

  if (!c) {
    return numofwins;
  }
  if (c->win != root) {
    wins[numofwins] = c->win;
    numofwins++;
  }

  if (c->a)
    numofwins = addtoclientlist(c->a, wins, numofwins);
  if (c->b)
    numofwins = addtoclientlist(c->b, wins, numofwins);

  return numofwins;
}

void createclientlist(void) {
#ifdef NWM_DEBUG
  printf("createclientlist\n");
#endif
  //Window wins[totalwins];

  if (!wins)
    free(wins);
  wins = malloc(sizeof(Window) * totalwins);

  int numofwins = 0;
  for (int i = 0; i < conf->num_of_desktops; i++) {
    numofwins = addtoclientlist(desktops[i].headc, wins, numofwins);
  }

  for (int i = 0; i < conf->num_of_desktops; i++) {
    if (desktops[i].floating) {
      Client *c = desktops[i].floating;
      while (c) {
        wins[numofwins] = c->win;
        numofwins++;
        c = c->a;
      }
    }
  }

  if (numofwins != totalwins) {
    printerr("can't create client list, totalwins != numofwins\n");
    exitwm(NULL);
    return;
  }

  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
      PropModeReplace, (unsigned char *)wins, totalwins);

#ifdef NWM_DEBUG
  printf("total wins: %d\n", totalwins);
#endif
}

int shouldmanage(Client *c) {
  // this function returns FAIL on success
  Atom wtype;
  if (!getwinprop(c, netatom[NetWMWindowType], &wtype, 1, XA_ATOM) ||
      wtype != netatom[NetWMWindowTypeDock]) {
    return SUC;
  }

  unsigned long strut[12];
  if (getwinprop(c, netatom[NetWMStrutPartial], strut, 12, XA_CARDINAL)) {
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

  XMapWindow(dpy, c->win);
  return FAIL;
}

void manage(Window w) {
#ifdef NWM_DEBUG
  printf("manage\n");
#endif
  if (!w) {
    printerr("window is null\n");
    return;
  }

  Client *headc = desktops[deski].headc;
  Client *focused = desktops[deski].tilefoc;

  Client *newc = createclient();
  newc->win = w;

  // check window type (normal, docks...)
  if (!shouldmanage(newc)) {
    free(newc);
    return;
  }

  // why was this done?
  //newc->p = headc;

  // setup the window
  XSelectInput(dpy, newc->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
  XChangeProperty(dpy, newc->win, netatom[NetWMDesktop], XA_CARDINAL, 32,
      PropModeReplace, (unsigned char *)&deski, 1);
  XSetWindowBorderWidth(dpy, newc->win, conf->bord_size);
  grabbuttons(newc);

  // rofi NEEDS this (otherwise it will not close properly...)
  // should try to get rid of this
  Atom protos[] = {wmatom[WMDelete]};
  XSetWMProtocols(dpy, newc->win, protos, 1);

  long sizehints = getsizehints(newc);

  // if the window doesn't want to be resized (minw == maxw && minh == maxh)
  // just make it floating
  if (sizehints & (PMinSize|PMaxSize) &&
      newc->maxw == newc->minw &&
      newc->maxh == newc->minh) {
    if (newc->w == 0 || newc->h == 0) {
      newc->w = newc->minw;
      newc->h = newc->minh;
    }
    newc->x = screenw/2 - newc->w/2;
    newc->y = screenh/2 - newc->h/2;
    addtoll(&desktops[deski].floating, newc, desktops[deski].floating);
  // else add to tiling
  } else if (!addtotree(headc, newc, focused)) {
    printerr("failed to add to tree\n");
    exitwm(NULL);
  }

  desktops[deski].active++;
  // for some client list (ewmh)
  totalwins++;
  createclientlist();
}

void unmanage(Window w) {
#ifdef NWM_DEBUG
  printf("unmanage\n");
#endif
  // this function has bugs!
  // may be fixed with the function above (fixchildren)?
  // nope there is a bug with focusing and maybe pointers to freed memory

  Client *c = NULL;
  if ((c = removefromtree(w)) == NULL) {
    //printerr("can't remove from tree\n");
    if ((c = removefromll(w)) == NULL) {
      //printerr("can't remove from ll\n");
      return;
    }
  }

  /*if (c == desktops[deski].focused) {
    setfocus(c->p);
    warptowin(c->p);
  }*/

  free(c);
  looptree(desktops[deski].headc, tilewins);
  looptree(desktops[deski].headc, mapwins);

  desktops[deski].active--;
  totalwins--;
  createclientlist();
}


// linked list
int loopll(Client *c, int (*func)(Client *)) {
#ifdef NWM_DEBUG
  printf("loopll\n");
#endif
  if (!c)
    return FAIL;

  while (c) {
    func(c);
    c = c->a;
  }

  return SUC;
}

Client *findclientll(Client *c, Window win) {
#ifdef NWM_DEBUG
  printf("findclientll\n");
#endif
  while (c) {
    if (c->win == win) {
      return c;
    }
    c = c->a;
  }

  return NULL;
}

int addtoll(Client **floating, Client *newc, Client *focused) {
#ifdef NWM_DEBUG
  printf("addtoll\n");
#endif
  if (!newc) {
    return FAIL;
  }

  newc->floating = True;
  newc->a = NULL;
  newc->b = NULL;
  newc->p = NULL;

  if (!focused) {
    if (!*floating) {
      *floating = newc;
      mapwins(newc);
      return SUC;
    }
    focused = *floating;
  }

  while (focused->a)
    focused = focused->a;

  // a forward
  // b backward
  focused->a = newc;
  newc->b = focused;

  mapwins(newc); // just draws the new one

  return SUC;
}

Client *removefromll(Window w) {
#ifdef NWM_DEBUG
  printf("removefromll\n");
#endif
  Client *c = NULL;
  Client *floating = NULL;
  Client *focused = NULL;
  int dt = 0;

  for (dt = 0; dt < conf->num_of_desktops; dt++) {
    c = findclientll(desktops[dt].floating, w);
    if (c) {
      floating = desktops[dt].floating;
      focused = desktops[dt].focused;
      break;
    }
  }

  if (!c) {
    //printf("can't find window\n");
    return NULL;
  }

  // a forward
  // b backward
  //
  // check if has next
  if (c->a) {
    c->a->b = c->b;
  }

  // if head set head to next
  if (c == floating) {
    desktops[dt].floating = c->a;
    c->b = c->a; // CHANGES IT'S PREV CLIENT FOR FOCUSING
  } else {
    c->b->a = c->a;
  }

  if (c == focused) {
    if (c->b) { // b has been set to the right one
      setfocus(c->a);
      warptowin(c->a);
    } else if (c->b) {
      setfocus(c->b);
      warptowin(c->b);
    } else {
      setfocus(desktops[dt].tilefoc);
      warptowin(desktops[dt].tilefoc);
    }
  }

  return c;
}


// bsp
int looptree(Client *c, int (*func)(Client *)) {
#ifdef NWM_DEBUG
  printf("looptree\n");
#endif
  if (!c)
    return FAIL;

  func(c);

  if (c->a)
    looptree(c->a, func);
  if (c->b)
    looptree(c->b, func);
  return SUC;
}

int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **)) {
#ifdef NWM_DEBUG
  printf("gototree\n");
#endif

  if (depth < 0) {
#ifdef NWM_DEBUG
    printf("depth < 0\n");
#endif
    return FAIL;
  }
  if (!c) {
#ifdef NWM_DEBUG
    printf("c is null\n");
#endif
    return FAIL;
  }
  if (depth <= 0 || !c->a || !c->b) {
    return func(c, retc);
  }

  if (path & 1)
    return gototree(c->a, retc, path >> 1, depth - 1, func);
  else
    return gototree(c->b, retc, path >> 1, depth - 1, func);
}

Client *findclient(Client *c, Window win) {
#ifdef NWM_DEBUG
  printf("findclient\n");
#endif
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
#ifdef NWM_DEBUG
  printf("findclientpath\n");
#endif
  *retc = c;
  return SUC;
}

int attachnode(Client *c, Client **newc) {
#ifdef NWM_DEBUG
  printf("addtotree\n");
#endif
  if (!c) {
    printerr("c is null\n");
    return FAIL;
  }
  if (c->win != root) {
    c->b = *newc;
    c->a = createclient();
    copyclient(c->a, c, True, True, True, False);
    c->win = root;
    c->a->depth++;
    c->b->depth++;
    c->a->path = c->a->path | (1 << c->depth);
    c->b->path = c->b->path & ~(1 << c->depth);
    /*if (c == desktops[deski].focused)
      desktops[deski].focused = *newc;*/
    c->a->p = c;
    c->b->p = c;
  } else {
    copyclient(c, *newc, True, True, True, False);
    free(*newc);
    c->p = NULL;
    *newc = c; // should prob do this because it's freeing memory that does NOT belong to this function
  }
  return SUC;
}

int addtotree(Client *headc, Client *newc, Client *focused) {
  if (focused) {
    newc->path = focused->path;
    newc->depth = focused->depth;
    if (focused->p)
      newc->p = focused->p;
  }

  if (gototree(headc, &newc, newc->path, newc->depth, attachnode)) {
#ifdef NWM_DEBUG
    printf("added to tree?\n");
    looptree(headc, printbsptree);
#endif
    looptree(headc, tilewins);
    looptree(headc, mapwins);

    setfocus(newc);
    warptowin(newc);
  } else {
    return FAIL;
  }
  return SUC;
}

Client *removefromtree(Window w) {
#ifdef NWM_DEBUG
  printf("removefromtree\n");
#endif
  // this function won't free the client, it just removes it from the tree and returns it

  if (!w)
    return NULL;

  if (w == root) {
    return NULL;
  }

  Client *c = NULL;
  Client *headc = NULL;
  Client *focused = NULL;
  int dt = 0; // DeskTop of deleted win

  for (dt = 0; dt < conf->num_of_desktops; dt++) {
    c = findclient(desktops[dt].headc, w);
    if (c) {
      headc = desktops[dt].headc;
      focused = desktops[dt].focused;
      break;
    }
  }

  if (!c)
    return FAIL;

  // to delete all clients
  if (c == headc) {
    desktops[dt].headc = createclient();
    desktops[dt].focused = NULL;
    desktops[dt].active = 0;

    // doesn't need to call setfocus because it just needs to delete this property
    //XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    //desktops[deski].tilefoc = NULL; // also needs to set this
    setfocus(desktops[dt].focused);
    return c; // caller should free this
  }

  if (c->a || c->b) {
    printerr("can't delete window, has children (how did we get here...?)\n");
    exitwm(NULL);
  }

  Client *prevc = c->p;
  if (!prevc)
    return NULL;

  if (c == prevc) {
    printerr("c == prevc (this is an error state in unmanage)\n");
    exitwm(NULL);
  }

  if (!prevc->a && !prevc->b) {
    printerr("doesn't have both children\n");
    exitwm(NULL);
  }

  if (prevc->a == c) {
    copyclient(prevc, prevc->b, True, True, False, True);
  } else {
    copyclient(prevc, prevc->a, True, True, False, True);
  }

  looptree(prevc, fixchildren);

  if (c == focused) {
    setfocus(prevc);
    warptowin(prevc);
    if (prevc->win == c->win) {
      exitwm(0);
    }
  }

  return c; // caller should free this
}

int tilewins(Client *c) {
#ifdef NWM_DEBUG
  printf("tilewins\n");
  //printf("tilling tree %ld\n", deski);

  printf("tiling %lx\n", c->win);
#endif

  if (c->p) {
    c->w = c->p->w;
    c->h = c->p->h;
    c->x = c->p->x;
    c->y = c->p->y;
  } else {
    c->w = screenw + swoff - (conf->hgaps*2) - (conf->bord_size*2);
    c->h = screenh + shoff - (conf->vgaps*2) - (conf->bord_size*2);
    c->x = 0 + sxoff + conf->hgaps;
    c->y = 0 + syoff + conf->vgaps;
    return SUC;
  }

  if ((c->path & (1 << (c->depth - 1))) == 0) {
    if (c->depth & 1) {
      c->w *= 1.0 - c->p->split;
      if (c->p->w * (1.0 - c->p->split) > c->w)
        c->w += 1;
      c->w -= conf->hgaps/2 + conf->bord_size;

      c->x += c->p->w * c->p->split;
      c->x += conf->hgaps/2 + conf->bord_size;
    } else {
      c->h *= 1.0 - c->p->split;
      if (c->p->h * (1.0 - c->p->split) > c->h)
        c->h += 1;
      c->h -= conf->vgaps/2 + conf->bord_size;

      c->y += c->p->h * c->p->split;
      c->y += conf->vgaps/2 + conf->bord_size;
    }
  } else {
    if (c->depth & 1) {
      c->w *= c->p->split;
      c->w -= conf->hgaps/2 + conf->bord_size;
    } else {
      c->h *= c->p->split;
      c->h -= conf->vgaps/2 + conf->bord_size;
    }
  }

  if (c->w <= 0 || c->h <= 0) {
    printerr("width/height too small\n");
    removefromtree(c->win);
    addtoll(&desktops[deski].floating, c, desktops[deski].floating);
  }
  return SUC;
}


// keypress
int focuswindow(Arg *arg) {
#ifdef NWM_DEBUG
  printf("focuswindow\n");
#endif

  Client *focused = desktops[deski].focused;

  if (!focused || focused->win == root || !focused->p || focused->floating)
    return FAIL;

  focused = findclientindir(focused, arg->i);

  if (!focused)
    return FAIL;

  //XDeleteProperty(dpy, root, netatom[NetActiveWindow]);

  setfocus(focused);
  warptowin(focused);

  return SUC;
}

int swapwindow(Arg *arg) {
#ifdef NWM_DEBUG
  printf("focuswindow\n");
#endif

  Client *focused = desktops[deski].focused;

  if (!focused || focused->win == root || !focused->p || focused->floating)
    return FAIL;

  Client *target = findclientindir(focused, arg->i);

  if (!target)
    return FAIL;

  Client temp;
  copyclient(&temp, focused, True, False, False, False);
  copyclient(focused, target, True, False, False, False);
  copyclient(target, &temp, True, False, False, False);

  //looptree(desktops[deski].headc, tilewins);
  looptree(desktops[deski].headc, mapwins);

  setfocus(target);
  warptowin(target);

  return SUC;
}

int movewindow(Arg *arg) {
#ifdef NWM_DEBUG
  printf("movewindow\n");
#endif
  Client *c = desktops[deski].focused;
  if (!c || !c->floating || c->win == root) {
    //printerr("not a floating window\n");
    return FAIL;
  }

  ignoreenter = 1;

  switch (arg->i) {
    case dirup:
      c->y -= conf->move_amount;
      break;
    case dirdown:
      c->y += conf->move_amount;
      break;
    case dirleft:
      c->x -= conf->move_amount;
      break;
    case dirright:
      c->x += conf->move_amount;
      break;
  }

  mapwins(c);

  return SUC;
}

int dragmovewindow(Arg *arg) {
#ifdef NWM_DEBUG
  printf("dragwindow\n");
#endif

  if (arg->i == 0) {
    return FAIL;
  }

  Client *c = desktops[deski].focused;

  if (!c || c->win == root) {
    return FAIL;
  }

  ignoreenter = 1;

  if (!c->floating) {
    floattoggle(0);
  }

  c->x += mxoff;
  c->y += myoff;

  mapwins(c);

  return SUC;
}

int dragresizewindow(Arg *arg) {
#ifdef NWM_DEBUG
  printf("dragwindow\n");
#endif

  if (arg->i == 0) {
    return FAIL;
  }

  Client *c = desktops[deski].focused;

  if (!c || c->win == root) {
    return FAIL;
  }

  ignoreenter = 1;

  if (!c->floating) {
    floattoggle(0);
  }

  c->w += mxoff;
  c->h += myoff;

  if (c->w <= conf->min_size)
    c->w = conf->min_size;

  if (c->h <= conf->min_size)
    c->h = conf->min_size;

  mapwins(c);

  return SUC;
}

int resizetiled(Arg *arg) {
#ifdef NWM_DEBUG
  printf("resizetiled\n");
#endif

  // find what to resize
  int dir = arg->i;

  static int iters;
  static Client *focused;

  if (iters == 0) {
    focused = desktops[deski].focused;
  }

  iters++;

  if (!focused->p)
    return FAIL;

  Client *ca = focused->p->a;
  Client *cb = focused->p->b;

  if (!ca || !cb)
    return FAIL;

  if (((dir >= 2) && (ca->p->depth & 1)) ||
      ((dir <= 1) && !(ca->p->depth & 1))) {
    Client *tempfoc = focused;
    focused = focused->p;
    if (!focused)
      return FAIL;
    if (resizetiled(arg)) {
      focused = tempfoc;
      return SUC;
    }
    focused = tempfoc;
  }

  // actually resize

  // needs to 0 or 1
  if (dir >= 2)
    dir -= 2;

  if (ca->depth & 1) {
    double scrwinratio = (double)(screenw - swoff) / (double)ca->p->w;
    double pixeltopercent = scrwinratio * conf->min_size / (screenw - swoff);

    ca->p->split += conf->resize_amount * scrwinratio * ((dir * 2) - 1);

    if (ca->p->split <= pixeltopercent)
      ca->p->split = pixeltopercent;
    else if (ca->p->split >= 1.0f - pixeltopercent)
      ca->p->split = 1.0f - pixeltopercent;
  } else {
    double scrwinratio = (double)(screenh - shoff) / (double)ca->p->h;
    double pixeltopercent = scrwinratio * conf->min_size / (screenh - shoff);

    ca->p->split += conf->resize_amount * scrwinratio * ((dir * 2) - 1);

    if (ca->p->split <= pixeltopercent)
      ca->p->split = pixeltopercent;
    else if (ca->p->split >= 1.0f - pixeltopercent)
      ca->p->split = 1.0f - pixeltopercent;
  }

  looptree(desktops[deski].headc, tilewins);
  looptree(desktops[deski].headc, mapwins);

  if (iters == 1) {
    setfocus(focused);
    warptowin(focused);
  } else {
    warptowin(desktops[deski].focused);
  }

  iters = 0;
  return SUC;
}

int resizewindow(Arg *arg) {
#ifdef NWM_DEBUG
  printf("resizewindow\n");
#endif

  Client *c = desktops[deski].focused;

  if (!c || c->win == root) {
    return FAIL;
  }


  if (c->floating) {
    switch (arg->i) {
      case dirup:
        c->h -= conf->move_amount;
        break;
      case dirdown:
        c->h += conf->move_amount;
        break;
      case dirleft:
        c->w -= conf->move_amount;
        break;
      case dirright:
        c->w += conf->move_amount;
        break;
    }

    if (c->w <= conf->min_size)
      c->w = conf->min_size;
    if (c->h <= conf->min_size)
      c->h = conf->min_size;

    mapwins(c);
    warptowin(c);
    return SUC;
  }

  return resizetiled(arg);
}

int focusdesktop(Arg *arg) {
#ifdef NWM_DEBUG
  printf("switching desktops...\n");
#endif
  
  if (arg->i >= conf->num_of_desktops) {
    printerr("can't focus desktop (out of range)\n");
    return FAIL;
  }

  XGrabServer(dpy);

  deski = arg->i;

  for (int i = 0; i < conf->num_of_desktops; i++) {
    if (i == deski)
      continue;
    looptree(desktops[i].headc, unmapwins);
    loopll(desktops[i].floating, unmapwins);
  }

  looptree(desktops[deski].headc, mapwins);
  loopll(desktops[deski].floating, mapwins);

  XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
      PropModeReplace, (const unsigned char *)&deski, 1);

  // deletes just in case focused is null
  //XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  setfocus(desktops[deski].focused);
  warptowin(desktops[deski].focused);

  XUngrabServer(dpy);
  XSync(dpy, True);

  return SUC;
}

int movedesktop(Arg *arg) {
#ifdef NWM_DEBUG
  printf("movedesktop\n");
#endif
  
  if (arg->i >= conf->num_of_desktops)
    return FAIL;
  if (!desktops[deski].focused)
    return FAIL;

  Client *c = desktops[deski].focused;

  if (c->floating) {
    c = removefromll(c->win);
  } else {
    c = removefromtree(c->win);
    looptree(desktops[deski].headc, tilewins);
    //looptree(desktops[deski].headc, mapwins);

    //desktops[deski].tilefoc = desktops[deski].focused;
  }

  if (!c)
    return FAIL;

  unmapwins(c);

  //setfocus(desktops[deski].headc->a);
  desktops[deski].active--;

  looptree(desktops[deski].headc, printbsptree);

  focusdesktop(arg); // moving to desktop

  // need this to tell polybar which window this window is on
  XChangeProperty(dpy, c->win, netatom[NetWMDesktop], XA_CARDINAL, 32,
      PropModeReplace, (unsigned char *)&deski, 1);

  c->path = 0;
  c->depth = 0;
  c->a = NULL;
  c->b = NULL;
  c->p = NULL;

  if (c->floating) {
    addtoll(&desktops[deski].floating, c, desktops[deski].floating);
  } else {
    addtotree(desktops[deski].headc, c, desktops[deski].tilefoc);
  }

  looptree(desktops[deski].headc, printbsptree);

  createclientlist();

  return SUC;
}

/*
 this code needs to be worked on
 there is some posix stuff that dwm does that this code DOES NOT DO
 god know if this is fixed ^
 prob fixed
 uhh this only returns suc...
*/
int spawn(Arg *arg) {
#ifdef NWM_DEBUG
  printf("spawning: [%s]\n", arg->s[0]);
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

  return SUC;
}

int killfocused(Arg *arg) {
  (void)arg;
#ifdef NWM_DEBUG
  printf("killfocused\n");
#endif
  if (!desktops[deski].focused || desktops[deski].focused->win == root)
    return FAIL;

  if (sendevent(desktops[deski].focused, wmatom[WMDelete]) == 0) {
    printerr("failed to send event\n");
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, desktops[deski].focused->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }

  return SUC;
}

int floattoggle(Arg *arg) {
#ifdef NWM_DEBUG
  printf("floattoggle\n");
#endif
  (void)arg;

  if (!desktops[deski].focused || desktops[deski].focused->win == root)
    return FAIL;

  Client *c;

  // floating window
  if (desktops[deski].focused->floating) {
    c = removefromll(desktops[deski].focused->win);

    if (!c)
      return FAIL;

    c->floating = False;
    c->depth = 0;
    c->path = 0;
    c->a = NULL;
    c->b = NULL;
    c->p = NULL;

    addtotree(desktops[deski].headc, c, desktops[deski].tilefoc);

  // tiled window
  } else {
    c = removefromtree(desktops[deski].focused->win);

    if (!c)
      return FAIL;

    c->floating = True;
    c->depth = 0;
    c->path = 0;
    c->a = NULL;
    c->b = NULL;
    c->p = NULL;

    addtoll(&desktops[deski].floating, c, desktops[deski].floating);
    mapwins(c);
  }

  //setfocus(c);
  warptowin(c);
  looptree(desktops[deski].headc, tilewins);
  looptree(desktops[deski].headc, mapwins);
  return SUC;
}

int focustoggle(Arg *arg) {
#ifdef NWM_DEBUG
  printf("focustoggle\n");
#endif

  (void)arg;

  Client *focused = desktops[deski].focused;

  if (!focused || focused->win == root)
    return FAIL;

  if (focused->floating) {
    if (focused->a)
      focused = focused->a;
    else
      focused = desktops[deski].tilefoc;
  } else if (desktops[deski].floating) {
    focused = desktops[deski].floating;
  }

  setfocus(focused);
  warptowin(focused);

  return SUC;
}

int exitwm(Arg *arg) {
#ifdef NWM_DEBUG
  printf("exitwm\n");
#endif
  XCloseDisplay(dpy);
  killserver();

  if (arg)
    exit(arg->i);

  exit(0);
  return SUC;
}


// x11
int sendevent(Client *c, Atom proto) {
#ifdef NWM_DEBUG
  printf("sendevent\n");
#endif
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
    ev.xclient.type = ClientMessage;
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
#ifdef NWM_DEBUG
  printf("setfocus\n");
#endif


  if (!c || c->win == root) {
    printf("c is null\n");
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    return;
  }

  if (desktops[deski].focused == NULL && desktops[deski].active >= 1) {
    printerr("focus is null\n");
    exitwm(0);
  }

  desktops[deski].focused = c;

  //printf("win: %lx\n", c->win);
  if (c->floating == False) {
    desktops[deski].tilefoc = c;
  } else {
    XRaiseWindow(dpy, c->win);
  }

  XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
  XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
      PropModeReplace, (unsigned char *)&c->win, 1);
  sendevent(c, wmatom[WMTakeFocus]); // god know what this does

  looptree(desktops[deski].headc, updateborders);
  loopll(desktops[deski].floating, updateborders);
}

int updateborders(Client *c) {
#ifdef NWM_DEBUG
  printf("updateborders\n");
#endif
  if (!c)
    return FAIL;

  XSetWindowBorderWidth(dpy, c->win, conf->bord_size);
  if (!c->floating)
    XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? conf->bord_foc_col : conf->bord_nor_col));
  else
    XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? 0xff0000 : 0x00ff00));
    //XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? conf->bord_foc_col : conf->bord_nor_col));
  return SUC;
}

int mapwins(Client *c) {
#ifdef NWM_DEBUG
  printf("mapwins\n");
#endif
  if (!c)
    return FAIL;

  if (c->win == root)
    return FAIL;

#ifdef NWM_DEBUG
  printf("win: path=%-8b, win=%-8lx, x=%-4d, y=%-4d, w=%-4d, h=%-4d\n",
         c->path, c->win, c->x, c->y, c->w, c->h);
#endif
  XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
  XMapWindow(dpy, c->win);
  return SUC;
}

int unmapwins(Client *c) {
#ifdef NWM_DEBUG
  printf("unmapwins\n");
#endif
  XUnmapWindow(dpy, c->win);
  return SUC;
}


// others
void setup(void) {
#ifdef NWM_DEBUG
  printf("setup\n");
#endif
  int screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  screenw = XDisplayWidth(dpy, screen);
  screenh = XDisplayHeight(dpy, screen);
  // for docks/bars
  sxoff = 0;
  syoff = 0;

  // to load the cursor at the start
  cursor = XcursorLibraryLoadCursor(dpy, "left_ptr");
  XDefineCursor(dpy, root, cursor);

  // https://tronche.com/gui/x/xlib/events/processing-overview.html
  XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|FocusChangeMask|EnterWindowMask|PropertyChangeMask);

  for (int i = 0; i < LASTEvent; i++)
    handler[i] = voidevent;
  handler[KeyPress] = keypress;
  handler[ButtonPress] = buttonpress;
  handler[ButtonRelease] = buttonrelease;
  handler[MotionNotify] = motionnotify;
  handler[MapRequest] = maprequest;
  handler[DestroyNotify] = destroynotify;
  handler[UnmapNotify] = unmapnotify;
  handler[EnterNotify] = enternotify;
  handler[FocusIn] = focusin;
  handler[ClientMessage] = clientmessage;

  conf = malloc(sizeof(Config));
  /**conf = (Config){
    .vgaps = 20,
    .hgaps = 20,
    .bord_size = 4,
    .bord_foc_col = 0xffc4a7e7L,
    .bord_nor_col = 0xff26233aL,
    .num_of_desktops = 2,
    .resize_amount = 4,
    .keyslen = 15,
  };*/
  *conf = (Config){
    .vgaps = 0,
    .hgaps = 0,
    .bord_size = 2,
    .bord_foc_col = 0xffeeeeee,
    .bord_nor_col = 0xff333333,
    .num_of_desktops = 5,
    .resize_amount = 0.05f,
    .min_size = 50.0f,
    .keyslen = 1,
  };
  conf->keys = NULL;

  char **arg;
  arg = malloc(sizeof(char *) * 2);
  arg[0] = "st";
  arg[1] = NULL;
  conf->keys = malloc(sizeof(Key) * conf->keyslen);
  conf->keys[0] = (Key){Mod1Mask, XStringToKeysym("a"), spawn, {.s = arg}, False};


  startserver();

  // desktops
  if (conf->num_of_desktops < 1)
    conf->num_of_desktops = 1;

  old_num_of_desktops = conf->num_of_desktops;

  desktops = malloc(conf->num_of_desktops * sizeof(Desktop));
  deski = 0;

  for (int i = 0; i < conf->num_of_desktops; i++) {
    desktops[i].headc = createclient();
    desktops[i].floating = NULL;
    desktops[i].focused = NULL;
    desktops[i].tilefoc = NULL;
    desktops[i].active = 0;
  }

  conf->desktop_names = "a\0""b\0""c\0""d\0";
  conf->desktop_names_len = 8;
  // you can't just do sizeof or strlen on the workspace names string

  // grab keys
  if (conf->keys) {
    for (int i = 0; i < conf->keyslen; i++) {
      XGrabKey(dpy, XKeysymToKeycode(dpy, conf->keys[i].keysym), conf->keys[i].mod,
            root, True, GrabModeAsync, GrabModeAsync);
    }
  }

  // startup script
  arg = malloc(sizeof(char *) * 2);
  arg[0] = "/home/ethan/neowm/startup";
  arg[1] = NULL;
  spawn(&(Arg){.s = arg});
  free(arg);

  arg = malloc(sizeof(char *) * 2);
  arg[0] = "/home/ethan/neowm/nwm.conf";
  arg[1] = NULL;
  spawn(&(Arg){.s = arg});
  free(arg);
}

void setupatoms(void) {
#ifdef NWM_DEBUG
  printf("setupatoms\n");
#endif
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

  netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
  netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
  netatom[NetWMDesktop] = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
  netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);

  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeNormal] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
  netatom[NetWMWindowTypePopup] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);

  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMStateAbove] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);

  Window WmCheckWin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char*)&WmCheckWin, 1);
  XChangeProperty(dpy, WmCheckWin, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char*)&WmCheckWin, 1);
  XChangeProperty(dpy, WmCheckWin, netatom[NetWMName], utf8string, 8,
      PropModeReplace, (unsigned char*)WM_NAME, strlen(WM_NAME));

  XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
      PropModeReplace, (const unsigned char *)&conf->num_of_desktops, 1);
  XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
      PropModeReplace, (const unsigned char *)&deski, 1);
  XChangeProperty(dpy, root, netatom[NetDesktopNames], utf8string, 8,
      PropModeReplace, (const unsigned char *)conf->desktop_names, conf->desktop_names_len);

  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
      PropModeReplace, (unsigned char *)netatom, NetLast);
}

int xerror(Display *dpy, XErrorEvent *ee) {
#ifdef NWM_DEBUG
  printf("xerror\n");
#endif
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
  (void)dpy;
  (void)ee;
#ifdef NWM_DEBUG
  printf("xerrordummy\n");
#endif
  return 0;
}

int xerrorstart(Display *dpy, XErrorEvent *ee) {
  (void)dpy;
  (void)ee;
#ifdef NWM_DEBUG
  printf("xerrorstart\n");
#endif
  printerr("another window manager is running\n");

  // can't use exitwm because it tries to close the display
  exit(0);
}

void checkotherwm(void) {
#ifdef NWM_DEBUG
  printf("checkotherwm\n");
#endif
  xerrorxlib = XSetErrorHandler(xerrorstart);
  // do something that will cause an error if a wm is running
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerrorxlib);
  XSync(dpy, False);
}


int main(void) {
  XEvent ev;

  if (!(dpy = XOpenDisplay(NULL))) {
    printerr("failed to open display\n");
    exitwm(0);
  }

  //printf("Default screen: %d\nScreen width: %d\nScreen height: %d\n", screen, screenw, screenh);

  checkotherwm();

  setup();
  setupatoms();
  xerrorxlib = XSetErrorHandler(xerror);

#ifdef NWM_DEBUG
  printf("\nEVENTS:\n");
#endif

  for (;;) {
    XNextEvent(dpy, &ev);
#ifdef NWM_DEBUG
    printf("\nevent recv of type %d ", ev.type);
#endif

    handler[ev.type](&ev);

    //printf("done\n");
  }

  printerr("exiting with no errors\n");
  exitwm(0);
  return 0;
}
