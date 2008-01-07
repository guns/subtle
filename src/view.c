
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Header$
	**/

#include "subtle.h"

static int nviews = 0;
static SubView *last = NULL;

static void
TagView(SubView *v)
{
	assert(v);
	v->w = subTileNew(SUB_WIN_TILE_HORZ);
	v->w->flags |= SUB_WIN_TYPE_VIEW;
	v->button = XCreateSimpleWindow(d->disp, d->bar.views, 0, 0, 1, d->th, 0, d->colors.border, d->colors.norm);

	XMapWindow(d->disp, v->button);
	XSaveContext(d->disp, v->button, 1, (void *)v);
}

static void
UntagView(SubView *v)
{
	assert(v->w);
	XDestroySubwindows(d->disp, v->w->frame);
	XDeleteContext(d->disp, v->button, 1);
	XDestroyWindow(d->disp, v->button);
	subTileDelete(v->w);
	free(v->w);
	v->w = NULL;
}

 /**
	* Find view
	* @param xid View id
	* @return Returns the found #SubView or NULL
	**/

SubView *
subViewFind(int xid)
{
	int i;
	SubView *v = d->rv;

	while(v)
		{
			if(v->w)
				{
					if(i == xid) return(v);
					i++;
				}
			v = v->next;
		}
	return(NULL);
}

 /**
	* Create a view
	* @param name Name of the view
	* @return Returns a new #SubWin
	**/

SubView *
subViewNew(char *name)
{
	SubView *v = NULL;

	assert(name);
	
	v	= (SubView *)subUtilAlloc(1, sizeof(SubView));
	v->name		= strdup(name);
	v->width	=	strlen(v->name) * d->fx + 8;
	v->xid		= ++nviews;

	if(!d->rv) 
		{
			d->rv = v;
			last	= v;
		}
	else
		{
			last->next = v;
			v->prev = last;
			last = v;
		}

	printf("Adding view %s (%s)\n", v->name, tags ? tags : "default");
	return(v);
}

 /**
	* Delete a view
	* @param w A #SubWin
	**/

void
subViewDelete(SubView *v)
{
	if(v && d->cv != v)
		{
			printf("Removing view (%s)\n", v->name);

			subWinUnmap(v->w);
			XUnmapWindow(d->disp, v->button);
			nviews--;

			subViewConfigure();
		}
}

 /**
	* Merge window
	* @param w A #SubWin
	**/

void
subViewMerge(Window win)
{
	int merged = 0;
	SubView *v = NULL;
	char *class = NULL;
	long xid = 0;

	assert(win && last);

	class = subEwmhGetProperty(win, XA_STRING, SUB_EWMH_WM_CLASS, NULL);

	/* Loop backwards because root view has no regex */
	v = last;
	while(v)
		{
			if((v == d->rv && !merged) || (v->regex && !regexec(v->regex, class, 0, NULL, 0)))
				{
					SubWin *w = subClientNew(win);

					if(w->flags & SUB_WIN_STATE_TRANS) 
						{
							w->parent = d->cv->w;
							subClientToggle(SUB_WIN_STATE_FLOAT, w);
						}
					else w->views |= v->xid;

					/* EWMH: Desktop */
					xid = v->xid - 1;
					subEwmhSetCardinals(w->client->win, SUB_EWMH_NET_WM_DESKTOP, &xid, 1);

					if(!v->w) TagView(v);
					subTileAdd(v->w, w);
					subTileConfigure(v->w);
					merged++;

					XSaveContext(d->disp, w->frame, 1, (void *)w);

					/* Map only if the desired view is the current view */
					if(d->cv == v) subWinMap(w);
					else XMapSubwindows(d->disp, w->frame);

					printf("Adding client %s (%s)\n", w->client->name, v->name);
				}
			v = v->prev;
		}
	
	XFree(class);
	
	subViewConfigure();
	subViewRender();
}

 /**
	* Render views
	* @param v A #SubView
	**/

void
subViewRender(void)
{
	SubView *v = NULL;
	
	assert(d->rv);

	XClearWindow(d->disp, d->bar.win);
	XFillRectangle(d->disp, d->bar.win, d->gcs.border, 0, 2, DisplayWidth(d->disp, DefaultScreen(d->disp)), d->th - 4);	

	v = d->rv;
	while(v)
		{
			if(v->w)
				{
					XSetWindowBackground(d->disp, v->button, (d->cv == v) ? d->colors.focus : d->colors.norm);
					XClearWindow(d->disp, v->button);
					XDrawString(d->disp, v->button, d->gcs.font, 3, d->fy - 1, v->name, strlen(v->name));
				}
			v = v->next;
		}
}

 /**
	* Configure views
	**/

void
subViewConfigure(void)
{
	int i = 0, width = 0;
	char **names = NULL;
	Window *wins = NULL;
	SubView *v = NULL;

	assert(d->rv);

	wins = (Window *)subUtilAlloc(nviews, sizeof(Window));
	names = (char **)subUtilAlloc(nviews, sizeof(char *));

	v = d->rv;
	while(v)
		{
			if(v->w)
				{
					XMoveResizeWindow(d->disp, v->button, width, 0, v->width, d->th);
					width += v->width;
					wins[i] = v->w->frame;
					names[i++]	= v->name;
				}
			v = v->next;
		}
	XMoveResizeWindow(d->disp, d->bar.views, 0, 0, width, d->th);

	/* EWMH: Virtual roots */
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, i);
	free(wins);

	/* EWMH: Desktops */
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
	subEwmhSetStrings(DefaultRootWindow(d->disp), SUB_EWMH_NET_DESKTOP_NAMES, names, i);
	free(names);
}

 /**
	* Kill views 
	**/

void
subViewKill(void)
{
	SubView *v = NULL;

	assert(d->rv);

	printf("Removing all views\n");
	
	v = d->rv;
	while(v)
		{
			SubView *next = v->next;

			XUnmapWindow(d->disp, v->button);
			XDeleteContext(d->disp, v->button, 1);
			XDestroyWindow(d->disp, v->button);

			if(v->w)
				{
					subTileDelete(v->w);
					XDeleteContext(d->disp, v->w->frame, v->xid);
					XDestroyWindow(d->disp, v->w->frame);
					free(v->w);
				}
			if(v->regex) 
				{
					regfree(v->regex);
					free(v->regex);
				}
			free(v->name);
			free(v);					

			v = next;
		}

	XDestroyWindow(d->disp, d->bar.win);
	XDestroyWindow(d->disp, d->bar.views);
	XDestroyWindow(d->disp, d->bar.sublets);
}

 /**
	* Switch view
	* @param v New active view
	**/

void
subViewSwitch(SubView *v)
{
	long xid = 0;
	assert(v);

	if(d->cv != v)	
		{
			subWinUnmap(d->cv->w);
			if(!d->cv->w->tile->first && d->cv != d->rv) UntagView(d->cv);
			d->cv = v;
		}
	if(!v->w) TagView(v);

	XMapRaised(d->disp, d->bar.win);
	subWinMap(d->cv->w);

	/* EWMH: Desktops */
	xid = d->cv->xid - 1;
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_CURRENT_DESKTOP, &xid, 1);

	subViewConfigure();
	subViewRender();

	printf("Switching view (%s)\n", d->cv->name);
}
