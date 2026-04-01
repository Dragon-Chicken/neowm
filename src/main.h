#ifndef NWM_MAIN
#define NWM_MAIN

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>

#define WM_NAME "nwm"

#define LENGTH(X) (int)(sizeof(X) / sizeof(X[0]))
#define CLEANMASK(mask) (mask & ~(LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

// for function returns
// easier to write this way
#define FAIL 0
#define SUC 1

#define NWM_DEBUG

enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; // wmatom
enum { NetSupported, NetWMName, NetActiveWindow, NetWMCheck,
  NetWMStrutPartial,
  NetWMWindowType, NetWMWindowTypeNormal, NetWMWindowTypeDock, NetWMWindowTypePopup,
  NetWMState, NetWMStateAbove,
  NetNumberOfDesktops, NetCurrentDesktop, NetWMDesktop, NetDesktopNames, NetClientList,
  NetLast }; // netatom

enum Dir { dirup, dirdown, dirleft, dirright };

typedef struct Client {
  struct Client *a;
  struct Client *b;
  struct Client *p;
  //struct Client *last; // find out what this was for...
  Window win;
  unsigned int path;
  int depth;
  double split;
  int x, y, w, h;
  int minw, minh;
  int maxw, maxh;
  Bool floating;
} Client;

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
  int num_of_desktops;
  int keyslen;
  int desktop_names_len;
  int min_size;
  double resize_amount;
  long bord_foc_col;
  long bord_nor_col;
  char *desktop_names;
  Key *keys;
} Config;


// debug
#ifdef NWM_DEBUG
void printpath(unsigned int path, int depth);
int printbsptree(Client *c);
void printll(Client *c);
#endif

// helpers
void printerr(char *errstr);
char keysymtostring(XKeyEvent *xkey);
int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype);
void remapwins(void);
void rebindkeys(void);
void unbindkeys(void);

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
unsigned int findpath(unsigned int path, int depth, Bool dir);
Client *findclientindir(Client *incl, enum Dir dir);
int mapwins(Client *c);
void manage(Window w);
// this is for a special case in unmanage()
int fixchildren(Client *c);
void unmanage(Window destroywin);
void createclientlist(void);

// linked list
Client *findclientll(Client *c, Window win);
int addtoll(Client *c, Client *newc);
int floatclient(Arg *arg);
Client *removefromll(Window w);

// bsp
int looptree(Client *c, int (*func)(Client *));
Client *findclient(Client *c, Window win);
int findclientpath(Client *c, Client **retc);
int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **));
int addtotree(Client *c, Client **newc);
Client *removefromtree(Window w);

// keypress
int focusswitch(Arg *arg);
int resizeclient(Arg *arg);
int focusdesktop(Arg *arg);
int spawn(Arg *arg);
int killfocused(Arg *);
int exitwm(Arg *arg);
int focustoggle(Arg *arg);

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
