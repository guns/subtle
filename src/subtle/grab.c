
 /**
  * @package subtle
  *
  * @file Key functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

static unsigned int numlockmask = 0;

 /** subGrabInit {{{
  * @brief Init grabs and get modifiers
  **/

void
subGrabInit(void)
{
  XModifierKeymap *modmap = XGetModifierMapping(subtle->disp);
  if(modmap && modmap->max_keypermod > 0)
    {
      const int modmasks[] = { ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask,
        Mod3Mask, Mod4Mask, Mod5Mask };
      const KeyCode numlock = XKeysymToKeycode(subtle->disp, XK_Num_Lock);
      int i, max = (sizeof(modmasks) / sizeof(int)) * modmap->max_keypermod;

      for(i = 0; i < max; i++)
        if(!modmap->modifiermap[i]) continue;
        else if(numlock && (modmap->modifiermap[i] == numlock))
          numlockmask = modmasks[i / modmap->max_keypermod];
    }
  if(modmap) XFreeModifiermap(modmap);
} /* }}} */

 /** subGrabNew {{{
  * @brief Create new grab
  * @param[in]  name   Grab name
  * @param[in]  value   Grab action
  * @return Returns a #SubGrab or \p NULL
  **/

SubGrab *
subGrabNew(const char *name,
  const char *value)
{
  int i;
  char *tok = NULL;
  KeySym sym;
  SubGrab *g = NULL;

  struct
  {
    char *name;
    FLAGS flags;
  } grabs[] = { 
    { "WindowMove",   SUB_GRAB_WINDOW_MOVE   },
    { "WindowResize", SUB_GRAB_WINDOW_RESIZE },
    { "WindowFloat",  SUB_GRAB_WINDOW_FLOAT  },
    { "WindowFull",   SUB_GRAB_WINDOW_FULL   },
    { "WindowKill",   SUB_GRAB_WINDOW_KILL   }
  };

  assert(name && value);
  
  g = GRAB(subUtilAlloc(1, sizeof(SubGrab)));

  /* Find grabs */  
  for(i = 0; 5 > i; i++)
    if(!strcmp(name, grabs[i].name))
      {
        g->flags |= (SUB_TYPE_GRAB|grabs[i].flags);
        break; ///< Found
      }

  /* Special grabs */
  if(!g->flags)
    {
      if(!strncmp(name, "ViewJump", 8))
        {
          char *desktop = (char *)name + 8; ///< Get view number
          if(desktop) 
            {
              g->number = atoi(desktop) - 1; ///< Decrease for array index
              g->flags |= (SUB_TYPE_GRAB|SUB_GRAB_VIEW_JUMP);
            }
          else 
            {
              subUtilLogWarn("Can't assign keychain `%s'.\n", name);
              free(g);

              return NULL;
            }
        }
      else
        {
          g->flags |= (SUB_TYPE_GRAB|SUB_GRAB_EXEC);
          g->string  = strdup(name);
        }
    }

  tok = strtok((char *)value, "-");
  while(tok)
    { 
      /* Get key sym and modifier */
      sym = XStringToKeysym(tok);
      if(NoSymbol == sym)
        {
          char *mouse[] = { "B1", "B2", "B3", "B4", "B5" };

          for(i = 0; 5 > i; i++)
            if(!strncmp(tok, mouse[i], 2))
              {
                sym = XK_Pointer_Button1 + i + 1; ///< @todo Implementation independent?
                break;
              }

          if(NoSymbol == sym) ///< Check if there's still no symbol
            {
              subUtilLogWarn("Can't assign keychain `%s'.\n", name);
              if(g->string) free(g->string);
              free(g);
              
              return NULL;
            }
        }

      /* Modifier mappings */
      switch(sym)
        {
          /* Keys */
          case XK_A: g->mod |= Mod1Mask;    break;
          case XK_S: g->mod |= ShiftMask;    break;
          case XK_C: g->mod |= ControlMask;  break;
          case XK_W: g->mod |= Mod4Mask;    break;
          case XK_M: g->mod |= Mod3Mask;    break;

          /* Mouse */
          case XK_Pointer_Button1:
          case XK_Pointer_Button2:
          case XK_Pointer_Button3:
          case XK_Pointer_Button4:
          case XK_Pointer_Button5:
            g->flags |= SUB_GRAB_MOUSE;
            g->code  = sym;
            break;
          default: 
            g->flags |= SUB_GRAB_KEY;
            g->code  = XKeysymToKeycode(subtle->disp, sym);
        }

      tok = strtok(NULL, "-");
    }
  subUtilLogDebug("type=%s, name=%s, code=%03d, mod=%02d\n",
    g->flags & SUB_GRAB_KEY ? "k" : "m", name, g->code, g->mod);
  
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
  SubGrab **ret = NULL, *gp = NULL, g;
  
  g.code = code;
  g.mod   = (mod & ~(LockMask|numlockmask));
  gp      = &g;
  ret    = (SubGrab **)bsearch(&gp, subtle->grabs->data, subtle->grabs->ndata,
    sizeof(SubGrab *), subGrabCompare);

  return ret ? *ret : NULL;
} /* }}} */

 /** subGrabGet {{{
  * @brief Get grab
  * @return Returns the keysym of the press key
  **/

KeySym
subGrabGet(void)
{
  XEvent ev;
  KeySym sym = None;

  XMaskEvent(subtle->disp, KeyPressMask, &ev);
  sym = XLookupKeysym(&ev.xkey, 0);

  return sym;
} /* }}} */

 /** subGrabSet {{{
  * @brief Grab keys for a window
  * @param[in]  win   Window
  **/

void
subGrabSet(Window win)
{
  if(win && subtle->grabs)
    {
      int i, j;
      unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };

      /* @todo Ugly key/modifier grabbing */
      for(i = 0; i < subtle->grabs->ndata; i++) 
        {
          SubGrab *g = GRAB(subtle->grabs->data[i]);

          for(j = 0; LENGTH(modifiers) > j; j++)
            {
              if(g->flags & SUB_GRAB_KEY)
                {
                  XGrabKey(subtle->disp, g->code, g->mod|modifiers[j], win, True,
                    GrabModeAsync, GrabModeAsync);
                }
              else if(g->flags & SUB_GRAB_MOUSE)
                {
                  XGrabButton(subtle->disp, g->code - XK_Pointer_Button1,
                    g->mod|modifiers[j], win, False, ButtonPressMask|ButtonReleaseMask,
                    GrabModeAsync, GrabModeSync, None, None);
                }
            }
        }
      if(subtle->cv->frame == win) 
        XSetInputFocus(subtle->disp, win, RevertToNone, CurrentTime);
      subtle->windows.focus = win; ///< Update focus window
    }
} /* }}} */

 /** subGrabUnset {{{
  * @brief Ungrab keys for a window
  * @param[in]  win  Window
  **/

void
subGrabUnset(Window win)
{
  XUngrabKey(subtle->disp, AnyKey, AnyModifier, win);
  XUngrabButton(subtle->disp, AnyButton, AnyModifier, win);
  subtle->windows.focus = 0; ///< Unset focus window
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
  int ret;
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
  **/

void
subGrabKill(SubGrab *g)
{
  assert(g);

  if(g->flags & SUB_GRAB_EXEC && g->string) free(g->string);
  free(g);
} /* }}} */
