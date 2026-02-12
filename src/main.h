#ifndef NWM_MAIN_H
#define NWM_MAIN_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>


#define WM_NAME "nwm"

#define LENGTH(X) (int)(sizeof(X) / sizeof(X[0]))
#define CLEANMASK(mask) (mask & ~(LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; // wmatom
enum { NetSupported, NetWMName, NetActiveWindow, NetWMCheck,
  NetWMStrutPartial,
  NetWMWindowType, NetWMWindowTypeNormal, NetWMWindowTypeDock, NetWMWindowTypePopup,
  NetWMState, NetWMStateAbove,
  NetLast }; // netatom

Atom wmatom[WMLast];
Atom netatom[NetLast];

typedef struct Client Client;
struct Client {
  Window win;
  Client *a;
  Client *b;
  unsigned int path;
  int depth;
  int x, y, w, h;
};

typedef union Arg {
  int i;
  char **s;
} Arg;

typedef struct Key {
  int mod;
  KeySym keysym;
  void (*func)(Arg *);
  Arg args;
} Key;

typedef struct Config {
  int vgaps;
  int hgaps;
  int bord_size;
  long bord_foc_col;
  long bord_nor_col;
  int keyslen;
  Key *keys;
} Config;

void printerr(char *errstr);
char keysymtostring(XKeyEvent *xkey);
int getwinprop(Client *c, Atom prop, unsigned long *retatom, unsigned long retatomlen, Atom proptype);
int looptree(Client *c, int (*func)(Client *));
Client *findclient(Client *c, Window win);
int findclientpath(Client *c, Client **retc);
int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **));

void (*handler[LASTEvent])(XEvent*);
void voidevent(XEvent *ev);
void keypress(XEvent *ev);
void maprequest(XEvent *ev);
void unmapnotify(XEvent *ev);
void destroynotify(XEvent *ev);
void enternotify(XEvent *ev);
void focusin(XEvent *ev);

void focusswitch(Arg *arg);
int sendevent(Client *c, Atom proto);
void setfocus(Client *c);
void manage(Window w, XWindowAttributes *wa);
void unmanage(Window destroywin);
int drawwindows(Client *c);
int updateborders(Client *c);
void setup(void);
void setupatoms(void);
void spawn(Arg *arg);
void killfocused(Arg *arg);
void exitwm(Arg *arg);
int (*xerrorxlib)(Display *, XErrorEvent *);
int xerror(Display *dpy, XErrorEvent *ee);
int xerrordummy(Display *dpy, XErrorEvent *ee);

Client *headc;
Client *focused;

Config conf;

Display *dpy;
Window root;

int screenw, screenh;
// ScreenXOFF, ScreenYOFF...
int sxoff, syoff;
int swoff, shoff;

//#define NWM_DEBUG

#endif
