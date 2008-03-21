
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Id$
	**/

#include "subtle.h"

 /** subTileNew {{{
	* Create new tile 
	* @param[in] mode Tile mode
	* @return Success: #SubTile
	* 				Error: NULL
	**/

SubTile *
subTileNew(int mode)
{
	SubTile *t = NULL;

	assert(mode == SUB_TYPE_HORZ || mode == SUB_TYPE_VERT);

	t	= (SubTile *)subUtilAlloc(1, sizeof(SubTile));
	t->flags = SUB_TYPE_TILE|mode;

	subUtilLogDebug("type=%s\n", (mode & SUB_TYPE_HORZ) ? "horz" : "vert");

	return(t);
} /* }}} */

 /** subTilePush {{{
	* Push clients to tile
	* @param[in] t A #SubTile
	* @param[in] c A clients
	**/

void
subTilePush(SubTile *t,
	void *c)
{
	assert(t && c);

	subArrayPush(t->clients, (void *)c);
	((SubClient *)c)->tile = t;
} /* }}} */

 /** subTilePop {{{
	* Pop clients from tile
	* @param[in] t A #SubTile
	* @param[in] c A clients
	**/

void
subTilePop(SubTile *t,
	void *c)
{
	assert(t && c);

	subArrayPop(t->clients, c);
	((SubClient *)c)->tile = NULL;
} /* }}} */

 /** subTileConfigure {{{
	* Configure tile and clients 
	* @param[in] t A #SubTile
	**/

void
subTileConfigure(SubTile *t)
{
	int i;
	SubClient *c = NULL;
	unsigned int x = 0, y = 0, ch = 0, cw = 0, width = 0, height = 0, size = 0, 
		comp = 0, shaded = 0, resized = 0, full = 0, floated = 0, nclients = 0;

	assert(t);

	width			= t->width;
	height		= t->height;
	nclients	= t->clients->ndata;

	if(nclients > 0)
		{
			size = (t->flags & SUB_TYPE_HORZ) ? width : height;

			/* Find special clients */
			for(i = 0; i < t->clients->ndata; i++)
				{
					c = (SubClient *)t->clients->data[i]; /* Both types have common fields */

					if(c->flags & SUB_STATE_SHADE) shaded++;
					else if(c->flags & SUB_STATE_FULL) full++;
					else if(c->flags & SUB_STATE_FLOAT) floated++;
					else if(c->flags & SUB_STATE_RESIZE && c->size > 0)
						{
							/* Prevent resized windows only */
							if(resized == nclients)
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

			/* Stacked window */
			if(t->flags & SUB_STATE_STACK) 
				{
					shaded = nclients - 1;
					resized	= 0;
				}

			/* Weighted window */
			if(resized > 0)
				{
					if(t->flags & SUB_TYPE_HORZ) width = size;
					else if(t->flags & SUB_TYPE_VERT) height = size;
				}

			 /* Prevent division by zero */
			nclients	= (nclients - shaded - resized - full - floated) > 0 ? nclients - shaded - resized - full - floated : 1;
			cw				= (t->flags & SUB_TYPE_HORZ) ? width / nclients : width;
			ch				= (t->flags & SUB_TYPE_VERT) ? (height - shaded * d->th) / nclients : height;

			/* Get compensation for int rounding */
			if(t->flags & SUB_TYPE_HORZ) comp = abs(width - nclients * cw - shaded * d->th);
			else comp = abs(height - nclients * ch - shaded * d->th);

			for(i = 0; i < t->clients->ndata; i++)
				{
					c = (SubClient *)t->clients->data[i]; /* Both types have common fields */
					
					if(!(c->flags & (SUB_STATE_TRANS|SUB_STATE_FLOAT|SUB_STATE_FULL)))
						{
							/* Shade every client that is not on top */
							if(t->flags & SUB_STATE_STACK && t->tile->top != c) 
								c->flags |= SUB_STATE_SHADE;

							/* Remove tiles with only one client */
							if(c->flags & SUB_TYPE_TILE && ((SubTile *)c)->clients->ndata == 1)
								{
									SubTile *tmp = (SubTile *)t->clients->data[i];
									t->clients->data[i] = tmp->clients->data[0];

									subUtilLogDebug("Removing dynamic tile %#lx\n", tmp);

									subTileKill(tmp);
								}

							c->tile		= t;
							c->x			= 0;
							c->y			= (c->flags & SUB_STATE_SHADE) ? y : shaded * d->th;
							c->width	= (c->flags & SUB_STATE_SHADE) ? t->width : cw;
							c->height	= (c->flags & SUB_STATE_SHADE) ? d->th : ch;

							/* Adjust sizes according to the tile alignment */
							if(t->flags & SUB_TYPE_HORZ)
								{
									if(!(c->flags & SUB_STATE_SHADE)) 
										{
											c->x = x;
											x += c->width; /* Adjust x */
										}
									if(c->flags & SUB_STATE_SHADE) 
										{
											c->width = SUBWINWIDTH(t);
											y += d->th; /* Adjust y */
										}
									else if(c->flags & SUB_STATE_RESIZE) c->width = t->width * c->size / 100;

									if(shaded > 0) c->height = ch - shaded * d->th;
								}
							else if(t->flags & SUB_TYPE_VERT)
								{
									c->y = y;
									y += c->height; /* Adjust y again */
									if(c->flags & SUB_STATE_RESIZE) c->height = t->height * c->size / 100;
								}

							/* Add compensation to width or height */
							if(t->clients->ndata == i) 
								{
									if(t->flags & SUB_TYPE_HORZ) c->width += comp; 
									else c->height += comp;
								}

							if(c->flags & SUB_TYPE_TILE) subTileConfigure((SubTile *)t->clients->data[i]);
							else if(c->flags & SUB_TYPE_CLIENT) 
								{
									subClientResize(c);
									subClientConfigure(c);
								}
						}
				}
			subUtilLogDebug("type=%s, nclients=%d, cw=%d, ch=%d\n", 
				(t->flags & SUB_TYPE_HORZ) ? "horz" : "vert", nclients, cw, ch);
		}
} /* }}} */

 /** subTileKill {{{
	* @param[in] t A #SubTile
	**/

void
subTileKill(SubTile *t)
{
	int i;

	for(i = 0; i < t->clients->ndata; i++)
		{
			if(((SubClient *)t->clients->data[i])->flags & SUB_TYPE_CLIENT) subClientKill((SubClient *)t->clients->data[i]);
			else if(((SubClient *)t->clients->data[i])->flags & SUB_TYPE_TILE) subTileKill((SubTile *)t->clients->data[i]);
		}
	
	subArrayKill(t->clients);
	free(t);
} /* }}} */
