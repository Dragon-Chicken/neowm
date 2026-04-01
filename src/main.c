#include "main.h"
#include "config.h"
#include <X11/Xlib.h>

Atom wmatom[WMLast];
Atom netatom[NetLast];

Cursor cursor;

Desktop *desktops;
long deski;
Config *conf;
Display *dpy;
Window root;

// for net client list ewmh
int totalwins = 0;

int screenw, screenh;
// for bars and stuff
int sxoff, syoff;
int swoff, shoff;

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
  /*if (c->a)
    printf(", a->path=%-8lb, a->win=%-8lx", c->a->path, c->a->win);
  if (c->a)
    printf(", b->path=%-8lb, b->win=%-8lx", c->b->path, c->b->win);*/
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
  fprintf(stderr, "%s: error: %s", WM_NAME, errstr);
}

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
    return SUC;
  }
  return FAIL;
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

void remapwins(void) {
  printf("flushx11\n");
  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, drawwins);
  looptree(desktops[deski].headc, updateborders);
  updatebordersll(desktops[deski].floating);
  XSync(dpy, False); // updates x11
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

  if (ev->xfocus.window == root) {
    setfocus(NULL);
    return;
  }

  if (desktops[deski].focused && ev->xfocus.window != desktops[deski].focused->win) {
    printf("setting focus\n");
    setfocus(desktops[deski].focused);
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
    a->minw = b->minw;
    a->minh = b->minh;
    a->maxw = b->maxw;
    a->maxh = b->maxh;
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

  /*for (int i = depth; i >= 0; i -= 2) {
    if ((path >> i) & 1) {
      path = path & ~(1 << i);
      if (!dir)
        break;
    } else {
      path = path | (1 << i);
      if (dir)
        break;
    }
  }*/

/*
  cl->path: 0000000000000001
  destpath: 1010101010101010101010101010100
  cl->path: 0000000000000000
  destpath: 0000000000000010
  cl->path: 0000000000000010
*/

  return path;
}

Client *findclientindir(Client *incl, enum Dir dir) {
#ifdef NWM_DEBUG
  printf("findclientdir\n");
#endif
  unsigned int destpath = 0;
  Client *cl;

  switch (dir) {
    case dirup: // k/up
      destpath = findpath(incl->path >> 1, incl->depth - 1, 1);
      destpath <<= 1;
      destpath |= incl->path & 1;
      break;
    case dirdown: // j/down
      destpath = findpath(incl->path >> 1, incl->depth - 1, 0);
      destpath <<= 1;
      destpath |= incl->path & 1;
      break;
    case dirleft: // h/left
      destpath = findpath(incl->path, incl->depth, 1);
      break;
    case dirright: // l/right
      destpath = findpath(incl->path, incl->depth, 0);
      break;
  }

  printf("destpath: %.16b\n", destpath);

  if (!gototree(desktops[deski].headc, &cl, destpath, 32, findclientpath)) {
    printf("can't find client\n");
    return NULL;
  }

  printf("cl->path: %.16b\n", cl->path);

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
    // this does not remove client from the tree.
    removefromtree(c->win);
    addtoll(desktops[deski].floating, c);
  }
  return SUC;
}

