
 /**
	* @package subtle
	*
	* @file Key functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static unsigned int nummask = 0;
static unsigned int scrollmask = 0;

/* Compare {{{ */
static int
Compare(const void *a,
	const void *b)
{
	SubKey *k1 = *(SubKey **)a, *k2 = *(SubKey **)b;

	assert(a && b);

	/* < -1, = 0, > 1 */
	return(k1->code + k1->mod < k2->code + k2->mod ? -1 : (k1->code + k1->mod == k2->code + k2->mod ? 0 : 1));
} /* }}} */

 /** subKeyInit {{{
	* @brief Init keys and get modifiers
	**/

void
subKeyInit(void)
{
	XModifierKeymap *modmap = XGetModifierMapping(d->disp);
	if(modmap && modmap->max_keypermod > 0)
		{
			const int modmasks[] = { ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
			const KeyCode num_lock = XKeysymToKeycode(d->disp, XK_Num_Lock);
			const KeyCode scroll_lock = XKeysymToKeycode(d->disp, XK_Scroll_Lock);
			int i, max = (sizeof(modmasks) / sizeof(int)) * modmap->max_keypermod;

			for(i = 0; i < max; i++)
				if(!modmap->modifiermap[i]) continue;
				else if(num_lock && (modmap->modifiermap[i] == num_lock)) nummask = modmasks[i / modmap->max_keypermod];
				else if(scroll_lock && (modmap->modifiermap[i] == scroll_lock)) scrollmask = modmasks[i / modmap->max_keypermod];
		}
	if(modmap) XFreeModifiermap(modmap);
} /* }}} */

 /** subKeyNew {{{
	* Create new key
	* @param[in] key Key name
	* @param[in] value Key action
	* @return A #SubKey or \p NULL
	**/

SubKey *
subKeyNew(const char *key,
	const char *value)
{
	int mod = 0;
	KeySym sym;
	char *tok = strtok((char *)value, "-");
	SubKey *k = (SubKey *)subUtilAlloc(1, sizeof(SubKey));
	k->flags |= SUB_TYPE_KEY;

	/* \todo Too slow? */	
	if(!strncmp(key, "FocusAbove", 10))						k->flags |= SUB_KEY_FOCUS_ABOVE;
	else if(!strncmp(key, "FocusBelow", 10))			k->flags |= SUB_KEY_FOCUS_BELOW;
	else if(!strncmp(key, "FocusNext", 9))				k->flags |= SUB_KEY_FOCUS_NEXT;
	else if(!strncmp(key, "FocusPrev", 9))				k->flags |= SUB_KEY_FOCUS_PREV;
	else if(!strncmp(key, "FocusAny", 8))					k->flags |= SUB_KEY_FOCUS_ANY;
	else if(!strncmp(key, "DeleteWindow", 12))		k->flags |= SUB_KEY_DELETE_WIN;
	else if(!strncmp(key, "ToggleCollapse", 14))	k->flags |= SUB_KEY_TOGGLE_SHADE;
	else if(!strncmp(key, "ToggleRaise", 11))			k->flags |= SUB_KEY_TOGGLE_RAISE;
	else if(!strncmp(key, "ToggleFull", 10))			k->flags |= SUB_KEY_TOGGLE_FULL;
	else if(!strncmp(key, "TogglePile", 10))			k->flags |= SUB_KEY_TOGGLE_PILE;
	else if(!strncmp(key, "ToggleLayout", 12))		k->flags |= SUB_KEY_TOGGLE_LAYOUT;
	else if(!strncmp(key, "NextDesktop", 11))			k->flags |= SUB_KEY_DESKTOP_NEXT;
	else if(!strncmp(key, "PreviousDesktop", 11))	k->flags |= SUB_KEY_DESKTOP_PREV;
	else if(!strncmp(key, "MoveToDesktop", 13))
		{
			char *desktop = (char *)key + 13;
			if(desktop) 
				{
					k->number = atoi(desktop);
					k->flags |= SUB_KEY_DESKTOP_MOVE;
				}
			else 
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					return(NULL);
				}
		}
	else
		{
			k->flags	= SUB_KEY_EXEC;
			k->string	= strdup(key);
		}

	while(tok)
		{ 
			/* Get key sym and modifier */
			sym = XStringToKeysym(tok);
			if(sym == NoSymbol) 
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					if(k->string) free(k->string);
					free(k);
					return(NULL);
				}

			/* Modifier mappings */
			switch(sym)
				{
					case XK_A: mod = Mod1Mask;		break;
					case XK_S: mod = ShiftMask;		break;
					case XK_C: mod = ControlMask;	break;
					case XK_W: mod = Mod4Mask;		break;
					case XK_M: mod = Mod3Mask;		break;
				}

			if(mod) 
				{
					k->mod	|= mod;
					mod			= 0;
				}
			else k->code = XKeysymToKeycode(d->disp, sym);

			tok = strtok(NULL, "-");
		}
	
	if(k->code && k->mod)
		{
			subArrayPush(d->keys, (void *)k);
			subUtilLogDebug("code=%03d, mod=%02d, key=%s\n", k->code, k->mod, key);
		}
	
	return(k);
} /* }}} */

	/** subKeySort {{{ 
	 * @brief Sort keys
	 **/

void
subKeySort(void)
{
	assert(d->keys && d->keys->ndata > 0);

	qsort(d->keys->data, d->keys->ndata, sizeof(SubKey *), Compare);

	subUtilLogDebug("sort=%d\n", d->keys->ndata);
} /* }}} */

 /** subKeyFind {{{
	* @brief Find key
	* @param[in] code A keycode
	* @param[in] mod A modmask
	* @return A #SubKey or \p NULL
	**/

SubKey *
subKeyFind(int code,
	unsigned int mod)
{
	SubKey *res, *k = (SubKey *)subUtilAlloc(1, sizeof(SubKey));
	
	k->code = code;
	k->mod	= (mod & ~(LockMask|nummask|scrollmask));

	res = *(SubKey **)bsearch(&k, d->keys->data, d->keys->ndata, sizeof(SubKey *), Compare);

	free(k);

	return(res);
} /* }}} */

 /** subKeyGrab {{{
	* @Grab keys for a window
	* @param[in] win A #Window
	**/

void
subKeyGrab(Window win)
{
	if(win && d->keys)
		{
			int i;

			/* \todo Ugly key/modifier grabbing */
			for(i = 0; i < d->keys->ndata; i++) 
				{
					SubKey *k = (SubKey *)d->keys->data[i];

					XGrabKey(d->disp, k->code, k->mod, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | LockMask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | nummask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | LockMask | nummask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask | LockMask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask | nummask, win, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask | LockMask | nummask, win, True, GrabModeAsync, GrabModeAsync);
				}
		}
} /* }}} */

 /** subKeyUngrab {{{
	* @brief Ungrab keys for a window
	* @param[in] win A #Window
	**/

void
subKeyUngrab(Window win)
{
	XUngrabKey(d->disp, AnyKey, AnyModifier, win);
} /* }}} */

 /** subKeyKill {{{
	* @brief Kill key
	* @param[in]
	**/

void
subKeyKill(SubKey *k)
{
	assert(k);

	if(k->flags & SUB_KEY_EXEC && k->string) free(k->string);
	free(k);
} /* }}} */
