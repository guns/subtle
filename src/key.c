
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License along
	* with this program; if not, write to the Free Software Foundation, Inc.,
	* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
	int mod;
	KeySym sym;
	char *tok = strtok((char *)value, "-");
	SubKey *k = (SubKey *)calloc(1, sizeof(SubKey));
	if(!k) subLogError("Can't alloc memory. Exhausted?\n");

	/* FIXME: strncmp() is way to slow.. */	
	if(!strncmp(key, "AddVertTile", 11)) 					k->flags = SUB_KEY_ACTION_ADD_VTILE;
	else if(!strncmp(key, "AddHorzTile", 11)) 		k->flags = SUB_KEY_ACTION_ADD_HTILE;
	else if(!strncmp(key, "DeleteWindow", 12))		k->flags = SUB_KEY_ACTION_DELETE_WIN;
	else if(!strncmp(key, "ToggleCollapse", 14))	k->flags = SUB_KEY_ACTION_TOGGLE_COLLAPSE;
	else if(!strncmp(key, "ToggleRaise", 11))			k->flags = SUB_KEY_ACTION_TOGGLE_RAISE;
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
					subLogWarn("Can't assign keychain `%s'.\n", key);
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
			if(*tok == 'S') 			{ sym = XK_Shift_L;		mod = ShiftMask; 		}
			else if(*tok == 'C')	{ sym = XK_Control_L;	mod = ControlMask;	}
			else if(*tok == 'A')	{ sym = XK_Alt_L;			mod = Mod1Mask;			}
			else 
				{
					sym = XStringToKeysym(tok);
					if(sym == NoSymbol) 
						{
							subLogWarn("Can't assign keychain `%s'.\n", key);
							if(k->string) free(k->string);
							free(k);
							return;
						}
				}
			if(IsModifierKey(sym)) k->mod |= mod;
			else k->code = XKeysymToKeycode(d->dpy, sym);

			tok = strtok(NULL, "-");
		}
	
	if(k->code && k->mod)
		{
			keys = (SubKey **)realloc(keys, sizeof(SubKey *) * (size + 1));
			if(!keys) subLogError("Can't alloc memory. Exhausted?\n");

			keys[size] = k;
			size++;

			printf("Keychain: name=%s, code=%d, mod=%d\n", key, k->code, k->mod);
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
					if(keys[i]->flags == SUB_KEY_ACTION_EXEC && keys[i]->string) 
						free(keys[i]->string);
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
	int i;

	if(w && keys)
		{
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
