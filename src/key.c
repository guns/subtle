
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int size = 0;
static SubKey **keys = NULL;
static unsigned int num_lock_mask = 0;
static unsigned int scroll_lock_mask = 0;

 /**
	* Init keys and get modifiers
	**/

void
subKeyInit(void)
{
	XModifierKeymap *modmap = XGetModifierMapping(d->dpy);
	if(modmap && modmap->max_keypermod > 0)
		{
			const int modmasks[] = { ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
			const KeyCode num_lock = XKeysymToKeycode(d->dpy, XK_Num_Lock);
			const KeyCode scroll_lock = XKeysymToKeycode(d->dpy, XK_Scroll_Lock);
			int i, size = (sizeof(modmasks) / sizeof(int)) * modmap->max_keypermod;

			for(i = 0; i < size; i++)
				if(!modmap->modifiermap[i]) continue;
				else if(num_lock && (modmap->modifiermap[i] == num_lock)) num_lock_mask = modmasks[i / modmap->max_keypermod];
				else if(scroll_lock && (modmap->modifiermap[i] == scroll_lock)) scroll_lock_mask = modmasks[i / modmap->max_keypermod];
		}
	if(modmap) XFreeModifiermap(modmap);
}

 /**
	* Find a key
	* @param keycode A keycode
	* @return Returns a #SubKey or NULL
	**/

SubKey *
subKeyFind(int keycode,
	unsigned int mod)
{
	int i;

	for(i = 0; i < size; i++)
		if(keys[i]->code == keycode && keys[i]->mod == (mod & ~(LockMask|num_lock_mask|scroll_lock_mask))) return(keys[i]);
	return(NULL);
}

 /**
	* Create new key
	* @key Key name
	* @value Key action
	**/

void
subKeyNew(const char *key,
	const char *value)
{
	int mod = 0;
	KeySym sym;
	char *tok = strtok((char *)value, "-");
	SubKey *k = (SubKey *)subUtilAlloc(1, sizeof(SubKey));

	/* FIXME: strncmp() is really slow.. */	
	if(!strncmp(key, "FocusAbove", 10))						k->flags = SUB_KEY_ACTION_FOCUS_ABOVE;
	else if(!strncmp(key, "FocusBelow", 10))			k->flags = SUB_KEY_ACTION_FOCUS_BELOW;
	else if(!strncmp(key, "FocusNext", 9))				k->flags = SUB_KEY_ACTION_FOCUS_NEXT;
	else if(!strncmp(key, "FocusPrev", 9))				k->flags = SUB_KEY_ACTION_FOCUS_PREV;
	else if(!strncmp(key, "FocusAny", 8))					k->flags = SUB_KEY_ACTION_FOCUS_ANY;
	else if(!strncmp(key, "DeleteWindow", 12))		k->flags = SUB_KEY_ACTION_DELETE_WIN;
	else if(!strncmp(key, "ToggleCollapse", 14))	k->flags = SUB_KEY_ACTION_TOGGLE_COLLAPSE;
	else if(!strncmp(key, "ToggleRaise", 11))			k->flags = SUB_KEY_ACTION_TOGGLE_RAISE;
	else if(!strncmp(key, "ToggleFull", 10))			k->flags = SUB_KEY_ACTION_TOGGLE_FULL;
	else if(!strncmp(key, "TogglePile", 10))			k->flags = SUB_KEY_ACTION_TOGGLE_PILE;
	else if(!strncmp(key, "ToggleLayout", 12))		k->flags = SUB_KEY_ACTION_TOGGLE_LAYOUT;
	else if(!strncmp(key, "NextDesktop", 11))			k->flags = SUB_KEY_ACTION_DESKTOP_NEXT;
	else if(!strncmp(key, "PreviousDesktop", 11))	k->flags = SUB_KEY_ACTION_DESKTOP_PREV;
	else if(!strncmp(key, "MoveToDesktop", 13))
		{
			char *desktop = (char *)key + 13;
			if(desktop) 
				{
					k->number = atoi(desktop);
					k->flags = SUB_KEY_ACTION_DESKTOP_MOVE;
				}
			else 
				{
					subUtilLogWarn("Can't assign keychain `%s'.\n", key);
					return;
				}
		}
	else
		{
			k->flags	= SUB_KEY_ACTION_EXEC;
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
			else k->code = XKeysymToKeycode(d->dpy, sym);

			tok = strtok(NULL, "-");
		}
	
	if(k->code && k->mod)
		{
			keys = (SubKey **)realloc(keys, sizeof(SubKey *) * (size + 1));
			if(!keys) subUtilLogError("Can't alloc memory. Exhausted?\n");

			keys[size++] = k;

			subUtilLogDebug("Key: name=%s, code=%d, mod=%d\n", key, k->code, k->mod);
		}
}

 /**
	* Kill keys
	**/

void
subKeyKill(void)
{
	int i;

	if(keys)
		{
			for(i = 0; i < size; i++) 
				if(keys[i])
					{
						if(keys[i]->flags == SUB_KEY_ACTION_EXEC && keys[i]->string) free(keys[i]->string);
						free(keys[i]);
					}
			free(keys);
		}
}

 /**
	* Grab keys for a window
	* @param  w A #SubWin
	**/

void
subKeyGrab(SubWin *w)
{
	if(w && keys)
		{
			int i;

			for(i = 0; i < size; i++) 
				{
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod, w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | LockMask, w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | num_lock_mask, w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | LockMask | num_lock_mask, 
						w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | scroll_lock_mask, w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | scroll_lock_mask | LockMask, 
						w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | scroll_lock_mask | num_lock_mask, 
						w->frame, True, GrabModeAsync, GrabModeAsync);
					XGrabKey(d->dpy, keys[i]->code, keys[i]->mod | scroll_lock_mask | LockMask | num_lock_mask, 
						w->frame, True, GrabModeAsync, GrabModeAsync);
				}
		}
}

 /** 
	* Ungrab keys for a window
	**/

void
subKeyUngrab(SubWin *w)
{
	XUngrabKey(d->dpy, AnyKey, AnyModifier, w->frame);
}
