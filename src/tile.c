#include "subtle.h"

 /**
	* Create a new tile.
	* @param mode Tile mode
	* @return Return either a #SubWin on success or otherwise NULL.
	**/

SubWin *
subTileNew(short mode)
{
	SubWin *w = subWinNew(0);

	w->prop	= mode ? SUB_WIN_TILEV : SUB_WIN_TILEH;
	w->tile	= (SubTile *)calloc(1, sizeof(SubTile));
	if(!w->tile) subLogError("Can't alloc memory. Exhausted?\n");

	w->tile->btnew	= XCreateSimpleWindow(d->dpy, w->frame, d->th, 0, 7 * d->fx, d->th, 0, d->colors.border, d->colors.norm);
	w->tile->btdel	= XCreateSimpleWindow(d->dpy, w->frame, d->th + 7 * d->fx, 0, 7 * d->fx, d->th, 0, d->colors.border, d->colors.norm);
	w->tile->mw			= w->width;
	w->tile->mh			= w->height;

	screen->active = w;

	subWinMap(w);
	subLogDebug("Adding %s-tile: x=%d, y=%d, width=%d, height=%d\n", (w->prop & SUB_WIN_TILEH) ? "h" : "v", w->x, w->y, w->width, w->height);

	return(w);
}

 /**
	* Add a window to a tile.
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
	* Delete a tile window and all children.
	* @param w A #SubWin
	**/

void
subTileDelete(SubWin *w)
{
	if(w && w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
		{
			unsigned int n, i;
			Window nil, *wins = NULL;

			SubWin *parent = NULL;
			XGrabServer(d->dpy);
			subWinUnmap(w);

			//if(screen->active == w) screen->active = w->parent;

			XQueryTree(d->dpy, w->win, &nil, &nil, &wins, &n);	
			for(i = 0; i < n; i++)
				{
					SubWin *c = subWinFind(wins[i]);
					if(c && c->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV)) subTileDelete(c);
					else if(c && c->prop & SUB_WIN_CLIENT) subClientSendDelete(c);
				}

			subLogDebug("Deleting %s-tile with %d children\n", (w->prop & SUB_WIN_TILEH) ? "h" : "v", n);

			/*if(w->parent) parent = w->parent; */
			free(w->tile); 
			subWinDelete(w);
			subTileConfigure(parent); 
			XFree(wins);

			XSync(d->dpy, False);
			XUngrabServer(d->dpy);
		}
}

 /**
	* Render a tile window.
	* @param mode Render mode
	* @param w A #SubWin
	**/

void
subTileRender(short mode,
	SubWin *w)
{
	if(w)
		{
			unsigned long col = mode ? (w->prop & SUB_WIN_COLLAPSE ? d->colors.cover : d->colors.norm) : d->colors.focus;

			if(w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
				{
					XSetWindowBackground(d->dpy, w->tile->btnew, col);
					XSetWindowBackground(d->dpy, w->tile->btdel, col);
					XClearWindow(d->dpy, w->tile->btnew);
					XClearWindow(d->dpy, w->tile->btdel);

					/* Descriptive buttons */
					XDrawString(d->dpy, w->tile->btnew, d->gcs.font, 3, d->fy - 1, 
						(w->prop & SUB_WIN_TILEV ? "Newrow" : "Newcol"), 6);
					XDrawString(d->dpy, w->tile->btdel, d->gcs.font, 3, d->fy - 1, 
						(w->parent && w->parent->prop & SUB_WIN_TILEV ? "Delrow" : "Delcol"), 6);
				}
		}
}

 /**
	* Configure a tile and each child.
	* @param w A #SubWin
	**/

void
subTileConfigure(SubWin *w)
{
	if(w && w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
		{
			int width = SUBWINWIDTH(w), height = SUBWINHEIGHT(w);
			unsigned int x = 0, y = 0, i = 0, n = 0, size = 0, comp = 0, collapsed = 0, weighted = 0;
			Window nil, *wins = NULL;

			XQueryTree(d->dpy, w->win, &nil, &nil, &wins, &n);
			if(n > 0)
				{
					size = (w->prop & SUB_WIN_TILEH) ? width : height;

					/* Find weighted and collapsed windows */
					for(i = 0; i < n; i++)
						{
							SubWin *c = subWinFind(wins[i]);
							if(c)
								{
									if(c->prop & SUB_WIN_COLLAPSE) collapsed++;
									else if(c->prop & SUB_WIN_WEIGHT && c->weight > 0)
										{
											if(n == 1)
												{
													c->prop &= ~SUB_WIN_WEIGHT;
													c->weight = 0;
												}
											else size -= size * c->weight / 100;
											weighted++;
										}
								}
						}

					if(weighted > 0)
						{
							if(w->prop & SUB_WIN_TILEH) width = size;
							else if(w->prop & SUB_WIN_TILEV) height = size;
						}

					n						= (n - collapsed - weighted) > 0 ? n - collapsed - weighted : 1; /* Prevent divide by zero */
					w->tile->mw = (w->prop & SUB_WIN_TILEH) ? width / n : width;
					w->tile->mh = (w->prop & SUB_WIN_TILEV) ? (height - collapsed * d->th) / n : height;

					/* Get compensation for bad rounding */
					if(w->prop & SUB_WIN_TILEH) comp = abs(width - n * w->tile->mw - collapsed * d->th);
					else comp = abs(height - n * w->tile->mh - collapsed * d->th);

					for(i = 0; i < n + collapsed + weighted; i++)
						{
							SubWin *c = subWinFind(wins[i]);
							if(c && !(c->prop & (SUB_WIN_RAISE|SUB_WIN_TRANS)))
								{
									c->parent	= w;
									c->x			= (w->prop & SUB_WIN_TILEH) ? x : 0;
									c->y			= (w->prop & SUB_WIN_TILEV) ? y : 0;
									c->width	= (c->prop & SUB_WIN_WEIGHT && w->prop & SUB_WIN_TILEH) ? SUBWINWIDTH(w) * c->weight / 100 : w->tile->mw;
									c->height = (c->prop & SUB_WIN_COLLAPSE) ? d->th : 
										((c->prop & SUB_WIN_WEIGHT && w->prop & SUB_WIN_TILEV) ? SUBWINHEIGHT(w) * c->weight / 100 : w->tile->mh);

									/* Add compensation to width or height */
									if(i == n + collapsed + weighted - 1) 
										if(w->prop & SUB_WIN_TILEH) c->width += comp;
										else c->height += comp;
		
									x += c->width;
									y += c->height;

									subLogDebug("Configuring %s-window: x=%d, y=%d, width=%d, height=%d, weight=%d\n", 
										(c->prop & SUB_WIN_COLLAPSE) ? "c" : ((w->prop & SUB_WIN_WEIGHT) ? "w" : "n"), c->x, c->y, c->width, c->height, c->weight);

									subWinResize(c);

									if(w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV)) subTileConfigure(c);
									else if(w->prop & SUB_WIN_CLIENT) subClientSendConfigure(c);
								}
						}
					XFree(wins);
					subLogDebug("Configuring %s-tile: n=%d, mw=%d, mh=%d\n", 
						(w->prop & SUB_WIN_TILEH) ? "h" : "v", n, w->tile->mw, w->tile->mh);
				}
		}
}
