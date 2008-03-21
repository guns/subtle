
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Id$
	**/

#include "subtle.h"

/* ViewNew {{{ */
static void
ViewNew(SubView *v)
{
	assert(v);

	v->tile = subTileNew(SUB_TYPE_HORZ);
	v->frame	= XCreateSimpleWindow(d->disp, DefaultRootWindow(d->disp), 0, 0, 1, d->th, 0, 
		d->colors.border, d->colors.norm);
	v->button	= XCreateSimpleWindow(d->disp, d->bar.views, 0, 0, 1, d->th, 0, d->colors.border, d->colors.norm);

	XSaveContext(d->disp, v->button, 1, (void *)v);

	XMapWindow(d->disp, v->frame);
	XMapWindow(d->disp, v->button);
} /* }}} */

/* ViewDelete {{{ */
static void
ViewDelete(SubView *v)
{
	assert(v && v->tile);

	XDeleteContext(d->disp, v->button, 1);

	XDestroyWindow(d->disp, v->frame);
	XDestroyWindow(d->disp, v->button);

	subTileKill(v->tile);
	v->tile = NULL;
} /* }}} */

 /** subViewNew {{{
	* Create a view
	* @param[in] name Name of the view
	* @return Success: #SubClient
	* 				Error: Exit
	**/

SubView *
subViewNew(char *name)
{
	SubView *v = NULL;

	assert(name);
	
	v	= (SubView *)subUtilAlloc(1, sizeof(SubView));
	v->flags	|= SUB_TYPE_VIEW;
	v->name		= strdup(name);
	v->width	=	strlen(v->name) * d->fx + 8; /* Font offsets */

	subArrayPush(d->views, (void *)v);

	printf("Adding view (%s)\n", v->name);
	return(v);
} /* }}} */

 /** subViewDelete {{{
	* Delete view
	* @param[in] w A #SubClient
	**/

void
subViewDelete(SubView *v)
{
	assert(v);

	XUnmapWindow(d->disp, v->frame);
	XUnmapWindow(d->disp, v->button);

	subViewConfigure();

	printf("Removing view (%s)\n", v->name);
} /* }}} */

 /** subViewMerge {{{
	* Merge window
	* @param[in] win A #Window
	**/

void
subViewMerge(Window win)
{
	int i, j, merged = 0;
	long vid = 0;
	char *class = NULL;
	SubView *v = NULL;
	SubRule *r = NULL;

	assert(d->views && win);

	class = subEwmhGetProperty(win, XA_STRING, SUB_EWMH_WM_CLASS, NULL);

	/* Loop through each view and rule */
	for(i = d->views->ndata; i > 0; i--)
		{
			for(j = 0; j < ((SubView *)d->views->data[i])->rules->ndata; j++)
				{
					if((0 == i && !merged) || (r->regex && !regexec(r->regex, class, 0, NULL, 0)))
						{
							SubClient *c = subClientNew(win);

							if(c->flags & SUB_STATE_TRANS) 
								{
									c->tile = d->cv->tile;
									subClientToggle(c, SUB_STATE_FLOAT);
								}

							/* EWMH: Desktop */
							vid = subArrayFind(d->views, (void *)d->cv);
							subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

							if(!v->tile) ViewNew(v);
							if(!r->tile)
								{
									r->tile = subTileNew(SUB_TYPE_HORZ);
									r->tile->flags |= SUB_STATE_RESIZE;

									subTilePush(v->tile, r->tile);
								}

							subTilePush(r->tile, (void *)c);
							merged++;

							/* Map only if the desired view is the current view */
							if(d->cv == v) subClientMap(c);
							else XMapSubwindows(d->disp, c->frame);

							printf("Adding client %s (%s)\n", c->name, v->name);
						}
				}
		}
	
	subViewConfigure();
	subViewRender();
	subTileConfigure(d->cv->tile);

	XFree(class);
} /* }}} */

 /** subViewRender {{{ 
	* Render views
	* @param[in] v A #SubView
	**/

void
subViewRender(void)
{
	int i;
	SubView *v = NULL;
	
	assert(d->views && d->cv);

	XClearWindow(d->disp, d->bar.win);
	XFillRectangle(d->disp, d->bar.win, d->gcs.border, 0, 2, DisplayWidth(d->disp, DefaultScreen(d->disp)), d->th - 4);	

	for(i = 0; i < d->clients->ndata; i++)
		{
			if(v->tile)
				{
					XSetWindowBackground(d->disp, v->button, (d->cv == v) ? d->colors.focus : d->colors.norm);
					XClearWindow(d->disp, v->button);
					XDrawString(d->disp, v->button, d->gcs.font, 3, d->fy - 1, v->name, strlen(v->name));
				}
		}
} /* }}} */

 /** subViewConfigure {{{
	* Configure views
	**/

void
subViewConfigure(void)
{
	int i = 0, width = 0;
	char **names = NULL;
	Window *wins = NULL;

	assert(d->cv);

	wins = (Window *)subUtilAlloc(d->clients->ndata, sizeof(Window));
	names = (char **)subUtilAlloc(d->clients->ndata, sizeof(char *));

	for(i = 0; i < d->clients->ndata; i++)
		{
			if(((SubView *)d->clients->data[i])->tile)
				{
					XMoveResizeWindow(d->disp, ((SubView *)d->clients->data[i])->button, width, 0, 
						((SubView *)d->clients->data[i])->width, d->th);
					width			+= ((SubView *)d->clients->data[i])->width;
					wins[i]		= ((SubView *)d->clients->data[i])->frame;
					names[i]	= ((SubView *)d->clients->data[i])->name;
				}
		}
	XMoveResizeWindow(d->disp, d->bar.views, 0, 0, width, d->th);

	/* EWMH: Virtual roots */
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, d->clients->ndata);

	/* EWMH: Desktops */
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&d->clients->ndata, 1);
	subEwmhSetStrings(DefaultRootWindow(d->disp), SUB_EWMH_NET_DESKTOP_NAMES, names, d->clients->ndata);

	free(wins);
	free(names);
} /* }}} */

 /** subViewKill {{{
	* Kill view 
	* @param[in] v A #SubView
	**/

void
subViewKill(SubView *v)
{
	assert(v);

	printf("Removing view (%s)\n", v->name);

	XUnmapWindow(d->disp, v->button);
	XDeleteContext(d->disp, v->button, 1);
	XDestroyWindow(d->disp, v->button);

	if(v->tile)
		{
			subTileKill(v->tile);
			XDeleteContext(d->disp, v->frame, 1);
			XDestroyWindow(d->disp, v->frame);
		}
	
	subArrayKill(v->rules);

	free(v->name);
	free(v);					
} /* }}} */

 /** subViewSwitch {{{
	* Switch view
	* @param[in] v A #SubView
	**/

void
subViewSwitch(SubView *v)
{
	long vid = 0;

	assert(v);

	if(d->cv == v) return; /* Just skip */

	XUnmapWindow(d->disp, d->cv->frame);
	if(d->cv != d->views->data[0] && d->cv->tile->clients->ndata == 0) ViewDelete(d->cv);
	d->cv = v;

	if(!v->frame) ViewNew(v);

	XMapRaised(d->disp, d->bar.win);
	XMapWindow(d->disp, d->cv->frame);

	/* EWMH: Desktops */
	vid = subArrayFind(d->views, v) + 1;
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

	subViewConfigure();
	subViewRender();

	printf("Switching view (%s)\n", d->cv->name);
} /* }}} */
