
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

static unsigned int nummask = 0;
static unsigned int scrollmask = 0;

 /** subKeyInit {{{
	* @brief Init keys and get modifiers
	**/

void
subKeyInit(void)
{
	XModifierKeymap *modmap = XGetModifierMapping(subtle->disp);
	if(modmap && modmap->max_keypermod > 0)
		{
			const int modmasks[] = { ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
			const KeyCode num_lock = XKeysymToKeycode(subtle->disp, XK_Num_Lock);
			const KeyCode scroll_lock = XKeysymToKeycode(subtle->disp, XK_Scroll_Lock);
			int i, max = (sizeof(modmasks) / sizeof(int)) * modmap->max_keypermod;

			for(i = 0; i < max; i++)
				if(!modmap->modifiermap[i]) continue;
				else if(num_lock && (modmap->modifiermap[i] == num_lock)) nummask = modmasks[i / modmap->max_keypermod];
				else if(scroll_lock && (modmap->modifiermap[i] == scroll_lock)) scrollmask = modmasks[i / modmap->max_keypermod];
		}
	if(modmap) XFreeModifiermap(modmap);
} /* }}} */

 /** subKeyNew {{{
	* @brief Create new key
	* @param[in]  key	   Key name
	* @param[in]  value	 Key action
	* @return Returns a #SubKey or \p NULL
	**/

SubKey *
subKeyNew(const char *key,
	const char *value)
{
	KeySym sym;
	char *tok = strtok((char *)value, "-");
	SubKey *k = (SubKey *)subUtilAlloc(1, sizeof(SubKey));
	k->flags	= SUB_TYPE_KEY;

	/* @todo Too slow? */	
	if(!strncmp(key, "ViewJump", 8))
		{
			char *desktop = (char *)key + 8; ///< Get desktop number
			if(desktop) 
				{
					k->number = atoi(desktop) - 1; ///< Decrease for array index
					k->flags |= SUB_KEY_VIEW_JUMP;
				}
			else 
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					free(k);
					return(NULL);
				}
		}
	else if(!strncmp(key, "ViewMnemonic", 12))
		{
			k->flags |= SUB_KEY_VIEW_MNEMONIC;
		}
	else
		{
			k->flags	|= SUB_KEY_EXEC;
			k->string	= strdup(key);
		}

	while(tok)
		{ 
			/* Get key sym and modifier */
			sym = XStringToKeysym(tok);
			if(NoSymbol == sym)
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					if(k->string) free(k->string);
					free(k);
					return(NULL);
				}

			/* Modifier mappings */
			switch(sym)
				{
					case XK_A: k->mod |= Mod1Mask;			break;
					case XK_S: k->mod |= ShiftMask;			break;
					case XK_C: k->mod |= ControlMask;		break;
					case XK_W: k->mod |= Mod4Mask;			break;
					case XK_M: k->mod |= Mod3Mask;			break;
					default:
						k->code = XKeysymToKeycode(subtle->disp, sym);
				}

			tok = strtok(NULL, "-");
		}
	subUtilLogDebug("code=%03d, mod=%02d, key=%s\n", k->code, k->mod, key);
	
	return(k);
} /* }}} */

 /** subKeyFind {{{
	* @brief Find key
	* @param[in]  code 	A keycode
	* @param[in]  mod	  A modmask
	* @return Returns a #SubKey or \p NULL
	**/

SubKey *
subKeyFind(int code,
	unsigned int mod)
{
	SubKey **ret = NULL, *kp = NULL, k;
	
	k.code	= code;
	k.mod		= (mod & ~(LockMask|nummask|scrollmask));
	kp 			= &k;

	ret = (SubKey **)bsearch(&kp, subtle->keys->data, subtle->keys->ndata, sizeof(SubKey *), subKeyCompare);

	return(ret ? *ret : NULL);
} /* }}} */

 /** subKeyGet {{{
	* @brief Get key
	* @return Returns the keysym of the press key
	**/

KeySym
subKeyGet(void)
{
	XEvent ev;
	KeySym sym = None;

	XMaskEvent(subtle->disp, KeyPressMask, &ev);
	sym = XLookupKeysym(&ev.xkey, 0);

	return(sym);
} /* }}} */

 /** subKeyGrab {{{
	* @brief Grab keys for a window
	* @param[in]  win	 Window
	**/

void
subKeyGrab(Window win)
{
	if(win && subtle->keys)
		{
			int i;

			/* @todo Ugly key/modifier grabbing */
			for(i = 0; i < subtle->keys->ndata; i++) 
				{
					SubKey *k = KEY(subtle->keys->data[i]);

					XGrabKey(subtle->disp, k->code, k->mod, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|LockMask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|nummask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|LockMask|nummask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|scrollmask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|scrollmask|LockMask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|scrollmask|nummask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(subtle->disp, k->code, k->mod|scrollmask|LockMask|nummask, win, True, GrabModeAsync, GrabModeAsync);
				}
			if(subtle->cv->frame == win) XSetInputFocus(subtle->disp, win, RevertToNone, CurrentTime);
			subtle->focus = win; ///< Update focus window
		}
} /* }}} */

 /** subKeyUngrab {{{
	* @brief Ungrab keys for a window
	* @param[in]  win	Window
	**/

void
subKeyUngrab(Window win)
{
	XUngrabKey(subtle->disp, AnyKey, AnyModifier, win);
	subtle->focus = 0; ///< Unset focus window
} /* }}} */

 /** subKeyCompare {{{
	* @brief Compare two keys
	* @param[in]  a	 A #SubKey
	* @param[in]  b	 A #SubKey
	* @return Returns the result of the comparison of both keys
	* @retval  -1  First is smaller
	* @retval  0	 Both are equal	
	* @retval  1   First is greater
	**/

int
subKeyCompare(const void *a,
	const void *b)
{
	int ret;
	SubKey *k1 = *(SubKey **)a, *k2 = *(SubKey **)b;

	assert(a && b);

	/* @todo Complicated.. */
	if(k1->code < k2->code) ret = -1;
	else if(k1->code == k2->code)
		{
			if(k1->mod < k2->mod) ret = -1;
			else if(k1->mod == k2->mod) ret = 0;
			else ret = 1;
		}
	else if(k1->code > k2->code) ret = 1;

	return(ret);
} /* }}} */

 /** subKeyKill {{{
	* @brief Kill key
	**/

void
subKeyKill(SubKey *k)
{
	assert(k);

	if(k->flags & SUB_KEY_EXEC && k->string) free(k->string);
	free(k);
} /* }}} */
