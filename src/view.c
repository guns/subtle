
 /**
	* @package subtle
	*
	* @file View functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

/* ViewNew {{{ */
static void
ViewNew(SubView *v)
{
	assert(v);

	v->tile = subTileNew(SUB_TYPE_HORZ, (void *)v);
	v->tile->flags 		|= SUB_TYPE_VIEW;
	v->tile->superior = (void *)v;
	v->frame	= XCreateSimpleWindow(d->disp, DefaultRootWindow(d->disp), 0, d->th, 
		DisplayWidth(d->disp, DefaultScreen(d->disp)), DisplayHeight(d->disp, DefaultScreen(d->disp)), 0, 
		d->colors.border, d->colors.norm);
	v->button	= XCreateSimpleWindow(d->disp, d->bar.views, 0, 0, 1, d->th, 0, d->colors.border, d->colors.norm);

	XSaveContext(d->disp, v->button, 1, (void *)v);
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

	subTileKill(v->tile, True);
	v->tile = NULL;
} /* }}} */

/* ViewPush {{{ */
void
ViewPush(SubView *v,
	SubRule *r,
	SubClient *c)
{
	long vid = 0;

	subArrayPush(r->tile->clients, (void *)c);
	XReparentWindow(d->disp, c->frame, v->frame, 0, 0);
	c->tile = r->tile;

	/* EWMH: Desktop */
	vid = subArrayFind(d->views, (void *)v);
	subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

	/* Map only if view is the current view */
	if(!d->cv) d->cv = v;
	if(d->cv == v) subClientMap(c);
	else XMapSubwindows(d->disp, c->frame);

	subTileConfigure(v->tile);
	printf("Adding client %s (%s)\n", c->name, v->name);
} /* }}} */


 /** subViewNew {{{
	* @brief Create a view
	* @param[in] name	Name of the view
	* @return Returns a #SubClient or \p NULL
	**/

SubView *
subViewNew(char *name)
{
	SubView *v = NULL;

	assert(name);
	
	v	= (SubView *)subUtilAlloc(1, sizeof(SubView));
	v->flags	= SUB_TYPE_VIEW;
	v->name		= strdup(name);
	v->width	=	strlen(v->name) * d->fx + 8; ///< Font offset

	printf("Adding view (%s)\n", v->name);
	subUtilLogDebug("new=view, name=%s\n", name);

	return(v);
} /* }}} */

 /** subViewConfigure {{{
	* @brief Configure all views
	**/

void
subViewConfigure(void)
{
	if(0 < d->views->ndata)
		{
			int i = 0, width = 0, nv = 0;
			char **names = NULL;
			Window *wins = NULL;

			assert(d->views->ndata > 0);

			wins	= (Window *)subUtilAlloc(d->views->ndata, sizeof(Window));
			names = (char **)subUtilAlloc(d->views->ndata, sizeof(char *));

			for(i = 0; i < d->views->ndata; i++)
				{
					SubView *v = VIEW(d->views->data[i]);

					/* Only views with tiles */
					if(v->tile)
						{
							XMoveResizeWindow(d->disp, v->button, width, 0, v->width, d->th);
							width				+= v->width;
							wins[nv]		= v->frame;
							names[nv++]	= v->name;
						}
				}

			if(0 < width) XMoveResizeWindow(d->disp, d->bar.views, 0, 0, width, d->th); ///< Sanity

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
	* @brief Render all views
	* @param[in] v	A #SubView
	**/

void
subViewRender(void)
{
	if(0 < d->views->ndata)
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
	* @brief Merge window into views
	* @param[in] win	Window
	**/

void
subViewMerge(Window win)
{
	int i, j, merged = 0;
	char *class = NULL;
	SubView *lv = NULL;
	SubClient *lc = NULL;

	assert(d->views && win);

	class = subEwmhGetProperty(win, XA_STRING, SUB_EWMH_WM_CLASS, NULL);

	/* Loop through each view and rule */
	for(i = 0; i < d->views->ndata; i++)
		{
			SubView *v = VIEW(d->views->data[i]);

			for(j = 0; j < v->rules->ndata; j++)
				{
					SubRule *r = RULE(v->rules->data[j]);

					/* Regex match and permit multi tags only on different views */
					if(r->regex && !regexec(r->regex, class, 0, NULL, 0) && lv != v) 
						{
							SubClient *c = subClientNew(win);

							if(c->flags & SUB_STATE_TRANS) 
								{
									c->tile = v->tile;
									subClientToggle(c, SUB_STATE_FLOAT);
								}

							if(!v->tile) ViewNew(v);
							if(!r->tile)
								{
									/* Create new rule */
									r->tile = subTileNew(SUB_TYPE_VERT, (void *)r);
									//r->tile->flags	|= SUB_TYPE_RULE|SUB_STATE_RESIZE;
									r->tile->flags	|= SUB_TYPE_RULE;
									r->tile->size		= 0 < r->size? r->size : 100;
									r->tile->tile		= v->tile;

									subArrayPush(v->tile->clients, (void *)r->tile);
								}
							ViewPush(v, r, c);

							/* Mark current and previus client as multi */
							if(2 <= ++merged) 
								{
									c->flags	|= SUB_STATE_MULTI;
									lc->flags	|= SUB_STATE_MULTI;

									/* Store multi clients in array for reference */
									if(!lc->multi) 
										{
											lc->multi = subArrayNew();
											subArrayPush(lc->multi, (void *)lc);
										}
									subArrayPush(lc->multi, (void *)c);
									c->multi = lc->multi;
								}
							lc = c;
							lv = v;
						}
				}
		}

	/* Handle untagged clients */
	if(0 == merged)
		{
			SubView *v = subViewNew(PACKAGE_NAME);
			SubRule *r = subRuleNew(".*", 100);
			SubClient *c = subClientNew(win);

			v->rules = subArrayNew();
			subArrayPush(v->rules, (void *)r);
			subArrayPush(v->tile->clients, (void *)r->tile);

			ViewPush(v, r, c);
		}
	XFree(class);
} /* }}} */

 /** subViewJump {{{
	* @brief Jump to view
	* @param[in] v	A #SubView
	**/

void
subViewJump(SubView *v)
{
	long vid = 0;

	assert(v);

	if(d->cv)
		{
			XUnmapWindow(d->disp, d->cv->frame);
			if(d->cv->tile->clients->ndata == 0) ViewDelete(d->cv); ///< Delete views without clients
		}
	d->cv = v;

	XMapWindow(d->disp, d->cv->frame);
	XMapSubwindows(d->disp, d->cv->frame);
	XMapWindow(d->disp, d->bar.win);

	if(v->tile) subTileRemap(v->tile); ///< Remap multi clients

	/* EWMH: Desktops */
	vid = subArrayFind(d->views, (void *)v) + 1; ///< Adjust desktop number
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

	subViewConfigure();
	subViewRender();

	printf("Switching view (%s)\n", d->cv->name);
} /* }}} */

 /** subViewKill {{{
	* @brief Kill view 
	* @param[in] v	A #SubView
	**/

void
subViewKill(SubView *v)
{
	assert(v);

	printf("Killing view (%s)\n", v->name);

	/* Active views need more cleaning */
	if(v->tile)
		{
			XDeleteContext(d->disp, v->button, 1);
			XDestroyWindow(d->disp, v->button);

			subTileKill(v->tile, True);
			XDeleteContext(d->disp, v->frame, 1);
			XDestroyWindow(d->disp, v->frame);
		}
	
	subArrayKill(v->rules, True);

	free(v->name);
	free(v);					

	subUtilLogDebug("kill=view\n");
} /* }}} */
