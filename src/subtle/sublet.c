
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
  SubSublet *s = NULL;

  /* Create new sublet */
  s = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));
  s->flags = SUB_TYPE_SUBLET;
  s->time  = subSharedTime();
  s->text  = subArrayNew();
  s->bg    = subtle->colors.bg_sublets;

  /* Create button */
  s->win = XCreateSimpleWindow(subtle->dpy, subtle->windows.panel1, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.bg_sublets);

  XSaveContext(subtle->dpy, s->win, BUTTONID, (void *)s);
  XMapRaised(subtle->dpy, s->win);

  subSharedLogDebug("new=sublet\n");

  return s;
} /* }}} */ 

 /** subSubletUpdate {{{
  * @brief Update sublet panel
  **/

void
subSubletUpdate(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int i;

      for(i = 0; i < subtle->sublets->ndata; i++)
        {
          SubSublet *s = SUBLET(subtle->sublets->data[i]);

          XResizeWindow(subtle->dpy, s->win, s->width, subtle->th);
        }
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
      int j, k, width = 0;
      XGCValues gvals;
      SubSublet *s = NULL;
      SubText *t = NULL;

      /* Render every sublet */
      for(j = 0; j < subtle->sublets->ndata; j++)
        {
          s     = SUBLET(subtle->sublets->data[j]);
          width = 3;

          XSetWindowBackground(subtle->dpy, s->win, s->bg);
          XClearWindow(subtle->dpy, s->win);

          /* Render text part */
          for(k = 0; k < s->text->ndata; k++)
            {
              if((t = TEXT(s->text->data[k])) && t->flags & SUB_DATA_STRING) ///< Text
                {
                  /* Update GC */
                  gvals.foreground = t->color;
                  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

                  XDrawString(subtle->dpy, s->win, subtle->gcs.font, width,
                    subtle->fy, t->data.string, strlen(t->data.string));

                  width += t->width;
                }
              else if(t->flags & SUB_DATA_NUM) ///< Icon
                {
                  SubIcon *i = NULL;
                  int x = (0 == k) ? 0 : 2; ///< Add spacing when icon isn't first
                  
                  if((i = ICON(subtle->icons->data[t->data.num])))
                    {
                      subIconRender(i, s->win, width + x, abs(subtle->th - i->height) / 2, 
                        t->color, subtle->colors.bg_sublets);

                      /* Add spacing when isn't last */
                      width += i->width + x + (k != s->text->ndata - 1 ? 2 : 0); 
                    }
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
  int i = 0, id = 0, left = 0, right = 0;
  char *tok = NULL;
  unsigned long color = 0;
  SubText *t = NULL;

  assert(s);

  color    = subtle->colors.fg_sublets;
  s->width = 6;

  /* Split and iterate over tokens */
  while((tok = strsep(&data, SEPARATOR)))
    {
      if('#' == *tok) color = atol(tok + 1); ///< Color
      else if('\0' != *tok) ///< Text or icon
        {
          /* Recycle items */
          if(i < s->text->ndata && (t = TEXT(s->text->data[i])))
            {
              if(t->flags & SUB_DATA_STRING && t->data.string)
                free(t->data.string);
            }
          else if((t = TEXT(subSharedMemoryAlloc(1, sizeof(SubText)))))
            subArrayPush(s->text, (void *)t);

          /* Get icon from id */
          if('!' == *tok && 0 <= (id = atoi(tok + 1)) && id <= subtle->icons->ndata)
            {
              SubIcon *p = ICON(subtle->icons->data[id]);

              t->flags    = SUB_TYPE_TEXT|SUB_DATA_NUM;
              t->data.num = id;
              t->width    = p->width + (0 == i ? 2 : 4); ///< Add spacing and check if icon is first
            }
          else
            {
              t->flags       = SUB_TYPE_TEXT|SUB_DATA_STRING;
              t->data.string = strdup(tok);
              t->width       = subSharedTextWidth(tok, strlen(tok), &left, &right, False);

              if(0 == i) t->width -= left; ///< Remove left bearing from first text item
            }

          t->color  = color;
          s->width += t->width; ///< Add spacing
          i++;
        }
    }
  
  /* Fix spacing of last item */
  if(t && t->flags & SUB_DATA_STRING)
    {
      s->width -= right;
      t->width -= right;
    }
  else if(t && t->flags & SUB_DATA_NUM)
    {
      s->width -= 2;
      t->width -= 2;
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
  
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata, sizeof(char *));

  /* Collect names */
  for(i = 0; i < subtle->sublets->ndata; i++)
    names[i] = SUBLET(subtle->sublets->data[i])->name;

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
      if(subtle->panel == PANEL(s)) subtle->panel = PANEL(s->next);
      else
        {
          SubPanel *iter = subtle->panel;

          while(iter && iter->next != PANEL(s)) iter = iter->next;

          iter->next = PANEL(s->next);
        }

      subRubyRemove(s->name); ///< Remove class definition
      subRubyRelease(s->recv); ///< Release from shelter
    }

#ifdef HAVE_SYS_INOTIFY_H
  /* Tidy up inotify */
  if(s->flags & SUB_SUBLET_INOTIFY)
    inotify_rm_watch(subtle->notify, s->interval);
#endif /* HAVE_SYS_INOTIFY_H */ 

  XDeleteContext(subtle->dpy, s->win, BUTTONID);
  XDestroyWindow(subtle->dpy, s->win);

  printf("Unloaded sublet (%s)\n", s->name);

  if(s->name) free(s->name);
  subArrayKill(s->text, True);
  free(s);

  subSharedLogDebug("kill=sublet\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
