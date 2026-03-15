#include "main.h"
#include "config.h"

Atom wmatom[WMLast];
Atom netatom[NetLast];

Desktop *desktops;
long deski;

Config *conf;
Display *dpy;
Window root;

int workspaceswitch = 0;

// for net client list ewmh
int totalwins = 0;

int screenw, screenh;
// for bars and stuff
int sxoff, syoff;
int swoff, shoff;

void (*handler[LASTEvent])(XEvent*);
int (*xerrorxlib)(Display *, XErrorEvent *);

#define NWM_DEBUG

// debug
void printerr(char *errstr) {
#ifdef NWM_DEBUG
  printf("printerr\n");
#endif
  fprintf(stderr, "%s: error: %s", WM_NAME, errstr);
}

void printpath(unsigned int path, int depth) {
#ifdef NWM_DEBUG
  printf("printpath\n");
#endif
  for (int i = 0; i < depth; i++) {
    if ((path & (1 << i)) == 1) {
      printf("1");
    } else {
      printf("0");
    }
  }
}

int printbsptree(Client *c) {
#ifdef NWM_DEBUG
  printf("printbsptree\n");
#endif
  if (!c) {
    printf("client is invalid\n");
    return 0;
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
  /*if (c->a)
    printf(", a->path=%-8lb, a->win=%-8lx", c->a->path, c->a->win);
  if (c->a)
    printf(", b->path=%-8lb, b->win=%-8lx", c->b->path, c->b->win);*/
  printf("\n");

  return 1;
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


// helpers
char keysymtostring(XKeyEvent *xkey) {
#ifdef NWM_DEBUG
  printf("keysymtostring\n");
#endif
  return *XKeysymToString(XLookupKeysym(xkey, 0));
}

int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype) {
#ifdef NWM_DEBUG
  printf("getwinprop\n");
#endif
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

void remapwins(void) {
  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, drawwins);
  looptree(desktops[deski].headc, updateborders);
  updatebordersll(desktops[deski].floating);
}

void rebindkeys(void) {
  for (int i = 0; i < conf->keyslen; i++) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, conf->keys[i].keysym), conf->keys[i].mod,
          root, True, GrabModeAsync, GrabModeAsync);
  }
}

void unbindkeys(void) {
  for (int i = 0; i < conf->keyslen; i++) {
    XUngrabKey(dpy, XKeysymToKeycode(dpy, conf->keys[i].keysym), conf->keys[i].mod, root);
  }
}

void flushx11(void) {
  printf("flushx11\n");
  remapwins();
  rebindkeys();
  XSync(dpy, False);
}


// events
void voidevent(XEvent *ev) {
  (void)ev;
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
  for (int i = 0; i < conf->keyslen; i++) {
    if (keysym == (conf->keys)[i].keysym &&
        CLEANMASK(conf->keys[i].mod) == CLEANMASK(xkey->state) &&
        conf->keys[i].func) {
      conf->keys[i].func(&conf->keys[i].args);
    }
  }

  // only here to make sure it's always possible to exit wm
  /*if (keysymtostring(xkey) == 'q' && xkey->state == Mod1Mask) {
    fprintf(stderr, "%s: exiting with no errors\n", WM_NAME);
    exitwm(0);
  }*/
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
  manage(mapreq->window);
}

