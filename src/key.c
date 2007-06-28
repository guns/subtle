
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

int
GetModifier(KeySym sym)
{
	if(sym == XK_Shift_L || sym == XK_Shift_R) return(ShiftMask);
	else if(sym == XK_Control_L || sym == XK_Control_R) return(ControlMask);
	else if(sym == XK_Meta_L || sym == XK_Meta_R) return(Mod1Mask);
	else if(sym == XK_Alt_L || sym == XK_Alt_R) return(Mod1Mask);
	else if(sym == XK_Num_Lock) return(Mod2Mask);
	else if(sym == XK_Super_L || XK_Super_R) return(Mod4Mask);
	else if(sym == XK_Hyper_L || XK_Hyper_L) return(Mod4Mask);
	else if(sym == XK_ISO_Level3_Shift) return(Mod5Mask);
	else return(AnyModifier);
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
	KeySym sym;
	char *tok = strtok((char *)value, "+");
	SubKey *k = (SubKey *)calloc(1, sizeof(SubKey));
	if(!k) subLogError("Can't alloc memory. Exhausted?\n");

	keys = (SubKey **)realloc(keys, sizeof(SubKey *) * (size + 1));
	if(!keys) subLogError("Can't alloc memory. Exhausted?\n");

	keys[size] = k;

	while(tok)
		{
			sym = XStringToKeysym(tok);
			if(sym == NoSymbol) 
				{
					subLogWarn("Can't assign keychain `%s'.\n", key);
					return;
				}
			if(IsModifierKey(sym)) k->mod |= GetModifier(sym);
			else k->code = XKeysymToKeycode(d->dpy, sym);

			/* FIXME: strncmp() is way to slow.. */	
			if(!strncmp(key, "add_vtile", 9)) 					k->flags = SUB_KEY_ACTION_ADD_VTILE;
			else if(!strncmp(key, "add_htile", 9)) 			k->flags = SUB_KEY_ACTION_ADD_HTILE;
			else if(!strncmp(key, "del_win", 7))				k->flags = SUB_KEY_ACTION_DEL_WIN;
			else if(!strncmp(key, "collapse_win", 12))	k->flags = SUB_KEY_ACTION_COLLAPSE_WIN;
			else if(!strncmp(key, "raise_win", 9))			k->flags = SUB_KEY_ACTION_RAISE_WIN;

			tok = strtok(NULL, "+");
		}

	subLogDebug("Parsing keychain: name=%s, code=%d, mod=%d\n", key, k->code, k->mod);

	size++;
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
