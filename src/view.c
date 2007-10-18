
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int nviews = 0;
static SubView *root = NULL, *last = NULL;

static void
TagView(SubView *v)
{
	assert(v);
	v->w = subTileNew(SUB_WIN_TILE_HORZ);
	v->w->flags |= SUB_WIN_TYPE_VIEW;
	v->button = XCreateSimpleWindow(d->dpy, d->bar.views, 0, 0, 1, d->th, 0, d->colors.border, d->colors.norm);

	XMapWindow(d->dpy, v->button);
	XSaveContext(d->dpy, v->button, 1, (void *)v);
}

static void
UntagView(SubView *v)
{
	assert(v->w);
	XDestroySubwindows(d->dpy, v->w->frame);
	XDeleteContext(d->dpy, v->button, 1);
	XDestroyWindow(d->dpy, v->button);
	subTileDelete(v->w);
	free(v->w);
	v->w = NULL;
}

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
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, d->cv->xid);
}

 /**
	* Create a view
	* @param name Name of the view
	* @param tags Tag list
	* @return Returns a new #SubWin
	**/

SubView *
subViewNew(char *name,
	char *tags)
{
	SubView *v = NULL;

	assert(name);
	
	v	= (SubView *)subUtilAlloc(1, sizeof(SubView));
	v->name		= strdup(name);
	v->width	=	strlen(v->name) * d->fx + 8;
	v->xid		= ++nviews;

	if(tags)
		{
			int errcode;
			v->regex = (regex_t *)subUtilAlloc(1, sizeof(regex_t));

			/* Stupid error handling */
			if((errcode = regcomp(v->regex, tags, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
				{
					size_t errsize = regerror(errcode, v->regex, NULL, 0);
					char *errbuf = (char *)subUtilAlloc(1, errsize);

					regerror(errcode, v->regex, errbuf, errsize);

					subUtilLogWarn("Can't compile regex `%s'\n", tags);
					subUtilLogDebug("%s\n", errbuf);

					free(errbuf);
					regfree(v->regex);
					free(v->regex);
					free(v);
					return(NULL);
				}
		}

	if(!root) 
		{
			root = v;
			last = v;
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
			XUnmapWindow(d->dpy, v->button);
			nviews--;

			subViewConfigure();
		}
}

 /**
	* Sift window
	* @param w A #SubWin
	**/

void
subViewSift(Window win)
{
	int sifted = 0;
	SubView *v = last;
	char *name = NULL;

	assert(win);

	XFetchName(d->dpy, win, &name);

	while(v)
		{
			if((v == root && !sifted) || (v->regex && !regexec(v->regex, name, 0, NULL, 0)))
				{
					SubWin *w = subClientNew(win);

					if(w->flags & SUB_WIN_STATE_TRANS) 
						{
							XSaveContext(d->dpy, w->frame, d->cv->xid, (void *)w);
							w->parent = d->cv->w;
							subClientToggle(SUB_WIN_STATE_FLOAT, w);
						}
					else XSaveContext(d->dpy, w->frame, v->xid, (void *)w);

					if(!v->w) TagView(v);
					subTileAdd(v->w, w);
					subTileConfigure(v->w);
					sifted++;

					if(d->cv == v) subWinMap(w);
					else XMapSubwindows(d->dpy, w->frame);

					printf("Adding client %s (%s)\n", w->client->name, v->name);
				}
			v = v->prev;
		}
	
	XFree(name);
	
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
	SubView *v = root;

	XClearWindow(d->dpy, d->bar.win);
	XFillRectangle(d->dpy, d->bar.win, d->gcs.border, 0, 2, DisplayWidth(d->dpy, DefaultScreen(d->dpy)), d->th - 4);	

	while(v)
		{
			if(v->w)
				{
					XSetWindowBackground(d->dpy, v->button, (d->cv == v) ? d->colors.focus : d->colors.norm);
					XClearWindow(d->dpy, v->button);
					XDrawString(d->dpy, v->button, d->gcs.font, 3, d->fy - 1, v->name, strlen(v->name));
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
	if(root)
		{
			int  width = 0;
			SubView *v = root;

			while(v)
				{
					if(v->w)
						{
							XMoveResizeWindow(d->dpy, v->button, width, 0, v->width, d->th);
							width += v->width;
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
					XDestroyWindow(d->dpy, v->button);

					if(v->w)
						{
							subTileDelete(v->w);
							XDeleteContext(d->dpy, v->w->frame, v->xid);
							XDestroyWindow(d->dpy, v->w->frame);
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
	assert(v);

	if(d->cv != v)	
		{
			subWinUnmap(d->cv->w);
			if(!d->cv->w->tile->first && d->cv != root) UntagView(d->cv);
			d->cv = v;
		}
	if(!v->w) TagView(v);

	XMapRaised(d->dpy, d->bar.win);
	subWinMap(d->cv->w);

	/* EWMH: Desktops */
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, d->cv->xid);

	subViewConfigure();
	subViewRender();

	printf("Switching view (%s)\n", d->cv->name);
}
