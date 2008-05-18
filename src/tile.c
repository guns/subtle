
 /**
	* @package subtle
	*
	* @file Tile functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

 /** subTileNew {{{
	* @brief Create new tile 
	* @param[in] mode			Tile mode
	* @param[in] superior	Tile superior
	* @return Returns a #SubTile or \p NULL
	**/

SubTile *
subTileNew(int mode,
	void *superior)
{
	SubTile *t = NULL;

	assert(mode &= (SUB_TYPE_HORZ|SUB_TYPE_VERT));

	t	= (SubTile *)subUtilAlloc(1, sizeof(SubTile));
	t->flags		= SUB_TYPE_TILE|mode;
	t->clients	= subArrayNew();
	t->superior	= superior;

	/* Start values */
	t->width	= DisplayWidth(d->disp, DefaultScreen(d->disp));
	t->height	= DisplayHeight(d->disp, DefaultScreen(d->disp)) - d->th;

	subUtilLogDebug("new=tile, type=%s\n", (mode & SUB_TYPE_HORZ) ? "horz" : "vert");

	return(t);
} /* }}} */

 /** subTileConfigure {{{
	* @brief Configure tile and clients 
	* @param[in] t	A #SubTile
	**/

void
subTileConfigure(SubTile *t)
{
	int i;
	SubClient *c = NULL;
	unsigned int x = 0, y = 0, ch = 0, cw = 0, width = 0, height = 0, size = 0, 
		comp = 0, shaded = 0, resized = 0, full = 0, floated = 0, n = 0;

	assert(t);

	if(0 < t->clients->ndata)
		{
			n			= t->clients->ndata;
			width	= t->width;
			height= t->height;
			size	= (t->flags & SUB_TYPE_HORZ) ? width : height;

			/* Find special clients */
			for(i = 0; i < t->clients->ndata; i++)
				{
					c = CLIENT(t->clients->data[i]); /* Both types have common fields */

					if(c->flags & SUB_STATE_SHADE) shaded++;
					else if(c->flags & SUB_STATE_FULL) full++;
					else if(c->flags & SUB_STATE_FLOAT) floated++;
					else if(c->flags & SUB_STATE_RESIZE && 0 < c->size)
						{
							/* Prevent resized windows only */
							if(resized == n)
								{
									c->flags &= ~SUB_STATE_RESIZE;
									c->size = 0;
								}
							else 
								{
									size -= width * c->size / 100;
									resized++;
								}
						}
				}

			subUtilLogDebug("type=%s, n=%d, sha=%d, ful=%d, flo=%d, res=%d\n", 
				(t->flags & SUB_TYPE_HORZ) ? "horz" : "vert", n, shaded, full, floated, resized);

			/* Stacked window */
			if(t->flags & SUB_STATE_STACK) 
				{
					shaded	= n - 1;
					resized	= 0;
				}

			/* Weighted window */
			if(0 < resized)
				{
					if(t->flags & SUB_TYPE_HORZ) width = size;
					else if(t->flags & SUB_TYPE_VERT) height = size;
				}

			 /* Prevent division by zero */
			n		= (n - shaded - resized - full - floated) > 0 ? n - shaded - resized - full - floated : 1;
			cw	= (t->flags & SUB_TYPE_HORZ) ? width / n : width;
			ch	= (t->flags & SUB_TYPE_VERT) ? (height - shaded * d->th) / n : height;

			/* Get compensation for int rounding */
			if(t->flags & SUB_TYPE_HORZ) comp = abs(width - n * cw - shaded * d->th);
			else comp = abs(height - n * ch - shaded * d->th);

			for(i = 0; i < t->clients->ndata; i++)
				{
					c = CLIENT(t->clients->data[i]); /* Both types have common fields */

					if(!(c->flags & (SUB_STATE_TRANS|SUB_STATE_FLOAT|SUB_STATE_FULL)))
						{
							/* Shade every client that is not on top */
							if(t->flags & SUB_STATE_STACK && t->tile->top != c) c->flags |= SUB_STATE_SHADE;

							/* Sanitize tree */
							if(c->flags & SUB_TYPE_TILE)
								{
									SubTile *tmp = TILE(t->clients->data[i]);
									
									if(tmp->flags & SUB_TYPE_RULE)
										{
											if(0 >= tmp->clients->ndata)
												{
													SubRule *r = RULE(tmp->superior);

													printf("Removing rule tile %p\n", t->clients->data[i]);

													subArrayPop(t->clients, t->clients->data[i]);
													subTileKill(tmp, False);
													subTileConfigure(t);
													r->tile = NULL;
													return;
												}
										}
									else if(1 == tmp->clients->ndata)
										{
											printf("Removing dynamic tile %p\n", tmp);
											t->clients->data[i] = tmp->clients->data[0];
											c = CLIENT(tmp->clients->data[0]);
											c->tile = t;

											subTileKill(tmp, False);
										}
								}

							c->tile		= t;
							c->x			= 0;
							c->y			= (c->flags & SUB_STATE_SHADE) ? y : shaded * d->th;
							c->width	= (c->flags & SUB_STATE_SHADE) ? t->width : cw;
							c->height	= (c->flags & SUB_STATE_SHADE) ? d->th : ch;

							/* Adjust sizes according to the tile alignment */
							if(t->flags & SUB_TYPE_HORZ)
								{
									if(0 < shaded) c->height = ch - shaded * d->th;
									if(!(c->flags & SUB_STATE_SHADE)) 
										{
											c->x = x;
											if(c->flags & SUB_STATE_RESIZE) c->width = t->width * c->size / 100;
										}
									else c->width = t->width;
								}
							else if(t->flags & SUB_TYPE_VERT)
								{
									c->y = y;
									if(c->flags & SUB_STATE_RESIZE) c->height = t->height * c->size / 100;
								}

							c->x += t->x;
							c->y += t->y;

							/* Add compensation to width or height */
							if(t->clients->ndata == i + 1) 
								{
									if(t->flags & SUB_TYPE_HORZ) c->width += comp; 
									else c->height += comp;
								}

							/* Store size */
							if(t->flags & SUB_TYPE_HORZ) 
								{
									if(c->flags & SUB_STATE_SHADE) y += d->th;
									else x += c->width;
								}
							else y += c->height;

							if(c->flags & SUB_TYPE_TILE) subTileConfigure((SubTile *)t->clients->data[i]);
							else if(c->flags & SUB_TYPE_CLIENT) subClientConfigure(c);
						}
				}
		}
} /* }}} */
#if 0
void
subTileSanitize(SubTile *t)
{
	assert(t);

	if(t->flags & SUB_TYPE_RULE)
		{
			if(t->clients->ndata <= 0)
				{
					SubRule *r = RULE(t->sup);
					SubTile *rt = r->tile;

					printf("Removing rule tile %p\n", t);

					subArrayPop(rt->clients, t);
					subTileKill(t, False);
					subTileConfigure(rt);
					r->tile = NULL;
					return;
				}
		}
	else if(t->clients->ndata == 1)
		{
			printf("Removing dynamic tile %p\n", t);
			t->clients->data[i] = t->clients->data[0];
			c = CLIENT(t->clients->data[0]);
			c->tile = t;

			subTileKill(tmp, False);
		}
}
#endif

 /** subTileRemap {{{ 
	* @brief Remap clients on multi views
	* @param[in] t	A #SubTile
	**/

void
subTileRemap(SubTile *t)
{
	int i;

	assert(t);

	for(i = 0; i < t->clients->ndata; i++)
		{
			SubClient *c = CLIENT(t->clients->data[i]);

			if(c->flags & SUB_TYPE_TILE) subTileRemap(TILE(t->clients->data[i]));
			else if(c->flags & SUB_STATE_MULTI)
				{
					XReparentWindow(d->disp, c->win, c->frame, d->bw, d->th);
					XMapWindow(d->disp, c->win);
					subClientConfigure(c);
				}
		}
} /* }}} */

 /** subTileKill {{{
	* @brief Kill tile
	* @param[in] t			A #SubTile
	* @param[in] clean	Free elements or not
	**/

void
subTileKill(SubTile *t,
	int clean)
{
	if(clean) subArrayKill(t->clients, clean);
	free(t);

	subUtilLogDebug("kill=tile\n");
} /* }}} */
