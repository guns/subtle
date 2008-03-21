
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Id$
	**/

#include "subtle.h"

static unsigned int nummask = 0;
static unsigned int scrollmask = 0;

 /** subKeyInit {{{
	* Init keys and get modifiers
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

 /** subKeyFind {{{
	* Find a key
	* @param[in] keycode A keycode
	* @return Success: #SubKey
	* 				Failure: NULL
	**/

SubKey *
subKeyFind(int keycode,
	unsigned int mod)
{
	int i;

	for(i = 0; i < d->keys->ndata; i++)
		{
			SubKey *k = (SubKey *)d->keys->data[i];
			if(k->code == keycode && k->mod == (mod & ~(LockMask|nummask|scrollmask))) return(k);
		}
	return(NULL);
} /* }}} */

 /** subKeyNew {{{
	* Create new key
	* @param[in] key Key name
	* @param[in] value Key action
	**/

void
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
					return;
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
					return;
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
			subUtilLogDebug("Key: name=%s, code=%d, mod=%d\n", key, k->code, k->mod);
		}
} /* }}} */

 /** subKeyKill {{{
	* Kill keys
	* @param[in]
	**/

void
subKeyKill(SubKey *k)
{
	assert(k);

	if(k->flags & SUB_KEY_EXEC && k->string) free(k->string);
	free(k);
} /* }}} */

 /** subKeyGrab {{{
	* Grab keys for a window
	* @param[in] c A #SubClient
	**/

void
subKeyGrab(SubClient *c)
{
	if(c && d->keys)
		{
			int i;

			/* \todo Ugly key/modifier grabbing */
			for(i = 0; i < d->keys->ndata; i++) 
				{
					SubKey *k = (SubKey *)d->keys->data[i];

					XGrabKey(d->disp, k->code, k->mod, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | LockMask, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | nummask, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | LockMask | nummask, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask | LockMask, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask | nummask, c->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->disp, k->code, k->mod | scrollmask | LockMask | nummask, c->frame, True, GrabModeAsync, GrabModeAsync);
				}
		}
} /* }}} */

 /** subKeyUngrab {{{
	* Ungrab keys for a window
	* @param[in] c A #SubClient
	**/

void
subKeyUngrab(SubClient *c)
{
	XUngrabKey(d->disp, AnyKey, AnyModifier, c->frame);
} /* }}} */
