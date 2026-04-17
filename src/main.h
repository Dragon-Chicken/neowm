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

#define BUTTONMASK (ButtonPressMask|ButtonReleaseMask)

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
  int active; // num of active windows on desktop
} Desktop;

typedef union Arg {
  int i;
  char **s;
} Arg;

typedef struct Key {
  unsigned int mod;
  KeySym keysym;
  int (*func)(Arg *);
  Arg args;
  Bool btn;
} Key;

typedef struct Config {
  unsigned int refreshrate; // in fps
  int vgaps; // in pixels
  int hgaps; // in pixels
  int bord_size; // in pixels
  int num_of_desktops;
  int keyslen;
  int btnslen;
  int min_size; // in pixels
  int move_amount; // in pixels
  int desktop_names_len;
  double split_ratio; // in percent (0.0 to 1.0)
  double resize_amount; // in percent (0.0 - 1.0)
  long bord_foc_col;
  long bord_nor_col;
  char *desktop_names;
  Key *keys;
  Key *btns;
} Config;


// debug
#ifdef NWM_DEBUG
void printpath(unsigned int path, int depth);
int printbsptree(Client *c);
void printll(Client *c);
#endif

// helpers
void printerr(char *errstr);
int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype);
void remapwins(void);
void bindkeys(void);
void unbindkeys(void);
int grabbuttons(Client *c);
int ungrabbuttons(Client *c);
void setdesktops(void);
void warptowin(Client *c);
void checkwinsize(Client *c);

// events
void voidevent(XEvent *);
void keypress(XEvent *ev);
void buttonpress(XEvent *ev);
void buttonrelease(XEvent *ev);
void motionnotify(XEvent *ev);
void maprequest(XEvent *ev);
void unmapnotify(XEvent *ev);
void destroynotify(XEvent *ev);
void enternotify(XEvent *ev);
void focusin(XEvent *ev);
void clientmessage(XEvent *ev);

// client
Client *createclient(void);
void copyclient(Client *a, Client *b, Bool win, Bool pos, Bool path, Bool ab);
unsigned int findpath(unsigned int path, int depth, Bool dir);
Client *findclientindir(Client *incl, enum Dir dir);
long getsizehints(Client *newc);
int fixchildren(Client *c);
int addtoclientlist(Client *c, Window *wins, int numofwins);
void createclientlist(void);
int shouldmanage(Client *c);
void manage(Window w);
void unmanage(Window w);

// linked list
int loopll(Client *c, int (*func)(Client *));
Client *findclientll(Client *c, Window win);
int addtoll(Client **floating, Client *newc, Client *c);
Client *removefromll(Window w, Bool warp);

// bsp
int looptree(Client *c, int (*func)(Client *));
int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **));
Client *findclient(Client *c, Window win);
int findclientpath(Client *c, Client **retc);
int addtotree(Client *headc, Client *newc, Client *focused);
int attachnode(Client *c, Client **newc);
Client *removefromtree(Window w, Bool warp);
int tilewins(Client *c);

// keypress
int focuswindow(Arg *arg);
int swapwindow(Arg *arg);
int movewindow(Arg *arg);
int dragmovewindow(Arg *arg);
int dragresizewindow(Arg *arg);
int resizetiled(Arg *arg);
int resizewindow(Arg *arg);
int focusdesktop(Arg *arg);
int movedesktop(Arg *arg);
int spawn(Arg *arg);
int killfocused(Arg *);
int floattoggle(Arg *arg);
int focustoggle(Arg *arg);
int exitwm(Arg *arg);

// x11
int sendevent(Client *c, Atom proto);
void setfocus(Client *c);
int updateborders(Client *c);
int mapwins(Client *c);
int unmapwins(Client *c);

// others
void setup(void);
void setupatoms(void);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *, XErrorEvent *);
int xerrorstart(Display *dpy, XErrorEvent *ee);
void checkotherwm(void);

#endif
