
 /**
	* @package subtle
	*
	* @file View functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
	* @version $Id$
	*
	* This program can be distributed under the terms of the GNU GPL.
	* See the file COPYING. 
	**/

#include "subtle.h"

 /** subViewNew {{{
	* @brief Create a view
	* @param[in] name	Name of the view
	* @param[in] tags	Tags for the view
	* @return Returns a #SubView or \p NULL
	**/

SubView *
subViewNew(char *name,
	char *tags)
{
	char c;
	int mnemonic = 0;
	SubView *v = NULL;
	XSetWindowAttributes attrs;

	assert(name);
	
	v	= VIEW(subUtilAlloc(1, sizeof(SubView)));
	v->flags		= SUB_TYPE_VIEW;
	v->name			= strdup(name);
	v->width		=	strlen(v->name) * d->fx + 8; ///< Font offset

	/* Create windows */
	attrs.event_mask = KeyPressMask;

	v->frame	= WINNEW(DefaultRootWindow(d->disp), 0, d->th, DisplayWidth(d->disp, DefaultScreen(d->disp)),
		DisplayHeight(d->disp, DefaultScreen(d->disp)), 0, CWEventMask);	
	v->button	= XCreateSimpleWindow(d->disp, d->bar.views, 0, 0, 1, d->th, 0, d->colors.border, d->colors.norm);

	/* Mnemonic */
	c = name[0];
	mnemonic = (int)XStringToKeysym(&c); ///< Convert char to keysym
	XSaveContext(d->disp, d->bar.views, mnemonic, (void *)v);

	XSaveContext(d->disp, v->frame, 2, (void *)v);
	XSaveContext(d->disp, v->button, 1, (void *)v);
	XMapWindow(d->disp, v->button);

	/* Tags */
	if(tags)
		{
			int i;
			regex_t *preg = subUtilRegexNew(tags);

			for(i = 0; i < d->tags->ndata; i++)
				if(subUtilRegexMatch(preg, TAG(d->tags->data[i])->name)) v->tags |= (1L << (i + 1));

			subUtilRegexKill(preg);
			subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
		}

	printf("Adding view (%s)\n", v->name);
	subUtilLogDebug("new=view, name=%s\n", name);

	return(v);
} /* }}} */

 /** subViewConfigure {{{
	* @brief Configure view
	* @param[in] v	A #SubView
	**/

void
subViewConfigure(SubView *v)
{
	int i, j = 0;
	long vid = 0;
	SubClient *c = NULL;
	unsigned int x = 0, y = 0, width = 0, height = 0, 
		ch = 0, cw = 0, comp = 0, total = 0, 
		shaded = 0, resized = 0, full = 0, floated = 0, special = 0;

	assert(v);

	vid = subArrayFind(d->views, (void *)v);

	if(0 < d->clients->ndata)
		{
			y				= d->th;
			width		= DisplayWidth(d->disp, DefaultScreen(d->disp));
			height	= DisplayHeight(d->disp, DefaultScreen(d->disp)) - d->th;

			/* Find clients */
			for(i = 0; i < d->clients->ndata; i++)
				{
					c = CLIENT(d->clients->data[i]);

					if(v->tags & c->tags)
						{
							/* Count clients in special states */
							if(c->flags & SUB_STATE_SHADE) shaded++;
							else if(c->flags & SUB_STATE_FULL) full++;
							else if(c->flags & SUB_STATE_FLOAT) floated++;
							total++;
						}
				}

			special = shaded + resized + full + floated;
			total		= total > special ? total - special : 1; ///< Prevent division by zero
			cw			= width / total;
			ch			= height;
			comp 		= abs(width - total * cw - shaded * d->th); ///< Compensation for int rounding

			printf("configure: total=%d, special=%d, cw=%d, ch=%d, comp=%d\n", total, special, cw, ch, comp);

			for(i = 0; i < d->clients->ndata; i++)
				{
					c = CLIENT(d->clients->data[i]);

					if(v->tags & c->tags && !(c->flags & (SUB_STATE_TRANS|SUB_STATE_FLOAT|SUB_STATE_FULL)))
						{
							XReparentWindow(d->disp, c->frame, v->frame, 0, 0);

							c->x			= x;
							c->y			= (c->flags & SUB_STATE_SHADE) ? y : shaded * d->th;
							c->width	= cw;
							c->height	= (c->flags & SUB_STATE_SHADE) ? d->th : ch;
							j++;

							if(0 < shaded) c->height = ch - shaded * d->th;
							if(total == j) c->width += comp; ///< Add compensation

							if(c->flags & SUB_STATE_SHADE) y += d->th;
							else x += c->width;

							/* EWMH: Desktop */
							subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

							subClientMap(c);
							subClientConfigure(c);
						}
				}
		}
} /* }}} */

 /** subViewUpdate {{{ 
	* @brief Update all views
	**/