void unmapnotify(XEvent *ev) {
#ifdef NWM_DEBUG
  printf("(unmapnotify)\n");
#endif

// NEED TO FIND A WAY TO TELL IF THE UNMAPNOTIFY EVENT WAS SENT BY THE WM OR NOT
// THIS IS NEEDED FOR focusdesktop()

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

  if (ev->xfocus.window == root)
    return;

  if (desktops[deski].focused && ev->xfocus.window != desktops[deski].focused->win)
    setfocus(desktops[deski].focused);
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
  c->split = 50;
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

void copyclientdata(Client *a, Client *b, Bool win, Bool path, Bool ab) {
#ifdef NWM_DEBUG
  printf("copyclientdata\n");
#endif
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
#ifdef NWM_DEBUG
  printf("finding path\n");
#endif

  if (dir == 0)
    depth = 32;

  if (!(depth & 1)) {
    depth--;
  }
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

Client *findclientindir(Client *incl, int dir) {
#ifdef NWM_DEBUG
  printf("findclientdir\n");
#endif
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

  if (!gototree(desktops[deski].headc, &cl, destpath, 32, findclientpath)) {
    printf("can't find client\n");
    return NULL;
  }

  return cl;
}

int mapwins(Client *c) {
#ifdef NWM_DEBUG
  printf("mapping tree %ld\n", deski);
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
    return 1;
  }

  if ((c->path & (1 << (c->depth - 1))) == 0) {
    if (c->depth & 1) {
      c->w *= (100.0 - c->p->split)/100.0;
      if (c->p->w * (100.0 - c->p->split)/100.0 > c->w)
        c->w += 1;
      c->w -= conf->hgaps/2 + conf->bord_size;

      c->x += (c->p->w * c->p->split) / 100.0;
      c->x += conf->hgaps/2 + conf->bord_size;
    } else {
      c->h *= (100.0 - c->p->split)/100.0;
      if (c->p->h * (100.0 - c->p->split)/100.0 > c->h)
        c->h += 1;
      c->h -= conf->vgaps/2 + conf->bord_size;

      c->y += (c->p->h * c->p->split) / 100.0;
      c->y += conf->vgaps/2 + conf->bord_size;
    }
  } else {
    if (c->depth & 1) {
      c->w *= (c->p->split)/100.0;
      c->w -= conf->hgaps/2 + conf->bord_size;
    } else {
      c->h *= (c->p->split)/100.0;
      c->h -= conf->vgaps/2 + conf->bord_size;
    }
  }

  if (c->w <= 0 || c->h <= 0) {
    printerr("width/height too small\n");
    int ret = 1;
    exitwm((Arg *)&ret);
  }
  return 1;
}

int unmapwins(Client *c) {
#ifdef NWM_DEBUG
  printf("unmapwins\n");
#endif
  XUnmapWindow(dpy, c->win);
  return 1;
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
      looptree(headc, mapwins);
      looptree(headc, drawwins);
      free(newc);
      return;
    }
  }

  newc->p = headc;

  XSetWindowBorderWidth(dpy, newc->win, conf->bord_size);
  XChangeProperty(dpy, w, netatom[NetWMDesktop], XA_CARDINAL, 32,
      PropModeReplace, (unsigned char *)&deski, 1);

  XSizeHints sizehints;
  long supret;
  if(XGetWMNormalHints(dpy, w, &sizehints, &supret)) {
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
  }

  if (newc->maxw == newc->minw && newc->maxh == newc->minh) {
    Client *c = desktops[deski].floating;
    if (!desktops[deski].floating) {
      desktops[deski].floating = newc;
    } else {
      while (c->a) {
        c = c->a;
      }
      if (!c) {
        printf("aaaaaaaaaaaaaaaaa\n");
        exitwm(NULL);
      }

      c->a = newc;
    }
    newc->a = NULL;
    newc->b = NULL;
    newc->w = newc->minw;
    newc->h = newc->minw;
    newc->floating = True;
    drawwins(newc);

    //printll(desktops[deski].floating);
    return;
  }

  if (focused == NULL) {
#ifdef NWM_DEBUG
    printf("focused is null\n");
#endif
    newc->path = 0;
    newc->depth = 0;
  } else {
    //printf("focused->win: %lx\n", focused->win);
    newc->path = focused->path;
    newc->depth = focused->depth;

    if (focused->p) {
      newc->p = focused->p;
    }
  }

  if (gototree(headc, &newc, newc->path, newc->depth, addtotree)) {
#ifdef NWM_DEBUG
    printf("added to tree?\n");
    looptree(headc, printbsptree);
#endif
    looptree(headc, mapwins);
    looptree(headc, drawwins);
    setfocus(newc);
  } else {
    printf("failed to add to tree\n");
  }

  totalwins++;
  createclientlist();
}

