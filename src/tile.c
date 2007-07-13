
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

 /**
	* Create a new tile 
	* @param mode Tile mode
	* @return Return either a #SubWin on success or otherwise NULL.
	**/

SubWin *
subTileNew(short mode)
{
	SubWin *w = subWinNew(0);

	w->flags	= SUB_WIN_TYPE_TILE;
	w->flags |= mode;
	w->tile		= (SubTile *)calloc(1, sizeof(SubTile));
	if(!w->tile) subLogError("Can't alloc memory. Exhausted?\n");

	w->tile->btnew	= XCreateSimpleWindow(d->dpy, w->frame, 
		d->th, 0, 7 * d->fx, d->th, 0, d->colors.border, d->colors.norm);
	w->tile->btdel	= XCreateSimpleWindow(d->dpy, w->frame, 
		d->th + 7 * d->fx, 0, 7 * d->fx, d->th, 0, d->colors.border, d->colors.norm);

	subWinMap(w);
	subLogDebug("Adding %s-tile: x=%d, y=%d, width=%d, height=%d\n", 
		(w->flags & SUB_WIN_TILE_HORZ) ? "h" : "v", w->x, w->y, w->width, w->height);

	return(w);
}

 /**
	* Delete a tile window and all children 
	* @param w A #SubWin
	**/

void
subTileDelete(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_TILE)
		{
			unsigned int n, i;
			Window nil, *wins = NULL;

			XQueryTree(d->dpy, w->win, &nil, &nil, &wins, &n);	
			for(i = 0; i < n; i++)
				{
					SubWin *c = subWinFind(wins[i]);
					if(c) subWinDelete(c);
				}

			subLogDebug("Deleting %s-tile with %d children\n", (w->flags & SUB_WIN_TILE_HORZ) ? "h" : "v", n);

			free(w->tile); 
			XFree(wins);
		}
}

 /**
	* Render a tile window 
	* @param w A #SubWin
	**/

void
subTileRender(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_TILE)
		{
			unsigned long col = d->focus && d->focus == w ? d->colors.focus : 
				(w->flags & SUB_WIN_OPT_COLLAPSE ? d->colors.cover : d->colors.norm);

			XSetWindowBackground(d->dpy, w->tile->btnew, col);
			XSetWindowBackground(d->dpy, w->tile->btdel, col);
			XClearWindow(d->dpy, w->tile->btnew);
			XClearWindow(d->dpy, w->tile->btdel);

			/* Descriptive buttons */
			XDrawString(d->dpy, w->tile->btnew, d->gcs.font, 3, d->fy - 1, 
				(w->flags & SUB_WIN_TILE_HORZ ? "Newrow" : "Newcol"), 6);
			XDrawString(d->dpy, w->tile->btdel, d->gcs.font, 3, d->fy - 1, 
				(w->parent && w->parent->flags & SUB_WIN_TILE_HORZ ? "Delrow" : "Delcol"), 6);
		}
}

 /**
	* Add a window to a tile 
	* @param t A #SubWin
	* @param w A #SubWin
	**/

void
subTileAdd(SubWin *t,
	SubWin *w)
{
	if(t && w)
		{
			XReparentWindow(d->dpy, w->frame, t->win, 0, 0);
			subWinRestack(w);
			subTileConfigure(t);
			w->parent = t;

			subLogDebug("Adding window: x=%d, y=%d, width=%d, height=%d\n", w->x, w->y, w->width, w->height);
		}
}

 /**
	* Configure a tile and each child 
	* @param w A #SubWin
	**/

