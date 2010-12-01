
 /**
  * @package subtle
  *
  * @file Key functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

static unsigned int numlockmask = 0;

/* GrabBind {{{ */
static void
GrabBind(SubGrab *g,
  Window win)
{
  int i;
  const unsigned int modifiers[] = { 0, LockMask, numlockmask,
    numlockmask|LockMask };

  /* @todo Ugly key/modifier grabbing */
  for(i = 0; i < LENGTH(modifiers); i++)
    {
      if(g->flags & SUB_GRAB_KEY)
        {
          XGrabKey(subtle->dpy, g->code, g->mod|modifiers[i],
            win, True, GrabModeAsync, GrabModeAsync);
        }
      else if(g->flags & SUB_GRAB_MOUSE)
        {
          XGrabButton(subtle->dpy, g->code - XK_Pointer_Button1,
            g->mod|modifiers[i], win, False,
            ButtonPressMask|ButtonReleaseMask,
            GrabModeAsync, GrabModeSync, None, None);
        }
    }
} /* }}} */

 /** subGrabInit {{{
  * @brief Init grabs and get modifiers
  **/

void
subGrabInit(void)
{
  XModifierKeymap *modmap = XGetModifierMapping(subtle->dpy);
  if(modmap && modmap->max_keypermod > 0)
    {
      const int modmasks[] = { ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask,
        Mod3Mask, Mod4Mask, Mod5Mask };
      const KeyCode numlock = XKeysymToKeycode(subtle->dpy, XK_Num_Lock);
      int i, max = (sizeof(modmasks) / sizeof(int)) * modmap->max_keypermod;

      for(i = 0; i < max; i++)
        if(!modmap->modifiermap[i]) continue;
        else if(numlock && (modmap->modifiermap[i] == numlock))
          numlockmask = modmasks[i / modmap->max_keypermod];
    }
  if(modmap) XFreeModifiermap(modmap);

  subSharedLogDebug("init=grab\n");
} /* }}} */

 /** subGrabNew {{{
  * @brief Create new grab
  * @param[in]  chain  Key chain
  * @param[in]  type   Type
  * @param[in]  data   Data
  * @return Returns a #SubGrab or \p NULL
  **/

SubGrab *
subGrabNew(const char *chain,
  int type,
  SubData data)
{
  int mouse = False;
  unsigned int code = 0, mod = 0;
  KeySym sym = NoSymbol;
  SubGrab *g = NULL;

  assert(chain);

  /* Parse keys */
  if(NoSymbol != (sym = subSharedParseKey(subtle->dpy,
      chain, &code, &mod, &mouse)))
    {
      /* Create new grab */
      g = GRAB(subSharedMemoryAlloc(1, sizeof(SubGrab)));
      g->data   = data;
      g->code   = code;
      g->mod    = mod;
      g->flags |= (SUB_TYPE_GRAB|type|
        (True == mouse ? SUB_GRAB_MOUSE : SUB_GRAB_KEY));

      subSharedLogDebug("new=grab, type=%s, chain=%s, code=%03d, mod=%02d\n",
        g->flags & SUB_GRAB_KEY ? "k" : "m", chain, g->code, g->mod);
    }
  else
    {
      subSharedLogWarn("Failed assigning grab `%s'\n", chain);

      if(type & SUB_GRAB_SPAWN && data.string) free(data.string);
    }

  return g;
} /* }}} */

 /** subGrabFind {{{
  * @brief Find grab
  * @param[in]  code   A code
  * @param[in]  mod    A modmask
  * @return Returns a #SubGrab or \p NULL
  **/

SubGrab *
subGrabFind(int code,
  unsigned int mod)
{
  SubGrab **ret = NULL, *gptr = NULL, g;

  /* Find grab via binary search */
  g.code = code;
  g.mod  = (mod & ~(LockMask|numlockmask));
  gptr   = &g;
  ret    = (SubGrab **)bsearch(&gptr, subtle->grabs->data, subtle->grabs->ndata,
    sizeof(SubGrab *), subGrabCompare);

  return ret ? *ret : NULL;
} /* }}} */

 /** subGrabSet {{{
  * @brief Grab keys for a window
  * @param[in]  win  Window
  * @param[in]  all  Bind all grabs
  **/

void
subGrabSet(Window win,
  int all)
{
  if(win)
    {
      if(all)
        {
          int i;

          /* Bind all grabs */
          for(i = 0; i < subtle->grabs->ndata; i++)
            GrabBind(GRAB(subtle->grabs->data[i]), win);
        }
      else GrabBind(subtle->escape, win); ///< Bind escape grab only
    }
} /* }}} */

 /** subGrabUnset {{{
  * @brief Ungrab keys for a window
  * @param[in]  win  Window
  **/

void
subGrabUnset(Window win)
{
  XUngrabKey(subtle->dpy, AnyKey, AnyModifier, win);
  XUngrabButton(subtle->dpy, AnyButton, AnyModifier, win);
} /* }}} */

 /** subGrabCompare {{{
  * @brief Compare two grabs
  * @param[in]  a   A #SubGrab
  * @param[in]  b   A #SubGrab
  * @return Returns the result of the comparison of both grabs
  * @retval  -1  First is smaller
  * @retval  0   Both are equal
  * @retval  1   First is greater
  **/

int
subGrabCompare(const void *a,
  const void *b)
{
  int ret = 0;
  SubGrab *g1 = *(SubGrab **)a, *g2 = *(SubGrab **)b;

  assert(a && b);

  /* @todo Complicated.. */
  if(g1->code < g2->code) ret = -1;
  else if(g1->code == g2->code)
  {
    if(g1->mod < g2->mod) ret = -1;
    else if(g1->mod == g2->mod) ret = 0;
    else ret = 1;
  }
  else if(g1->code > g2->code) ret = 1;

  return ret;
} /* }}} */

 /** subGrabKill {{{
  * @brief Kill grab
  * @param[in]  g  A #SubGrab
  **/

void
subGrabKill(SubGrab *g)
{
  assert(g);

  /* Clean certain types */
  if(g->flags & (SUB_RUBY_DATA|SUB_GRAB_PROC) && g->data.num)
    {
      subRubyRelease(g->data.num); ///< Free ruby proc
    }
  else if(g->flags & (SUB_GRAB_SPAWN|SUB_GRAB_WINDOW_GRAVITY) && g->data.string)
    free(g->data.string);

  free(g);

  subSharedLogDebug("kill=grab\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
