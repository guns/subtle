
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
			const int modmasks[] = { ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
			const KeyCode numlock = XKeysymToKeycode(subtle->disp, XK_Num_Lock);
			int i, max = (sizeof(modmasks) / sizeof(int)) * modmap->max_keypermod;

			for(i = 0; i < max; i++)
				if(!modmap->modifiermap[i]) continue;
				else if(numlock && (modmap->modifiermap[i] == numlock)) numlockmask = modmasks[i / modmap->max_keypermod];
		}
	if(modmap) XFreeModifiermap(modmap);
} /* }}} */

 /** subGrabNew {{{
	* @brief Create new grab
	* @param[in]  key	   Key name
	* @param[in]  value	 Key action
	* @return Returns a #SubGrab or \p NULL
	**/

SubGrab *
subGrabNew(const char *key,
	const char *value)
{
	KeySym sym;
	char *tok = strtok((char *)value, "-");
	SubGrab *g = NULL;

  assert(key && value);
  
  g = GRAB(subUtilAlloc(1, sizeof(SubGrab)));
	g->flags = SUB_TYPE_GRAB;

	/* @todo Too slow? */	
	if(!strncmp(key, "ViewJump", 8))
		{
			char *desktop = (char *)key + 8; ///< Get desktop number
			if(desktop) 
				{
					g->number = atoi(desktop) - 1; ///< Decrease for array index
					g->flags |= SUB_GRAB_VIEW_JUMP;
				}
			else 
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					free(g);
					return NULL;
				}
		}
  else if(!strncmp(key, "MouseMove", 9))
    {
      g->flags |= SUB_GRAB_MOUSE_MOVE;
    }
  else if(!strncmp(key, "MouseResize", 11))
    {
      g->flags |= SUB_GRAB_MOUSE_RESIZE;
    }
	else
		{
			g->flags	|= SUB_GRAB_EXEC;
			g->string	= strdup(key);
		}

	while(tok)
		{ 
			/* Get key sym and modifier */
			sym = XStringToKeysym(tok);
			if(NoSymbol == sym)
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					if(g->string) free(g->string);
					free(g);
					return NULL;
				}

			/* Modifier mappings */
			switch(sym)
				{
					case XK_A: g->mod |= Mod1Mask;		break;
					case XK_S: g->mod |= ShiftMask;		break;
					case XK_C: g->mod |= ControlMask;	break;
					case XK_W: g->mod |= Mod4Mask;		break;
					case XK_M: g->mod |= Mod3Mask;		break;
					default:
						g->code = XKeysymToKeycode(subtle->disp, sym);
				}

			tok = strtok(NULL, "-");
		}
	subUtilLogDebug("code=%03d, mod=%02d, key=%s\n", g->code, g->mod, key);
	
	return g;
} /* }}} */

 /** subGrabFind {{{
	* @brief Find grab
	* @param[in]  code 	A code
	* @param[in]  mod	  A modmask
	* @return Returns a #SubGrab or \p NULL
	**/

SubGrab *
subGrabFind(int code,
	unsigned int mod)
{
	SubGrab **ret = NULL, *gp = NULL, g;
	
	g.code = code;
	g.mod	 = (mod & ~(LockMask|numlockmask));
	gp 		 = &g;
	ret    = (SubGrab **)bsearch(&gp, subtle->grabs->data, subtle->grabs->ndata, sizeof(SubGrab *), subGrabCompare);

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
	* @param[in]  win	 Window
	**/

void
subGrabSet(Window win)
{
	if(win && subtle->grabs)
		{
			int i, j;
      unsigned int modifiers[4] = { 0, LockMask, numlockmask, numlockmask|LockMask };

			/* @todo Ugly key/modifier grabbing */
			for(i = 0; i < subtle->grabs->ndata; i++) 
				{
					SubGrab *g = GRAB(subtle->grabs->data[i]);

          for(j = 0; 4 > j; j++)
					  XGrabKey(subtle->disp, g->code, g->mod|modifiers[j], win, True, 
              GrabModeAsync, GrabModeAsync);
				}
			if(subtle->cv->frame == win) 
        XSetInputFocus(subtle->disp, win, RevertToNone, CurrentTime);
			subtle->focus = win; ///< Update focus window

      XGrabButton(subtle->disp, AnyButton, AnyModifier, win, False,
        ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeSync, None, None);
		}
} /* }}} */

 /** subGrabUnset {{{
	* @brief Ungrab keys for a window
	* @param[in]  win	Window
	**/

void
subGrabUnset(Window win)
{
	XUngrabKey(subtle->disp, AnyKey, AnyModifier, win);
	subtle->focus = 0; ///< Unset focus window
} /* }}} */

 /** subGrabCompare {{{
	* @brief Compare two grabs
	* @param[in]  a	 A #SubGrab
	* @param[in]  b	 A #SubGrab
	* @return Returns the result of the comparison of both grabs
	* @retval  -1  First is smaller
	* @retval  0	 Both are equal	
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