void
subTileConfigure(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_TILE)
		{
			int mw = 0, mh = 0, width = SUBWINWIDTH(w), height = SUBWINHEIGHT(w);
			unsigned int x = 0, y = 0, i = 0, n = 0, size = 0, comp = 0, collapsed = 0, weighted = 0, full = 0;
			Window nil, *wins = NULL;

			XQueryTree(d->dpy, w->win, &nil, &nil, &wins, &n);
			if(n > 0)
				{
					size = (w->flags & SUB_WIN_TILE_HORZ) ? width : height;

					/* Find weighted and collapsed windows */
					for(i = 0; i < n; i++)
						{
							SubWin *c = subWinFind(wins[i]);
							if(c)
								{
									if(c->flags & SUB_WIN_OPT_COLLAPSE) collapsed++;
									else if(c->flags & SUB_WIN_OPT_FULL) full++;
									else if(c->flags & SUB_WIN_OPT_WEIGHT && c->weight > 0)
										{
											/* Prevent weighted single or only weighted windows */
											if(n == 1 || (i == n - 1 && weighted == n - 1))
												{
													c->flags &= ~SUB_WIN_OPT_WEIGHT;
													c->weight = 0;
													continue;
												}
											else size -= width * c->weight / 100;
											weighted++;
										}
								}
						}

					if(weighted > 0)
						{
							if(w->flags & SUB_WIN_TILE_HORZ) width = size;
							else if(w->flags & SUB_WIN_TILE_VERT) height = size;
						}

					 /* Prevent divide by zero */
					n		= (n - collapsed - weighted - full) > 0 ? n - collapsed - weighted - full: 1;
					mw	= (w->flags & SUB_WIN_TILE_HORZ) ? width / n : width;
					mh	= (w->flags & SUB_WIN_TILE_VERT) ? (height - collapsed * d->th) / n : height;

					/* Get compensation for bad rounding */
					if(w->flags & SUB_WIN_TILE_HORZ) comp = abs(width - n * mw - collapsed * d->th);
					else comp = abs(height - n * mh - collapsed * d->th);

					for(i = 0; i < n + collapsed + weighted + full; i++)
						{
							SubWin *c = subWinFind(wins[i]);
							if(c && !(c->flags & (SUB_WIN_OPT_TRANS|SUB_WIN_OPT_RAISE|SUB_WIN_OPT_FULL)))
								{
									c->parent	= w;
									c->x			= 0;
									c->y			= (c->flags & SUB_WIN_OPT_COLLAPSE) ? y : collapsed * d->th;
									c->width	= mw;
									c->height = (c->flags & SUB_WIN_OPT_COLLAPSE) ? d->th : mh;

									/* Adjust sizes according to the tile alignment */
									if(w->flags & SUB_WIN_TILE_HORZ)
										{
											if(!(c->flags & SUB_WIN_OPT_COLLAPSE)) c->x = x;
											if(c->flags & SUB_WIN_OPT_COLLAPSE) c->width = SUBWINWIDTH(w);
											else if(c->flags & SUB_WIN_OPT_WEIGHT) c->width = SUBWINWIDTH(w) * c->weight / 100;

											if(collapsed > 0) c->height = mh - collapsed * d->th;
										}
									else if(w->flags & SUB_WIN_TILE_VERT)
										{
											c->y = y;
											if(c->flags & SUB_WIN_OPT_WEIGHT) c->height = SUBWINHEIGHT(w) * c->weight / 100;
										}

									/* Add compensation to width or height */
									if(i == n + collapsed + weighted - 1) 
										if(w->flags & SUB_WIN_TILE_HORZ) c->width += comp; 
										else c->height += comp;
		
									/* Adjust steps */
									if(w->flags & SUB_WIN_TILE_HORZ) 
										{
											if(c->flags & SUB_WIN_OPT_COLLAPSE) y += d->th;
											else x += c->width;
										}
									if(w->flags & SUB_WIN_TILE_VERT) y += c->height;

									subLogDebug("Configuring %s-window: x=%d, y=%d, width=%d, height=%d, weight=%d\n", 
										(c->flags & SUB_WIN_OPT_COLLAPSE) ? "c" : ((w->flags & SUB_WIN_OPT_WEIGHT) ? "w" : 
										((w->flags & SUB_WIN_OPT_RAISE) ? "r" : "n")), c->x, c->y, c->width, c->height, c->weight);

									subWinResize(c);
									if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(c);
									else if(w->flags & SUB_WIN_TYPE_CLIENT) subClientConfigure(c);
								}
						}
					XFree(wins);
					subLogDebug("Configuring %s-tile: n=%d, mw=%d, mh=%d\n", (w->flags & SUB_WIN_TILE_HORZ) ? "h" : "v", n, mw, mh);
				}
		}
}