int fixchildren(Client *c) {
#ifdef NWM_DEBUG
  printf("fixchildren\n");
#endif
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
#ifdef NWM_DEBUG
  printf("unmanage\n");
#endif
  // this function has bugs!
  // may be fixed with the function above (fixchildren)?
  // nope there is a bug with focusing and maybe pointers to freed memory

  if (!w)
    return;

  if (w == root) {
    printf("root\n");
    return;
  }

  //printf("w: %lx\n", w);

  Client *c = NULL;
  Client *headc = NULL;
  Client *focused = NULL;
  int dt = 0;

  for (dt = 0; dt < conf->num_of_desktops; dt++) {
    c = findclient(desktops[dt].headc, w);
    if (c) {
      /*printf("found client\n");
      printf("dt: %d\n", dt);
      printf("deski: %ld\n", deski);*/
      headc = desktops[dt].headc;
      focused = desktops[dt].focused;
      break;
    }
  }

  if (!c) {
    //printf("c is null\n");
    return;
  }

  // to delete all clients
  if (c == headc) {
    free(desktops[dt].headc);
    desktops[dt].headc = createclient();
    desktops[dt].focused = NULL;
    setfocus(desktops[dt].focused);

    totalwins--;
    createclientlist();
    return;
  }

  // can't delete this node if it has both children
  if (c->a && c->b) {
    printerr("client is invalid (has both children)\n");
    return;
  }

  Client *prevc = c->p;

  if (!prevc) {
    return;
  }

  if (c == prevc) {
    printf("##########\nc == prevc (this is an error state in unmanage)\n##########\n");
    return;
  }

  if (!prevc->a && !prevc->b) {
    printf("doesn't have both children\n");
    return;
  }

  if (prevc->a == c) {
    copyclientdata(prevc, prevc->b, True, False, True);
  } else {
    copyclientdata(prevc, prevc->a, True, False, True);
  }

  looptree(prevc, fixchildren);

  if (c == focused)
    setfocus(prevc);

  //printf("freeing c\n");
  free(c);
#ifdef NWM_DEBUG
  printf("mapping wins (after deletion)\n");
#endif
  looptree(headc, mapwins);
  looptree(headc, drawwins);

  totalwins--;
  createclientlist();
}

int addtoclientlist(Client *c, Window *wins, int index) {
#ifdef NWM_DEBUG
  printf("addtoclientlist\n");
#endif
  if (c->win != root) {
    wins[index] = c->win;
    index++;
  }
  if (c->a)
    index = addtoclientlist(c->a, wins, index);
  if (c->b)
    index = addtoclientlist(c->b, wins, index);
  return index;
}

void createclientlist(void) {
#ifdef NWM_DEBUG
  printf("createclientlist\n");
#endif
  Window wins[totalwins];

  int index = 0;
  for (int i = 0; i < conf->num_of_desktops; i++) {
    index = addtoclientlist(desktops[i].headc, wins, index);
  }

  if (index != totalwins) {
    printerr("can't create client list, totalwins != index\n");
    return;
  }

  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
      PropModeReplace, (unsigned char *)wins, totalwins);

#ifdef NWM_DEBUG
  printf("total wins: %d\n", totalwins);
#endif
}


// linked list
Client *findclientll(Client *c, Window win) {
  printf("findclientll\n");
  while (c) {
    if (c->win == win) {
      return c;
    }
    c = c->a;
  }

  return NULL;
}


// bsp
int looptree(Client *c, int (*func)(Client *)) {
#ifdef NWM_DEBUG
  printf("looptree\n");
#endif
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
  printf("findpath\n");
#endif
  *retc = c;
  return 1;
}

