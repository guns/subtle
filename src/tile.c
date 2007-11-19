
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

 /**
	* Create new tile 
	* @param mode Tile mode
	* @return Return either a #SubWin on success or otherwise NULL.
	**/

SubWin *
subTileNew(short mode)
{
	SubWin *t = subWinNew();

	t->flags	= SUB_WIN_TYPE_TILE|mode;
	t->tile		= (SubTile *)subUtilAlloc(1, sizeof(SubTile));

	XMapSubwindows(d->disp, t->frame);
	XSaveContext(d->disp, t->frame, d->cv->xid, (void *)t);

	subUtilLogDebug("Adding %s-tile: x=%d, y=%d, width=%d, height=%d\n", 
		(t->flags & SUB_WIN_TILE_HORZ) ? "h" : "v", t->x, t->y, t->width, t->height);

	return(t);
}

 /**
	* Delete a tile window and all children 
	* @param t A #SubWin
	**/

void
subTileDelete(SubWin *t)
{
	SubWin *c = NULL;

	assert(t && t->flags & SUB_WIN_TYPE_TILE);

	c = t->tile->first;

	while(c)
		{
			SubWin *next = c->next;
			subWinDelete(c);
			c = next;
		}

	subUtilLogDebug("Deleting %s-tile\n", (t->flags & SUB_WIN_TILE_HORZ) ? "h" : "v");
	subTileDestroy(t);
}

 /**
	* Destroy tile
	* @param t A #SubWin
	**/

void
subTileDestroy(SubWin *t)
{
	assert(t && t->flags & SUB_WIN_TYPE_TILE);

	free(t->tile);
}
	
 /**
	* Add window to tile 
	* @param t A #SubWin
	* @param w A #SubWin
	**/

void
subTileAdd(SubWin *t,
	SubWin *w)
{
	assert(t && t->flags & SUB_WIN_TYPE_TILE && w);

	if(!t->tile->first)
		{
			t->tile->first = w;
			t->tile->last = w;
			w->next = NULL;
			w->prev = NULL;
			w->parent = t;

			XReparentWindow(d->disp, w->frame, t->frame, 0, t->flags & SUB_WIN_TYPE_VIEW ? d->th : 0); 
		}
	else subWinAppend(t->tile->last, w);

	subUtilLogDebug("Adding window: x=%d, y=%d, width=%d, height=%d\n", w->x, w->y, w->width, w->height);
}

 /**
	* Configure a tile and each child 
	* @param t A #SubWin
	**/