int unmapwins(Client *c) {
#ifdef NWM_DEBUG
  printf("unmapwins\n");
#endif
  XUnmapWindow(dpy, c->win);
  return SUC;
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

  // set the window border and desktop it belongs to
  XSetWindowBorderWidth(dpy, newc->win, conf->bord_size);
  XChangeProperty(dpy, w, netatom[NetWMDesktop], XA_CARDINAL, 32,
      PropModeReplace, (unsigned char *)&deski, 1);

  // rofi NEEDS this (otherwise it will not close properly...)
  // should try to get rid of this
  Atom protos[] = {wmatom[WMDelete]};
  XSetWMProtocols(dpy, newc->win, protos, 1);

  // use size hints for default size
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

  // if the window doesn't want to be resized (minw == maxw && minh == maxh)
  // just make it floating
  if (newc->maxw == newc->minw && newc->maxh == newc->minh) {
    addtoll(desktops[deski].floating, newc);
    return;
  }

  // tile the window
  if (focused) {
    newc->path = focused->path;
    newc->depth = focused->depth;
    if (focused->p)
      newc->p = focused->p;
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
    printerr("failed to add to tree\n");
    exitwm(NULL);
  }

  // for some client list (ewmh)
  totalwins++;
  //createclientlist();
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

void unmanage(Window w) {
#ifdef NWM_DEBUG
  printf("unmanage\n");
#endif
  // this function has bugs!
  // may be fixed with the function above (fixchildren)?
  // nope there is a bug with focusing and maybe pointers to freed memory

  Client *c = NULL;
  if ((c = removefromtree(w)) == NULL) {
    printerr("can't remove from tree\n");
    if ((c = removefromll(w)) == NULL) {
      printerr("can't remove from ll\n");
      return;
    }
  }

  free(c);
  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, drawwins);

  totalwins--;
  //createclientlist();
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

int addtoll(Client *c, Client *newc) {
  if (!newc) {
    return FAIL;
  }

  newc->floating = True;
  newc->a = NULL;
  newc->b = NULL;
  newc->p = NULL;

  if (!c) {
    if (!desktops[deski].floating) {
      desktops[deski].floating = newc;
      drawwins(newc);
      return SUC;
    }
    c = desktops[deski].floating;
  }

  while (c->a)
    c = c->a;

  // a forward
  // b backward
  c->a = newc;
  newc->b = c;

  drawwins(newc); // just draws the new one

  return SUC;
}

Client *removefromll(Window w) {
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
    printf("can't find window\n");
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
    if (c->b) {
      setfocus(desktops[dt].focused);
    } else {
      setfocus(desktops[dt].tilefoc);
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
  return SUC;
}

int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **)) {
#ifdef NWM_DEBUG
  printf("gototree\n");
#endif
  if (depth < 0) {
    printf("depth < 0\n");
    return FAIL;
  }
  if (!c) {
    printf("c is null\n");
    return FAIL;
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
    return FAIL;
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
  return SUC;
}

Client *removefromtree(Window w) {
  // this function won't free the client, it just removes it from the tree and returns it

  if (!w)
    return NULL;

  if (w == root) {
    printf("root\n");
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
    setfocus(desktops[dt].focused);

    totalwins--;
    //createclientlist();
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
    printf("##########\nc == prevc (this is an error state in unmanage)\n##########\n");
    exitwm(NULL);
  }

  if (!prevc->a && !prevc->b) {
    printf("doesn't have both children\n");
    exitwm(NULL);
  }

  if (prevc->a == c) {
    copyclientdata(prevc, prevc->b, True, False, True);
  } else {
    copyclientdata(prevc, prevc->a, True, False, True);
  }

  looptree(prevc, fixchildren);

  if (c == focused)
    setfocus(prevc);

  return c; // caller should free this
}


// keypress
int focusswitch(Arg *arg) {
#ifdef NWM_DEBUG
  printf("focusswitch\n");
#endif

  Client *focused = desktops[deski].focused;

  if (!focused || !focused->p || focused->floating)
    return FAIL;

  desktops[deski].focused = findclientindir(focused, arg->i);
  focused = desktops[deski].focused;
  setfocus(focused);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));

  return SUC;
}

