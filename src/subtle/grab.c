
 /**
  * @package subtle
  *
  * @file Key functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

static unsigned int numlockmask = 0;

/* GrabBind {{{ */
static void
GrabBind(SubGrab *g,
  Window win)
{
  int i;
  const unsigned int states[] = { 0, LockMask, numlockmask,
    numlockmask|LockMask };

  /* FIXME: Ugly key/state grabbing */
  for(i = 0; i < LENGTH(states); i++)
    {
      if(g->flags & SUB_GRAB_KEY)
        {
          XGrabKey(subtle->dpy, g->code, g->state|states[i],
            win, True, GrabModeAsync, GrabModeAsync);
        }
      else if(g->flags & SUB_GRAB_MOUSE)
        {
          XGrabButton(subtle->dpy, g->code - XK_Pointer_Button1,
            g->state|states[i], win, False, ButtonPressMask|ButtonReleaseMask,
            GrabModeAsync, GrabModeSync, None, None);
        }
    }
} /* }}} */

/* Public */

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
  * @param[in]   keys       Key chain
  * @param[out]  duplicate  Added twice
  * @return Returns a #SubGrab or \p NULL
  **/

SubGrab *
subGrabNew(const char *keys,
  int *duplicate)
{
  int mouse = False;
  unsigned int code = 0, state = 0;
  KeySym sym = NoSymbol;
  SubGrab *g = NULL;

  assert(keys);

  /* Parse keys */
  if(NoSymbol != (sym = subSharedParseKey(subtle->dpy,
      keys, &code, &state, &mouse)))
    {
      /* Find or create new grab */
      if(!(g = subGrabFind(code, state)))
        {
          g = GRAB(subSharedMemoryAlloc(1, sizeof(SubGrab)));
          g->code  = code;
          g->state = state;
          g->flags = SUB_TYPE_GRAB|(mouse ? SUB_GRAB_MOUSE : SUB_GRAB_KEY);

          if(duplicate) *duplicate = False;
        }
      else if(duplicate) *duplicate = True;

      subSharedLogDebugSubtle("new=grab, type=%s, keys=%s, code=%03d, state=%02d\n",
        g->flags & SUB_GRAB_KEY ? "key" : "mouse", keys, g->code, g->state);
    }
  else subSharedLogWarn("Failed assigning grab `%s'\n", keys);

  return g;
} /* }}} */

 /** subGrabFind {{{
  * @brief Find grab
  * @param[in]  code   A key code
  * @param[in]  state  A key state
  * @return Returns a #SubGrab or \p NULL
  **/

SubGrab *
subGrabFind(int code,
  unsigned int state)
{
  SubGrab **ret = NULL, *gptr = NULL, g;

  /* Find grab via binary search */
  g.code   = code;
  g.state  = (state & ~(LockMask|numlockmask));
  gptr     = &g;
  ret      = (SubGrab **)bsearch(&gptr, subtle->grabs->data, subtle->grabs->ndata,
    sizeof(SubGrab *), subGrabCompare);

  return ret ? *ret : NULL;
} /* }}} */

 /** subGrabSet {{{
  * @brief Grab keys for a window
  * @param[in]  win  Window
  **/

void
subGrabSet(Window win)
{
  if(win)
    {
      int i;

      /* Bind grabs */
      for(i = 0; i < subtle->grabs->ndata; i++)
        {
          SubGrab *g = GRAB(subtle->grabs->data[i]);

          /* Assign only grabs with action */
          if(!(g->flags & (SUB_GRAB_CHAIN_LINK|SUB_GRAB_CHAIN_END)))
            GrabBind(g, win);
        }
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

  /* FIXME Complicated.. */
  if(g1->code < g2->code) ret = -1;
  else if(g1->code == g2->code)
  {
    if(g1->state < g2->state) ret = -1;
    else if(g1->state == g2->state) ret = 0;
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

  /* Delete keys */
  if(g->keys) subArrayKill(g->keys, False);

  free(g);

  subSharedLogDebugSubtle("kill=grab\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