void
subTileConfigure(SubWin *t)
{
	SubWin *c = NULL;
	int mw = 0, mh = 0, width = 0, height = 0;
	unsigned int x = 0, y = 0, n = 0, size = 0, comp = 0, shaded = 0, resized = 0, full = 0, floated = 0;

	assert(t && t->flags & SUB_WIN_TYPE_TILE);

	width		= t->width;
	height	= t->height;
	c				= t->tile->first;

	if(c)
		{
			size = (t->flags & SUB_WIN_TILE_HORZ) ? width : height;

			/* Find special windows */
			while(c)
				{
					if(c->flags & SUB_WIN_STATE_SHADE) shaded++;
					else if(c->flags & SUB_WIN_STATE_FULL) full++;
					else if(c->flags & SUB_WIN_STATE_FLOAT) floated++;
					else if(c->flags & SUB_WIN_STATE_RESIZE && c->resized > 0)
						{
							/* Prevent only resized windows */
							if(!c->next && resized == n)
								{
									c->flags &= ~SUB_WIN_STATE_RESIZE;
									c->resized = 0;
								}
							else 
								{
									size -= width * c->resized / 100;
									resized++;
								}
						}
					c = c->next;
					n++;
				}

			/* Stacked window */
			if(t->flags & SUB_WIN_STATE_STACK) 
				{
					shaded = n - 1;
					resized	= 0;
				}

			/* Weighted window */
			if(resized > 0)
				{
					if(t->flags & SUB_WIN_TILE_HORZ) width = size;
					else if(t->flags & SUB_WIN_TILE_VERT) height = size;
				}

			 /* Prevent divide by zero */
			n		= (n - shaded - resized - full - floated) > 0 ? n - shaded - resized - full - floated : 1;
			mw	= (t->flags & SUB_WIN_TILE_HORZ) ? width / n : width;
			mh	= (t->flags & SUB_WIN_TILE_VERT) ? (height - shaded * d->th) / n : height;

			/* Get compensation for bad rounding */
			if(t->flags & SUB_WIN_TILE_HORZ) comp = abs(width - n * mw - shaded * d->th);
			else comp = abs(height - n * mh - shaded * d->th);

			c = t->tile->first;
			while(c)
				{
					if(!(c->flags & (SUB_WIN_STATE_TRANS|SUB_WIN_STATE_FLOAT|SUB_WIN_STATE_FULL)))
						{
							if(t->flags & SUB_WIN_STATE_STACK && t->tile->pile != c) c->flags |= SUB_WIN_STATE_SHADE;

							/* Remove tiles with only one or less clients */
							if(c->flags & SUB_WIN_TYPE_TILE && !(c->flags & SUB_WIN_TYPE_VIEW) && 
								c->tile->first == c->tile->last)
								{
									SubWin *first = c->tile->first;

									if(t->tile->first == c) t->tile->first	= first;
									if(t->tile->last == c) t->tile->last		= first;

									first->prev		= c->prev;
									first->next		= c->next;
									first->parent	= c->parent;

									if(first->prev) first->prev->next = first;
									if(first->next) first->next->prev = first;

									c->prev		= NULL;
									c->next		= NULL;
									c->parent = NULL;
									c->tile->first = NULL;

									XReparentWindow(d->disp, first->frame, first->parent->frame, 0, 0);

									subTileDelete(c);
									//subWinDestroy(c);

									XDeleteContext(d->disp, c->frame, 1);
									XDestroySubwindows(d->disp, c->frame);
									XDestroyWindow(d->disp, c->frame);

									printf("Removing dynamic tile %#lx\n", c->frame);

									c = first;
								}

							c->parent	= t;
							c->x			= t->flags & SUB_WIN_TYPE_VIEW ? d->th : 0;
							c->y			= (c->flags & SUB_WIN_STATE_SHADE) ? y : shaded * d->th;
							c->width	= mw;
							c->height	= (c->flags & SUB_WIN_STATE_SHADE) ? d->th : mh;

							/* Adjust sizes according to the tile alignment */
							if(t->flags & SUB_WIN_TILE_HORZ)
								{
									if(!(c->flags & SUB_WIN_STATE_SHADE)) c->x = x;
									if(c->flags & SUB_WIN_STATE_SHADE) c->width = SUBWINWIDTH(t);
									else if(c->flags & SUB_WIN_STATE_RESIZE) c->width = SUBWINWIDTH(t) * c->resized / 100;

									if(shaded > 0) c->height = mh - shaded * d->th;
								}
							else if(t->flags & SUB_WIN_TILE_VERT)
								{
									c->y = y;
									if(c->flags & SUB_WIN_STATE_RESIZE) c->height = SUBWINHEIGHT(t) * c->resized / 100;
								}

							/* Add compensation to width or height */
							if(t->tile->last == c) 
								{
									if(t->flags & SUB_WIN_TILE_HORZ) c->width += comp; 
									else c->height += comp;
								}

							/* Adjust size */
							if(t->flags & SUB_WIN_TILE_HORZ) 
								{
									if(c->flags & SUB_WIN_STATE_SHADE) y += d->th;
									else x += c->width;
								}
							if(t->flags & SUB_WIN_TILE_VERT) y += c->height;

							subUtilLogDebug("Configuring %s-window: x=%d, y=%d, width=%d, height=%d, resized=%d\n", 
								(c->flags & SUB_WIN_STATE_SHADE) ? "c" : ((t->flags & SUB_WIN_STATE_RESIZE) ? "t" : 
								((t->flags & SUB_WIN_STATE_FLOAT) ? "r" : "n")), c->x, c->y, c->width, c->height, c->resized);

							subWinResize(c);

							if(c->flags & SUB_WIN_TYPE_TILE) subTileConfigure(c);
							else if(c->flags & SUB_WIN_TYPE_CLIENT) subClientConfigure(c);
						}
					c = c->next;
				}

			subUtilLogDebug("Configuring %s-tile: n=%d, mw=%d, mh=%d\n", (t->flags & SUB_WIN_TILE_HORZ) ? "h" : "v", n, mw, mh);
		}
}