int resizeclient(Arg *arg) {
#ifdef NWM_DEBUG
  printf("resizeclient\n");
#endif
  int dir = arg->i;

  static int iters;
  static Client *focused;
  if (iters == 0) {
    focused = desktops[deski].focused;
  }

  iters++;

  if (!focused->p) {
#ifdef NWM_DEBUG
    printf("focused doesn't have a parent\n");
#endif
    return FAIL;
  }

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
    if (resizeclient(arg)) {
      focused = tempfoc;
      return SUC;
    }
    focused = tempfoc;
  }

  iters = 0;

  /*
   * 1920
   * 1920 * 0.5 = 960
   * 960 * 0.5 = 480
   *
   * 1920 * 0.5 * 0.5 = 1920 * (1920/960) = 480
   * 1920/960 = 960/480
   *
   * 1920 * x = 10
   * 10 / 1920 = ~10 (9.6)
   *
   * pixels/screenw * screenw/windoww
   */

  if (dir >= 2) {
    dir -= 2;
  }

  if (ca->depth & 1) {
    double scrwinratio = (double)screenw/(double)ca->p->w;
    double pixeltopercent = scrwinratio * conf->min_size/screenw;

    ca->p->split += conf->resize_amount * scrwinratio * ((dir * 2) - 1);

    if (ca->p->split <= pixeltopercent)
      ca->p->split = pixeltopercent;
    else if (ca->p->split >= 1.0f - pixeltopercent)
      ca->p->split = 1.0f - pixeltopercent;
  } else {
    double scrwinratio = (double)screenh/(double)ca->p->h;
    double pixeltopercent = scrwinratio * conf->min_size/screenh;

    ca->p->split += conf->resize_amount * ((double)screenh/(double)ca->p->h) * ((dir * 2) - 1);

    if (ca->p->split <= pixeltopercent)
      ca->p->split = pixeltopercent;
    else if (ca->p->split >= 1.0f - pixeltopercent)
      ca->p->split = 1.0f - pixeltopercent;
  }


  /*if ((ca->w >= ca->p->w) || (ca->h >= ca->p->h)) {
    ca->p->split -= conf->resize_amount * ((dir * 2) - 1);
  }*/

  // should prob change this to also be relative to the screen
  /*if (ca->p->split >= 0.9f) {
    ca->p->split = 0.9f;
  } else if (ca->p->split <= 0.1f) {
    ca->p->split = 0.1f;
  }*/

  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, drawwins);

  setfocus(focused);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));
  return SUC;
}

int focusdesktop(Arg *arg) {
#ifdef NWM_DEBUG
  printf("switching desktops...\n");
#endif

  if (arg->i > conf->num_of_desktops) {
    printerr("can't focus desktop (out of range)\n");
    return FAIL;
  }

  XGrabServer(dpy);

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

  XUngrabServer(dpy);
  XSync(dpy, True);

#ifdef NWM_DEBUG
  printf("focus switched to desktop %ld\n", deski);
#endif
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

  return SUC;
}

int killfocused(Arg *arg) {
  (void)arg;
#ifdef NWM_DEBUG
  printf("killfocused\n");
#endif
  if (!desktops[deski].focused)
    return FAIL;

  if (sendevent(desktops[deski].focused, wmatom[WMDelete]) == 0) {
    printf("failed to send event\n");
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

int floatclient(Arg *arg) {
  (void)arg;

  //looptree(desktops[deski].headc, printbsptree);

  if (!desktops[deski].focused)
    return FAIL;

  Client *c = removefromtree(desktops[deski].focused->win);

  if (!c)
    return FAIL;

  c->floating = True;
  c->a = NULL;
  c->b = NULL;
  c->p = NULL;

  addtoll(desktops[deski].floating, c);

  setfocus(c);

  //looptree(desktops[deski].headc, printbsptree);

  looptree(desktops[deski].headc, mapwins);
  looptree(desktops[deski].headc, drawwins);
  return SUC;
}

int focustoggle(Arg *arg) {
  (void)arg;

  Client *focused = desktops[deski].focused;

  if (!focused)
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
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, focused->x + (focused->w/2), focused->y + (focused->h/2));

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

  //XSetInputFocus(dpy, None, RevertToPointerRoot, CurrentTime);
  //XDeleteProperty(dpy, root, netatom[NetActiveWindow]); // I think this isn't needed
  if (!c || c->win == root) {
    return;
   }

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
    //XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? conf->bord_foc_col : conf->bord_nor_col));
    XSetWindowBorder(dpy, c->win, 0xffff0000);
    c = c->a;
  }
}

int updateborders(Client *c) {
#ifdef NWM_DEBUG
  printf("updateborders\n");
#endif
  if (!c)
    return FAIL;

  XSetWindowBorderWidth(dpy, c->win, conf->bord_size);
  XSetWindowBorder(dpy, c->win, (c == desktops[deski].focused ? conf->bord_foc_col : conf->bord_nor_col));
  return SUC;
}

int drawwins(Client *c) {
#ifdef NWM_DEBUG
  printf("drawwins\n");
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

  conf->keys[11] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("k"), resizeclient, {0}};
  conf->keys[10] = (Key){Mod1Mask|ShiftMask, XStringToKeysym("j"), resizeclient, {1}};
  conf->keys[9]  = (Key){Mod1Mask|ShiftMask, XStringToKeysym("h"), resizeclient, {2}};
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

    //printf("done\n");
  }

  printerr("exiting with no errors\n");
  exitwm(0);
  return 0;
}
