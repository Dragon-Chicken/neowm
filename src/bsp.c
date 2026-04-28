#include "main.h"
#include "memory.h"
#include "bsp.h"

extern Desktop *desktops;
extern long deski;

extern Config *conf;
extern Window root;
extern int screenw, screenh;
extern int sxoff, syoff;
extern int swoff, shoff;

int looptree(Client *c, int (*func)(Client *)) {
#ifdef NWM_DEBUG
  printf("%s\n", __func__);
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
  printf("%s\n", __func__);
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
  printf("%s\n", __func__);
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
  printf("%s\n", __func__);
#endif
  *retc = c;
  return SUC;
}

int attachnode(Client *c, Client **newc) {
#ifdef NWM_DEBUG
  printf("%s\n", __func__);
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
    nwm_free(*newc);
    c->p = NULL;
    *newc = c; // should prob do this because it's freeing memory that does NOT belong to this function
  }
  return SUC;
}

int addtotree(Client *headc, Client *newc, Client *focused) {
#ifdef NWM_DEBUG
  printf("%s\n", __func__);
#endif

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

Client *removefromtree(Window win, Bool warp) {
#ifdef NWM_DEBUG
  printf("%s\n", __func__);
#endif
  // this function won't free the client, it just removes it from the tree and returns it

  if (!win || win == root)
    return NULL;

  Client *c = NULL;
  Client *headc = NULL;
  Client *focused = NULL;
  int dt = 0; // DeskTop of deleted win

  for (dt = 0; dt < conf->num_of_desktops; dt++) {
    c = findclient(desktops[dt].headc, win);
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

  Client *tofree = NULL;
  if (prevc->a == c) {
    tofree = prevc->b;
    copyclient(prevc, prevc->b, True, True, False, True);
  } else {
    tofree = prevc->a;
    copyclient(prevc, prevc->a, True, True, False, True);
  }

  nwm_free(tofree);

  looptree(prevc, fixchildren);

  if (c == focused) {
    if (prevc->win == root) {
      prevc = prevc->a;
    }
    setfocus(prevc);

    if (warp)
      warptowin(prevc);

    if (prevc->win == c->win) {
      exitwm(0);
    }
  }

  return c; // caller should free this
}

int tilewins(Client *c) {
#ifdef NWM_DEBUG
  printf("%s\n", __func__);
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

  checkwinsize(c);
  return SUC;
}