void
subViewUpdate(void)
{
	if(0 < d->views->ndata)
		{
			int i, width = 0;

			for(i = 0; i < d->views->ndata; i++)
				{
					SubView *v = VIEW(d->views->data[i]);

					XMoveResizeWindow(d->disp, v->button, width, 0, v->width, d->th);
					width	+= v->width;
				}
			if(0 < width) XMoveResizeWindow(d->disp, d->bar.views, 0, 0, width, d->th); ///< Sanity
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
			XFillRectangle(d->disp, d->bar.win, d->gcs.border, 0, 2, 
				DisplayWidth(d->disp, DefaultScreen(d->disp)), d->th - 4);	

			for(i = 0; i < d->views->ndata; i++)
				{
					SubView *v = VIEW(d->views->data[i]);

					XSetWindowBackground(d->disp, v->button, (d->cv == v) ? d->colors.focus : d->colors.norm);
					XClearWindow(d->disp, v->button);
					XDrawString(d->disp, v->button, d->gcs.font, 3, d->fy - 1, v->name, strlen(v->name));
				}
		}
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
			XUnmapSubwindows(d->disp, d->cv->frame);
		}
	d->cv = v;

	subViewConfigure(v);

	XMapWindow(d->disp, d->cv->frame);
	XMapSubwindows(d->disp, d->cv->frame);
	XMapWindow(d->disp, d->bar.win);

	/* EWMH: Current desktops */
	vid = subArrayFind(d->views, (void *)v); ///< Get desktop number
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

	subViewRender();
	subKeyGrab(d->cv->frame);

	printf("Switching view (%s)\n", d->cv->name);
} /* }}} */

 /** subViewPublish {{{
	* @brief Publish views
	**/

void
subViewPublish(void)
{
	int i;
	long vid = 0;
	char **names = NULL;
	Window *frames = NULL;

	assert(0 < d->views->ndata);

	frames	= (Window *)subUtilAlloc(d->views->ndata, sizeof(Window));
	names 	= (char **)subUtilAlloc(d->views->ndata, sizeof(char *));

	for(i = 0; i < d->views->ndata; i++)
		{
			SubView *v = VIEW(d->views->data[i]);

			frames[i]	= v->frame;
			names[i]	= v->name;
		}

	/* EWMH: Virtual roots */
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_VIRTUAL_ROOTS, frames, i);

	/* EWMH: Desktops */
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
	subEwmhSetStrings(DefaultRootWindow(d->disp), SUB_EWMH_NET_DESKTOP_NAMES, names, i);

	/* EWMH: Current desktop */
	vid = subArrayFind(d->views, (void *)d->cv); ///< Get desktop number
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

	subUtilLogDebug("publish=views, n=%d\n", i);

	free(frames);
	free(names);
} /* }}} */

 /** subViewKill {{{
	* @brief Kill view 
	* @param[in] v	A #SubView
	**/

void
subViewKill(SubView *v)
{
	int mnemonic = 0;

	assert(v);

	printf("Killing view (%s)\n", v->name);

	mnemonic = (int)XStringToKeysym(&v->name[0]); ///< Convert char to keysym

	XDeleteContext(d->disp, v->frame, 1);
	XDeleteContext(d->disp, v->button, 1);
	XDeleteContext(d->disp, d->bar.views, mnemonic);

	XDestroyWindow(d->disp, v->button);
	XDestroyWindow(d->disp, v->frame);

	free(v->name);
	free(v);					

	subUtilLogDebug("kill=view\n");
} /* }}} */
