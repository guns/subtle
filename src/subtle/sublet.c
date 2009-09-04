
 /**
  * @package subtle
  *
  * @file Sublet functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subSubletNew {{{
  * @brief Create a new sublet 
  * @return Returns a #SubSublet or \p NULL
  **/

SubSublet *
subSubletNew(void)
{
  SubSublet *s = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));

  /* Init sublet */
  s->flags = SUB_TYPE_SUBLET;
  s->time  = subSharedTime();
  s->text  = subArrayNew();

  /* Create button */
  s->button = XCreateSimpleWindow(subtle->dpy, subtle->panels.sublets.win, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.bg_sublets);

  XSaveContext(subtle->dpy, s->button, BUTTONID, (void *)s);
  XMapRaised(subtle->dpy, s->button);

  subSharedLogDebug("new=sublet\n");

  return s;
} /* }}} */ 

 /** subSubletUpdate {{{
  * @brief Update sublet bar
  **/

void
subSubletUpdate(void)
{
  subtle->panels.sublets.width = 0;

  if(0 < subtle->sublets->ndata)
    {
      SubSublet *s = NULL;

      for(s = subtle->sublet; s; s = s->next)
        {
          XMoveResizeWindow(subtle->dpy, s->button, subtle->panels.sublets.width, 
            0, s->width, subtle->th);
          subtle->panels.sublets.width += s->width + 6;
        }

      XResizeWindow(subtle->dpy, subtle->panels.sublets.win, 
        subtle->panels.sublets.width, subtle->th);
    }
} /* }}} */

 /** subSubletRender {{{
  * @brief Render sublets
  **/

void
subSubletRender(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int i, width = 0;
      XGCValues gvals;
      SubSublet *s = NULL;
      SubText *t = NULL;

      /* Render every sublet */
      for(s = subtle->sublet; s; s = s->next)
        {
          width = 3;

          XSetWindowBackground(subtle->dpy,  s->button, subtle->colors.bg_sublets);
          XClearWindow(subtle->dpy, s->button);

          /* Render text part */
          for(i = 0; i < s->text->ndata; i++)
            {
              if((t = TEXT(s->text->data[i])) && t->flags & SUB_DATA_STRING)
                {
                  /* Update GC */
                  gvals.foreground = t->color;
                  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

                  XDrawString(subtle->dpy, s->button, subtle->gcs.font, width,
                    subtle->fy, t->data.string, strlen(t->data.string));

                  width += t->width;
                }
              else if(t->flags & SUB_DATA_NUM)
                {
                  SubPixmap *p = PIXMAP(subtle->pixmaps->data[t->data.num]);

                  subPixmapRender(p, s->button, width + 2, abs(subtle->th - p->height) / 2, 
                    t->color, subtle->colors.bg_sublets);

                  width += p->width + 4;
                }
            }
        }

      XSync(subtle->dpy, False); ///< Sync before going on
    }
} /* }}} */

 /** subSubletCompare {{{
  * @brief Compare two sublets
  * @param[in]  a  A #SubSublet
  * @param[in]  b  A #SubSublet
  * @return Returns the result of the comparison of both sublets
  * @retval  -1  a is smaller
  * @retval  0   a and b are equal  
  * @retval  1   a is greater
  **/

int
subSubletCompare(const void *a,
  const void *b)
{
  SubSublet *s1 = *(SubSublet **)a, *s2 = *(SubSublet **)b;

  assert(a && b);
  
  /* Include only interval sublets */
  if(!(s1->flags & (SUB_SUBLET_INTERVAL))) return 1;
  if(!(s2->flags & (SUB_SUBLET_INTERVAL))) return -1;

  return s1->time < s2->time ? -1 : (s1->time == s2->time ? 0 : 1);
} /* }}} */

 /** subSubletSetData {{{
  * @brief Compare two sublets
  * @param[in]  s     A #SubSublet
  * @param[in]  data  Data
  **/

void
subSubletSetData(SubSublet *s,
  char *data)
{
  int i = 0, id = 0;
  char *tok = NULL;
  unsigned long color = 0;
  SubText *t = NULL;

  assert(s);

  color    = subtle->colors.fg_sublets;
  s->width = 0;

  /* Split and iterate over tokens */
  while((tok = strsep(&data, SEPARATOR)))
    {
      if('#' == *tok) color = atol(tok + 1); ///< Color
      else if('\0' != *tok) ///< Text or pixmap
        {
          /* Recycle items */
          if(i < s->text->ndata && (t = TEXT(s->text->data[i])))
            {
              if(t->flags & SUB_DATA_STRING && t->data.string)
                free(t->data.string);
            }
          else if((t = TEXT(subSharedMemoryAlloc(1, sizeof(SubText)))))
            subArrayPush(s->text, (void *)t);

          /* Get pixmap from id */
          if('!' == *tok && 0 <= (id = atoi(tok + 1)) && id <= subtle->pixmaps->ndata)
            {
              SubPixmap *p = PIXMAP(subtle->pixmaps->data[id]);

              t->flags    = SUB_TYPE_TEXT|SUB_DATA_NUM;
              t->data.num = id;
              t->width    = p->width + 4; ///< Add spacer
            }
          else
            {
              t->flags       = SUB_TYPE_TEXT|SUB_DATA_STRING;
              t->data.string = strdup(tok);
              t->width       = XTextWidth(subtle->xfs, tok, strlen(tok)); ///< Font offset
            }

          t->color  = color;
          s->width += t->width;
          i++;
        }
    }
} /* }}} */

 /** subSubletPublish {{{
  * @brief Publish sublets
  **/

void
subSubletPublish(void)
{
  int i = 0;
  char **names = NULL;
  SubSublet *iter = NULL;
  
  iter  = subtle->sublet;
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata, sizeof(char *));

  /* Get list in order */
  while(iter)
    {
      names[i++] = iter->name;
      iter       = iter->next;
    }

  /* EWMH: Sublet list */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST, names, subtle->sublets->ndata);

  subSharedLogDebug("publish=sublet, n=%d\n", subtle->sublets->ndata);

  free(names);
} /* }}} */

 /** subSubletKill {{{
  * @brief Kill sublet
  * @param[in]  s       A #SubSublet
  * @param[in]  unlink  Unlink sublets
  **/

void
subSubletKill(SubSublet *s,
  int unlink)
{
  assert(s);

  if(unlink)
    {
      /* Update linked list */
      if(subtle->sublet == s) subtle->sublet = s->next;
      else
        {
          SubSublet *iter = subtle->sublet;

          while(iter && iter->next != s) iter = iter->next;

          iter->next = s->next;
        }

      subRubyRemove(s->name); ///< Remove class definition
    }

#ifdef HAVE_SYS_INOTIFY_H
  /* Tidy up inotify */
  if(s->flags & SUB_SUBLET_INOTIFY)
    inotify_rm_watch(subtle->notify, s->interval);
#endif /* HAVE_SYS_INOTIFY_H */ 

  XDeleteContext(subtle->dpy, s->button, BUTTONID);
  XDestroyWindow(subtle->dpy, s->button);

  if(s->name) free(s->name);
  subArrayKill(s->text, True);
  free(s);

  subSharedLogDebug("kill=sublet\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