int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **)) {
#ifdef NWM_DEBUG
  printf("gototree\n");
#endif
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
#ifdef NWM_DEBUG
  printf("addtotree\n");
#endif
  if (!c) {
    printf("c is null\n");
    return 0;
  }
  if (c->win != root) {
    c->b = *newc;
    c->a = createclient();
    copyclientdata(c->a, c, True, True, False);
    c->win = root;
    c->a->depth++;
    c->b->depth++;
    c->a->path = c->a->path | (1 << c->depth);
    c->b->path = c->b->path & ~(1 << c->depth);
    if (c == desktops[deski].focused)
      desktops[deski].focused = *newc;
    c->a->p = c;
    c->b->p = c;
  } else {
    copyclientdata(c, *newc, True, True, False);
    free(*newc);
    c->p = NULL;
    *newc = c; // should prob do this because it's freeing memory that does NOT belong to this function
  }
  return 1;
}


// keypress
int focusswitch(Arg *arg) {
#ifdef NWM_DEBUG
  printf("focusswitch\n");
#endif

  Client *focused = desktops[deski].focused;

  if (!focused || !focused->p)
    return 0;

  desktops[deski].focused = findclientindir(focused, arg->i);
  focused = desktops[deski].focused;
  setfocus(focused);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));

  return 1;
}

int resizeclient(Arg *arg) {
#ifdef NWM_DEBUG
  printf("resizeclient\n");
#endif
  int dir = arg->i;

  Client *focused = desktops[deski].focused;

  //printf("resizing client\n");
  //printf("dir = %d\n", dir);

  if (!focused->p) {
#ifdef NWM_DEBUG
    printf("focused doesn't have a parent\n");
#endif
    return 0;
  }

  Client *ca = focused->p->a;
  Client *cb = focused->p->b;

  if (!ca || !cb)
    return 0;

  /*printf("ca->x = %d\n", ca->x);
  printf("ca->y = %d\n", ca->y);
  printf("cb->x = %d\n", cb->x);
  printf("cb->y = %d\n", cb->y);*/

  if (((dir >= 2) && (ca->x == cb->x)) ||
      ((dir <= 1) && (ca->y == cb->y))) {
    Client *tempfoc = focused;
    // may be a bad idea to change this?
    desktops[deski].focused = focused->p;
    focused = desktops[deski].focused;
    if (!focused)
      return 0;
    if (resizeclient(arg)) {
      XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
      desktops[deski].focused = tempfoc;
      focused = desktops[deski].focused;
      return 1;
    }
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
    desktops[deski].focused = tempfoc;
    focused = desktops[deski].focused;
  }

  if (dir >= 2)
    dir -= 2;

  ca->p->split += conf->resize_amount * ((dir * 2) - 1);

  if (ca->p->split >= 90) {
    ca->p->split = 90;
  } else if (ca->p->split <= 10) {
    ca->p->split = 10;
  }

  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, drawwins);

  setfocus(focused);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
  return 1;
}

int focusdesktop(Arg *arg) {
#ifdef NWM_DEBUG
  printf("switching desktops...\n");
#endif

  if (arg->i > conf->num_of_desktops) {
    printerr("can't focus desktop (out of range)\n");
    return 1;
  }

  XGrabServer(dpy);
  workspaceswitch = 1;

  deski = arg->i;

  for (int i = 0; i < conf->num_of_desktops; i++) {
    if (i == deski)
      continue;
    looptree(desktops[i].headc, unmapwins);
  }

  looptree(desktops[deski].headc, drawwins);
#ifdef NWM_DEBUG
  looptree(desktops[deski].headc, printbsptree);
#endif

  XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
      PropModeReplace, (const unsigned char *)&deski, 1);

  setfocus(desktops[deski].focused);

  workspaceswitch = 0;
  XUngrabServer(dpy);
  XSync(dpy, True);

#ifdef NWM_DEBUG
  printf("focus switched to desktop %ld\n", deski);
#endif
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
  (void)arg;
#ifdef NWM_DEBUG
  printf("killfocused\n");
#endif
  if (!desktops[deski].focused)
    return 0;

  if (!sendevent(desktops[deski].focused, wmatom[WMDelete])) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, desktops[deski].focused->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }

  return 1;
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
  return 1;
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
#ifdef NWM_DEBUG
  printf("setfocus\n");
