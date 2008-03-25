
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
	v->frame	= XCreateSimpleWindow(d->disp, DefaultRootWindow(d->disp), 0, d->th, 
		DisplayWidth(d->disp, DefaultScreen(d->disp)), DisplayHeight(d->disp, DefaultScreen(d->disp)), 0, 
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

	printf("Adding view (%s)\n", v->name);
	subUtilLogDebug("new=view, name=%s\n", name);

	return(v);
} /* }}} */

 /** subViewConfigure {{{
	* Configure views
	**/

void
subViewConfigure(void)
{
	if(d->views->ndata > 0)
		{
			int i = 0, width = 0, nv = 0;
			char **names = NULL;
			Window *wins = NULL;

			assert(d->views->ndata > 0);

			wins	= (Window *)subUtilAlloc(d->views->ndata, sizeof(Window));
			names = (char **)subUtilAlloc(d->views->ndata, sizeof(char *));

			for(i = 0; i < d->views->ndata; i++)
				{
					SubView *v = (SubView *)d->views->data[i];

					if(v->tile)
						{
							XMoveResizeWindow(d->disp, v->button, width, 0, v->width, d->th);
							width			+= v->width;
							wins[nv]		= v->frame;
							names[nv++]	= v->name;
						}
				}
			XMoveResizeWindow(d->disp, d->bar.views, 0, 0, width, d->th);

			/* EWMH: Virtual roots */
			subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, nv);

			/* EWMH: Desktops */
			subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&nv, 1);
			subEwmhSetStrings(DefaultRootWindow(d->disp), SUB_EWMH_NET_DESKTOP_NAMES, names, nv);

			free(wins);
			free(names);
	}
} /* }}} */

 /** subViewRender {{{ 
	* Render views
	* @param[in] v A #SubView
	**/

void
subViewRender(void)
{
	if(d->views->ndata > 0)
		{
			int i;
			XClearWindow(d->disp, d->bar.win);
			XFillRectangle(d->disp, d->bar.win, d->gcs.border, 0, 2, DisplayWidth(d->disp, DefaultScreen(d->disp)), d->th - 4);	

			for(i = 0; i < d->views->ndata; i++)
				{
					SubView *v = (SubView *)d->views->data[i];

					if(v->tile)
						{
							XSetWindowBackground(d->disp, v->button, (d->cv == v) ? d->colors.focus : d->colors.norm);
							XClearWindow(d->disp, v->button);
							XDrawString(d->disp, v->button, d->gcs.font, 3, d->fy - 1, v->name, strlen(v->name));
						}
				}
		}
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
	SubView *cv = NULL;
	SubClient *lc = NULL;

	assert(d->views && win);

	class = subEwmhGetProperty(win, XA_STRING, SUB_EWMH_WM_CLASS, NULL);

	/* Loop through each view and rule */
	for(i = 0; i < d->views->ndata; i++)
		{
			SubView *v = (SubView *)d->views->data[i];

			for(j = 0; j < v->rules->ndata; j++)
				{
					SubRule *r = (SubRule *)v->rules->data[j];

					if(r->regex && !regexec(r->regex, class, 0, NULL, 0))
						{
							SubClient *c = subClientNew(win);

							if(c->flags & SUB_STATE_TRANS) 
								{
									c->tile = d->cv->tile;
									subClientToggle(c, SUB_STATE_FLOAT);
								}

							if(!v->tile) ViewNew(v);
							if(!r->tile)
								{
									/* Create new rule */
									r->tile = subTileNew(SUB_TYPE_VERT);
									//r->tile->flags	|= SUB_TYPE_RULE|SUB_STATE_RESIZE;
									r->tile->flags	|= SUB_TYPE_RULE;
									r->tile->size		= r->size > 0 ? r->size : 100;
									r->tile->tile		= v->tile;

									subArrayPush(v->tile->clients, (void *)r->tile);
								}

							subArrayPush(r->tile->clients, (void *)c);
							XReparentWindow(d->disp, c->frame, v->frame, 0, 0);
							c->tile = r->tile;

							/* EWMH: Desktop */
							vid = subArrayFind(d->views, (void *)v);
							subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

							/* Map only if view is the current view */
							if(d->cv == v) subClientMap(c);
							else XMapSubwindows(d->disp, c->frame);
							cv = v;

							/* Mark clients as multi */
							if(++merged >= 2) 
								{
									c->flags	|= SUB_STATE_MULTI;
									lc->flags	|= SUB_STATE_MULTI;
								}
							lc = c;

							subTileConfigure(v->tile);
							printf("Adding client %s (%s/%d)\n", c->name, v->name, j);
						}
				}
		}
	
	subViewConfigure();
	subViewRender();
	subViewJump(cv);

	XFree(class);
} /* }}} */

 /** subViewJump {{{
	* Jump to view
	* @param[in] v A #SubView
	**/

void
subViewJump(SubView *v)
{
	long vid = 0;

	assert(v);

	if(d->cv == v) return; /* Just skip */
	if(d->cv)
		{
			XUnmapWindow(d->disp, d->cv->frame);
			if(d->cv != d->views->data[0] && d->cv->tile->clients->ndata == 0) ViewDelete(d->cv);
		}
	d->cv = v;

	XMapWindow(d->disp, d->cv->frame);
	XMapSubwindows(d->disp, d->cv->frame);
	XMapRaised(d->disp, d->bar.win);

	subTileRemap(v->tile);

	/* EWMH: Desktops */
	vid = subArrayFind(d->views, v) + 1;
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

	subViewConfigure();
	subViewRender();

	printf("Switching view (%s)\n", d->cv->name);
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
	
	subArrayKill(v->rules, True);

	free(v->name);
	free(v);					

	subUtilLogDebug("kill=tile\n");
} /* }}} */
