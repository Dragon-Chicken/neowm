#ifndef NWM_BSP
#define NWM_BSP

int looptree(Client *c, int (*func)(Client *));
int gototree(Client *c, Client **retc, unsigned int path, int depth, int (*func)(Client *, Client **));
Client *findclient(Client *c, Window win);
int findclientpath(Client *c, Client **retc);
int addtotree(Client *headc, Client *newc, Client *focused);
int attachnode(Client *c, Client **newc);
Client *removefromtree(Window win, Bool warp);
int tilewins(Client *c);

#endif
