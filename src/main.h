#ifndef NWM_MAIN
#define NWM_MAIN

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "config.h"


#define WM_NAME "nwm"

#define LENGTH(X) (int)(sizeof(X) / sizeof(X[0]))
#define CLEANMASK(mask) (mask & ~(LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; // wmatom
enum { NetSupported, NetWMName, NetActiveWindow, NetWMCheck,
  NetWMStrutPartial,
  NetWMWindowType, NetWMWindowTypeNormal, NetWMWindowTypeDock, NetWMWindowTypePopup,
  NetWMState, NetWMStateAbove,
  NetNumberOfDesktops, NetCurrentDesktop, NetWMDesktop, NetDesktopNames, NetClientList,
  NetLast }; // netatom

typedef struct Client Client;
struct Client {
  Client *a;
  Client *b;
  Client *p;
  Window win;
  unsigned int path;
  int depth;
  int split;
  int x, y, w, h;
  int minw, minh;
  int maxw, maxh;
  Bool floating;
};

typedef struct Desktop {
  Client *headc;
  Client *floating;
  Client *focused;
  Client *tilefoc;
} Desktop;

typedef union Arg {
  int i;
  char **s;
} Arg;

typedef struct Key {
  int mod;
  KeySym keysym;
  int (*func)(Arg *);
  Arg args;
} Key;

typedef struct Config {
  int vgaps;
  int hgaps;
  int bord_size;
  int keyslen;
  int num_of_desktops;
  unsigned int resize_amount;
  long bord_foc_col;
  long bord_nor_col;
  Key *keys;
} Config;


// debug
void printerr(char *errstr);
void printpath(unsigned int path, int depth);
int printbsptree(Client *c);

// helpers
char keysymtostring(XKeyEvent *xkey);
int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype);

// events
void voidevent(XEvent *);
void keypress(XEvent *ev);
void maprequest(XEvent *ev);
void unmapnotify(XEvent *ev);
void destroynotify(XEvent *ev);
void enternotify(XEvent *ev);
void focusin(XEvent *ev);

// client
Client *createclient(void);
void copyclientdata(Client *a, Client *b, Bool win, Bool path, Bool ab);
unsigned int findpath(unsigned int path, int depth, bool dir);
Client *findclientindir(Client *incl, int dir);
int mapwins(Client *c);
void manage(Window w);
int fixchildren(Client *c);
void unmanage(Window destroywin);
void createclientlist(void);

// linked list
Client *findclientll(Client *c, Window win);

// bsp
int looptree(Client *c, int (*func)(Client *));
Client *findclient(Client *c, Window win);
int findclientpath(Client *c, Client **retc);
int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **));
int addtotree(Client *c, Client **newc);

// keypress
int focusswitch(Arg *arg);
int resizeclient(Arg *arg);
int focusdesktop(Arg *arg);
int spawn(Arg *arg);
int killfocused(Arg *);
int exitwm(Arg *arg);

// x11
int sendevent(Client *c, Atom proto);
void setfocus(Client *c);
void updatebordersll(Client *c);
int updateborders(Client *c);
int drawwins(Client *c);

// others
void setup(void);
void setupatoms(void);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *, XErrorEvent *);

#endif