#endif
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]); // I think this isn't needed
  if (!c || c->win == root)
    return;

  //printf("win: %lx\n", c->win);
  if (c->floating == False) {
    desktops[deski].tilefoc = c;
  } else {
    XRaiseWindow(dpy, c->win);
  }
  desktops[deski].focused = c;

  XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
  XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
      PropModeReplace, (unsigned char *)&c->win, 1);
  sendevent(c, wmatom[WMTakeFocus]); // god know what this does

  looptree(desktops[deski].headc, updateborders);
  updatebordersll(desktops[deski].floating);
}

void updatebordersll(Client *c) {
  while (c != NULL) {
    XSetWindowBorderWidth(dpy, c->win, conf->bord_size);
    XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? conf->bord_foc_col : conf->bord_nor_col));
    c = c->a;
  }
}

int updateborders(Client *c) {
#ifdef NWM_DEBUG
  printf("updateborders\n");
#endif
  if (!c)
    return 0;

  XSetWindowBorderWidth(dpy, c->win, conf->bord_size);
  XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? conf->bord_foc_col : conf->bord_nor_col));
  return 1;
}

int drawwins(Client *c) {
#ifdef NWM_DEBUG
  printf("drawwins\n");
#endif
  if (!c)
    return 0;

  if (c->win != root) {
#ifdef NWM_DEBUG
    printf("win: path=%-8b, win=%-8lx, x=%-4d, y=%-4d, w=%-4d, h=%-4d\n",
        c->path, c->win, c->x, c->y, c->w, c->h);
#endif
    XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    XMapWindow(dpy, c->win);
    return 1;
  }
  return 0;
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

  // https://tronche.com/gui/x/xlib/events/processing-overview.html
  XSelectInput(dpy, root, SubstructureRedirectMask|SubstructureNotifyMask|FocusChangeMask|EnterWindowMask|PropertyChangeMask);

  for (int i = 0; i < LASTEvent; i++)
    handler[i] = voidevent;
  handler[KeyPress] = keypress;
  handler[MapRequest] = maprequest;
  handler[DestroyNotify] = destroynotify;
  handler[UnmapNotify] = unmapnotify;
  handler[EnterNotify] = enternotify;
  handler[FocusIn] = focusin;

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
    .resize_amount = 5,
    .keyslen = 1,
  };
  conf->keys = NULL;

  char **arg;
  arg = malloc(sizeof(char *) * 2);
  arg[0] = "st";
  arg[1] = NULL;
  conf->keys = malloc(sizeof(Key) * conf->keyslen);
  conf->keys[0] = (Key){Mod1Mask, XStringToKeysym("a"), spawn, {.s = arg}};

  /*conf->keys = malloc(sizeof(Key) * conf->keyslen);

  conf->keys[0] = (Key){Mod1Mask, XStringToKeysym("q"), exitwm, {0}};

  arg = malloc(sizeof(char *) * 2);
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
  conf->keys[14] = (Key){Mod1Mask, XStringToKeysym("2"), focusdesktop, {1}};*/

  startserver();

  // desktops
  if (conf->num_of_desktops < 1)
    conf->num_of_desktops = 1;

  desktops = malloc(conf->num_of_desktops * sizeof(Desktop));
  deski = 0;

  for (int i = 0; i < conf->num_of_desktops; i++) {
    desktops[i].headc = createclient();
    desktops[i].floating = NULL;
    desktops[i].focused = NULL;
    desktops[i].tilefoc = NULL;
  }

  conf->desktop_names = "1\0""2\0";
  conf->desktop_names_len = 4;
  // you can't just do sizeof or strlen on the workspace names string

  // grab input
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

  for (;;) {
    XNextEvent(dpy, &ev);
#ifdef NWM_DEBUG
    printf("\nevent rec of type %d ", ev.type);
#endif

    handler[ev.type](&ev);
  }

  printerr("exiting with no errors\n");
  exitwm(0);
  return 0;
}
