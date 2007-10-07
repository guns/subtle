
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int nviews = 0;
static SubView *root = NULL;

static void
UpdateViews(void)
{
	int i = 0;
	SubView *v = root;
	Window *wins = NULL;

	/* EWMH: Virtual roots */
	wins = (Window *)subUtilAlloc(nviews, sizeof(Window));
	while(v)
		{
			if(v->w) wins[i++] = v->w->frame;
			v = v->next;
		}
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, nviews);
	free(wins);

	/* EWMH: Desktops */
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, nviews);
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, d->cv->n);
}

 /**
	* Create a view
	* @param name Name of the view
	* @return Returns a new #SubWin
	**/

SubView *
subViewNew(char *name)
{
	SubView *tail = NULL, *v = NULL;

	assert(name);
	
	v	= (SubView *)subUtilAlloc(1, sizeof(SubView));
	v->name			= strdup(name);
	v->width		=	strlen(v->name) * d->fx + 2;

	if(!root) root = v;
	else
		{
			tail = root;
			while(tail->next) tail = tail->next;
			tail->next = v;
		}

	printf("Adding view (%s)\n", v->name);
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
			XUnmapWindow(d->dpy, v->button);
			nviews--;

			subViewConfigure();
		}
}

 /**
	* Render a screen 
	* @param v A #SubView
	**/

void
subViewRender(void)
{
	SubView *v = root;

	XClearWindow(d->dpy, d->bar.win);
	XFillRectangle(d->dpy, d->bar.win, d->gcs.border, 0, 2, DisplayWidth(d->dpy, DefaultScreen(d->dpy)), d->th - 4);	

	while(v)
		{
			if(v->w)
				{
					XSetWindowBackground(d->dpy, v->button, (d->cv == v) ? d->colors.focus : d->colors.norm);
					XClearWindow(d->dpy, v->button);
					XDrawString(d->dpy, v->button, d->gcs.font, 1, d->fy - 4, v->name, strlen(v->name));
				}
			v = v->next;
		}
}

 /**
	* Configure view bar
	**/

void
subViewConfigure(void)
{
	if(root)
		{
			int  width = 3;
			SubView *v = root;

			while(v)
				{
					if(v->w)
						{
							XMoveResizeWindow(d->dpy, v->button, width, 2, v->width, d->th - 6);
							width += v->width + 6;
						}
					v = v->next;
				}
			XMoveResizeWindow(d->dpy, d->bar.views, 0, 0, width, d->th);
		}
}

 /**
	* Kill views 
	**/

void
subViewKill(void)
{
	if(root)
		{
			SubView *v = root;

			printf("Removing all views\n");

			while(v)
				{
					SubView *next = v->next;

					XUnmapWindow(d->dpy, v->button);
					XDeleteContext(d->dpy, v->button, 1);
					XDeleteContext(d->dpy, v->w->frame, 1);
					XDestroyWindow(d->dpy, v->button);
					XDestroyWindow(d->dpy, v->w->frame);

					free(v->name);
					free(v);					

					v = next;
				}

			XDestroyWindow(d->dpy, d->bar.win);
			XDestroyWindow(d->dpy, d->bar.views);
			XDestroyWindow(d->dpy, d->bar.sublets);
		}
}

 /**
	* Switch screen
	* @param w New active screen
	**/

void
subViewSwitch(SubView *v)
{
	if(v) 
		{
			if(d->cv != v)	
				{
					subWinUnmap(d->cv->w);
					d->cv = v;
				}
			if(!v->w)
				{
					v->w = subTileNew(SUB_WIN_TILE_HORZ);
					v->w->flags |= SUB_WIN_TYPE_VIEW;
					v->button = XCreateSimpleWindow(d->dpy, d->bar.views, 0, 0, 1, d->th - 6, 1, d->colors.border, d->colors.norm);

					XSaveContext(d->dpy, v->button, 1, (void *)v);
				}

			XMapRaised(d->dpy, d->bar.win);
			XMapRaised(d->dpy, d->cv->button);
			subWinMap(d->cv->w);

			/* EWMH: Desktops */
			subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, d->cv->n);

			subViewConfigure();
			subViewRender();

			printf("Switching to view (%s)\n", d->cv->name);
		}
}
