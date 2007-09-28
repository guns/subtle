
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

int size = 0;
SubKey **keys = NULL;

 /**
	* Find a key
	* @param keycode A keycode
	* @return Returns a #SubKey or NULL
	**/

SubKey *
subKeyFind(int keycode,
	int mod)
{
	int i;

	for(i = 0; i < size; i++)
		if(keys[i]->code == keycode && keys[i]->mod == mod) return(keys[i]);
	return(NULL);
}

 /**
	* Parse key chains
	* @key Key name
	* @value Key action
	**/

void
subKeyParseChain(const char *key,
	const char *value)
{
	int mod = 0;
	KeySym sym;
	char *tok = strtok((char *)value, "-");
	SubKey *k = (SubKey *)subUtilAlloc(1, sizeof(SubKey));

	/* FIXME: strncmp() is really slow.. */	
	if(!strncmp(key, "AddVertTile", 11)) 					k->flags = SUB_KEY_ACTION_ADD_VTILE;
	else if(!strncmp(key, "AddHorzTile", 11)) 		k->flags = SUB_KEY_ACTION_ADD_HTILE;
	else if(!strncmp(key, "DeleteWindow", 12))		k->flags = SUB_KEY_ACTION_DELETE_WIN;
	else if(!strncmp(key, "ToggleCollapse", 14))	k->flags = SUB_KEY_ACTION_TOGGLE_COLLAPSE;
	else if(!strncmp(key, "ToggleRaise", 11))			k->flags = SUB_KEY_ACTION_TOGGLE_RAISE;
	else if(!strncmp(key, "ToggleFull", 10))			k->flags = SUB_KEY_ACTION_TOGGLE_FULL;
	else if(!strncmp(key, "ToggleWeight", 12))		k->flags = SUB_KEY_ACTION_TOGGLE_WEIGHT;
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
					case XK_A: mod = ShiftMask;		break;
					case XK_S: mod = ControlMask;	break;
					case XK_C: mod = Mod1Mask;		break;
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

			keys[size] = k;
			size++;

			subUtilLogDebug("Keychain: name=%s, code=%d, mod=%d\n", key, k->code, k->mod);
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
				{
					if(keys[i])
						{
							if(keys[i]->flags == SUB_KEY_ACTION_EXEC && keys[i]->string) free(keys[i]->string);
							free(keys[i]);
						}
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
				XGrabKey(d->dpy, keys[i]->code, keys[i]->mod, w->frame, True, GrabModeAsync, GrabModeAsync);
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
