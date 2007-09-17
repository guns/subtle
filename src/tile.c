
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
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
	SubWin *w = subWinNew();

	w->flags	= SUB_WIN_TYPE_TILE;
	w->flags |= mode;
	w->tile		= (SubTile *)subUtilAlloc(1, sizeof(SubTile));

	subWinMap(w);

	subUtilLogDebug("Adding %s-tile: x=%d, y=%d, width=%d, height=%d\n", 
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
			SubWin *c = w->tile->first;
			while(c)
				{
					subWinDelete(c);
					c = c->next;
				}

			subUtilLogDebug("Deleting %s-tile\n", (w->flags & SUB_WIN_TILE_HORZ) ? "h" : "v");

			free(w->tile); 
		}
}

 /**
	* Add a window to a tile 
	* @param p A #SubWin
	* @param w A #SubWin
	**/

void
subTileAdd(SubWin *p,
	SubWin *w)
{
	if(p && w)
		{
			subWinAppend(p, w);

			XReparentWindow(d->dpy, w->frame, p->frame, 0, 0);

			subTileConfigure(p);
			subUtilLogDebug("Adding window: x=%d, y=%d, width=%d, height=%d\n", w->x, w->y, w->width, w->height);
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
			SubWin *c = w->tile->first;
			int mw = 0, mh = 0, width = w->width, height = w->height;
			unsigned int x = 0, y = 0, n = 0, size = 0, comp = 0, collapsed = 0, weighted = 0, full = 0, raised = 0;

			if(c)
				{
					size = (w->flags & SUB_WIN_TILE_HORZ) ? width : height;

					/* Find special windows */
					while(c)
						{
							if(c->flags & SUB_WIN_STATE_COLLAPSE) collapsed++;
							else if(c->flags & SUB_WIN_STATE_FULL) full++;
							else if(c->flags & SUB_WIN_STATE_RAISE) raised++;
							else if(c->flags & SUB_WIN_STATE_WEIGHT && c->weight > 0)
								{
									/* Prevent only weighted windows */
									if(!c->next && weighted == n)
										{
											c->flags &= ~SUB_WIN_STATE_WEIGHT;
											c->weight = 0;
										}
									else 
										{
											size -= width * c->weight / 100;
											weighted++;
										}
								}
							c = c->next;
							n++;
						}

					/* Remove unneeded tiles */
					if(n == 1 && !(w->flags & SUB_WIN_TYPE_SCREEN))
						{
							SubWin *swap = w->tile->first;
							subWinReplace(w, swap);
							XReparentWindow(d->dpy, swap->frame, swap->parent->frame, 0, 0);
							subWinCut(w);
							w->tile->first = NULL;
							subWinDelete(w);
							printf("Removing dynamic window..\n");
							return;
						}

					/* Piled window */
					if(w->flags & SUB_WIN_STATE_PILE) 
						{
							collapsed = n - 1;
							weighted	= 0;
						}
					if(weighted > 0)
						{
							if(w->flags & SUB_WIN_TILE_HORZ) width = size;
							else if(w->flags & SUB_WIN_TILE_VERT) height = size;
						}

					 /* Prevent divide by zero */
					n		= (n - collapsed - weighted - full - raised) > 0 ? n - collapsed - weighted - full - raised : 1;
					mw	= (w->flags & SUB_WIN_TILE_HORZ) ? width / n : width;
					mh	= (w->flags & SUB_WIN_TILE_VERT) ? (height - collapsed * d->th) / n : height;

					/* Get compensation for bad rounding */
					if(w->flags & SUB_WIN_TILE_HORZ) comp = abs(width - n * mw - collapsed * d->th);
					else comp = abs(height - n * mh - collapsed * d->th);

					c = w->tile->first;
					while(c)
						{
							if(!(c->flags & (SUB_WIN_STATE_TRANS|SUB_WIN_STATE_RAISE|SUB_WIN_STATE_FULL)))
								{
									if(w->flags & SUB_WIN_STATE_PILE && w->tile->pile != c) c->flags |= SUB_WIN_STATE_COLLAPSE;

									c->parent	= w;
									c->x			= w->flags & SUB_WIN_TYPE_SCREEN ? d->th : 0;
									c->y			= (c->flags & SUB_WIN_STATE_COLLAPSE) ? y : collapsed * d->th;
									c->width	= mw;
									c->height	= (c->flags & SUB_WIN_STATE_COLLAPSE) ? d->th : mh;

									/* Adjust sizes according to the tile alignment */
									if(w->flags & SUB_WIN_TILE_HORZ)
										{
											if(!(c->flags & SUB_WIN_STATE_COLLAPSE)) c->x = x;
											if(c->flags & SUB_WIN_STATE_COLLAPSE) c->width = SUBWINWIDTH(w);
											else if(c->flags & SUB_WIN_STATE_WEIGHT) c->width = SUBWINWIDTH(w) * c->weight / 100;

											if(collapsed > 0) c->height = mh - collapsed * d->th;
										}
									else if(w->flags & SUB_WIN_TILE_VERT)
										{
											c->y = y;
											if(c->flags & SUB_WIN_STATE_WEIGHT) c->height = SUBWINHEIGHT(w) * c->weight / 100;
										}

									/* Add compensation to width or height */
									if(w->tile->last == c) 
										if(w->flags & SUB_WIN_TILE_HORZ) c->width += comp; 
										else c->height += comp;
		
									/* Adjust steps */
									if(w->flags & SUB_WIN_TILE_HORZ) 
										{
											if(c->flags & SUB_WIN_STATE_COLLAPSE) y += d->th;
											else x += c->width;
										}
									if(w->flags & SUB_WIN_TILE_VERT) y += c->height;

									subUtilLogDebug("Configuring %s-window: x=%d, y=%d, width=%d, height=%d, weight=%d\n", 
										(c->flags & SUB_WIN_STATE_COLLAPSE) ? "c" : ((w->flags & SUB_WIN_STATE_WEIGHT) ? "w" : 
										((w->flags & SUB_WIN_STATE_RAISE) ? "r" : "n")), c->x, c->y, c->width, c->height, c->weight);

									subWinResize(c);
									if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(c);
									else if(w->flags & SUB_WIN_TYPE_CLIENT) subClientConfigure(c);
								}
							c = c->next;
						}
					subUtilLogDebug("Configuring %s-tile: n=%d, mw=%d, mh=%d\n", (w->flags & SUB_WIN_TILE_HORZ) ? "h" : "v", n, mw, mh);
				}
		}
}
