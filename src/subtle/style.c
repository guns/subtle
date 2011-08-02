 /**
  * @package subtle
  *
  * @file Style functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

/* StyleInheritSides {{{ */
static void
StyleInheritSides(SubSides *s1,
  SubSides *s2)
{
  if(-1 == s1->top)    s1->top    = s2->top;
  if(-1 == s1->right)  s1->right  = s2->right;
  if(-1 == s1->bottom) s1->bottom = s2->bottom;
  if(-1 == s1->left)   s1->left   = s2->left;
} /* }}} */

/* StyleInherit {{{ */
static void
StyleInherit(SubStyle *s1,
  SubStyle *s2,
  int sanitize)
{
  assert(s1 && s2);

  /* Inherit nset colors */
  if(-1 == s1->fg)     s1->fg     = s2->fg;
  if(-1 == s1->bg)     s1->bg     = s2->bg;
  if(-1 == s1->icon)   s1->icon   = s2->icon;
  if(-1 == s1->top)    s1->top    = s2->top;
  if(-1 == s1->right)  s1->right  = s2->right;
  if(-1 == s1->bottom) s1->bottom = s2->bottom;
  if(-1 == s1->left)   s1->left   = s2->left;

  /* Sanitize icon */
  if(sanitize && -1 == s1->icon) s1->icon = s1->fg;

  /* Inherit unset border, padding and margin */
  StyleInheritSides(&s1->border,  &s2->border);
  StyleInheritSides(&s1->padding, &s2->padding);
  StyleInheritSides(&s1->margin,  &s2->margin);

  /* Check styles */
  if(s1->styles)
    {
      int i;

      /* Inherit unset values from parent style */
      for(i = 0; i < s1->styles->ndata; i++)
        {
          SubStyle *style = STYLE(s1->styles->data[i]);

          StyleInherit(style, s1, True);
        }
    }
} /* }}} */

/* Public */

 /** subStyleNew {{{
  * @brief Create new style
  * @return Returns a new #SubStyle or \p NULL
  **/

SubStyle *
subStyleNew(void)
{
  SubStyle *s = NULL;

  /* Create new style */
  s = STYLE(subSharedMemoryAlloc(1, sizeof(SubStyle)));
  s->flags |= SUB_TYPE_STYLE;

  /* Init style values */
  subStyleReset(s, -1);

  subSharedLogDebugSubtle("new=style\n");

  return s;
} /* }}} */

 /** subStylePush {{{
  * @brief Push style state
  * @param[in]  s1  A #SubStyle
  * @param[in]  s2  A #SubStyle
  **/

void
subStylePush(SubStyle *s1,
  SubStyle *s2)
{
  assert(s1 && s2);

  if(!s1->styles) s1->styles = subArrayNew();
  subArrayPush(s1->styles, (void *)s2);
} /* }}} */

 /** subStyleFind {{{
  * @brief Find style
  * @param[in]    s      A #SubStyle
  * @param[in]    name   Name of style state
  * @param[inout] idx    Index of found state
  * @return Returns found #SubStyle or \p NULL
  **/

SubStyle *
subStyleFind(SubStyle *s,
  char *name,
  int *idx)
{
  SubStyle *found = NULL;

  assert(s);

  if(s->styles && name)
    {
      int i;

      /* Check each state */
      for(i = 0; i < s->styles->ndata; i++)
        {
          SubStyle *style = STYLE(s->styles->data[i]);

          /* Compare state name */
          if(0 == strcmp(name, style->name))
            {
              found = style;
              if(idx) *idx = i;

              break;
            }
        }
    }

  return found;
} /* }}} */

 /** subStyleReset {{{
  * Reset style values
  * @param[in]  s  A #SubStyle
  **/

void
subStyleReset(SubStyle *s,
  int val)
{
  assert(s);

  /* Set value */
  s->fg = s->bg = s->top = s->right = s->bottom = s->left = val;
  s->border.top  = s->border.right  = s->border.bottom  = s->border.left  = val;
  s->padding.top = s->padding.right = s->padding.bottom = s->padding.left = val;
  s->margin.top  = s->margin.right  = s->margin.bottom  = s->margin.left  = val;

  /* Force value to prevent inheriting of 0 value from all */
  s->icon = -1;

  /* Remove states */
  if(s->styles) subArrayKill(s->styles, True);
  s->styles = NULL;
} /* }}} */

 /** subStyleKill {{{
  * @brief Kill a style
  * @param[in]  s  A #SubStyle
  **/

void
subStyleKill(SubStyle *s)
{
  assert(s);

  if(s->name)   free(s->name);
  if(s->styles) subArrayKill(s->styles, True);
  free(s);

  subSharedLogDebugSubtle("kill=style\n");
} /* }}} */

/* All */

 /** subStyleInheritance {{{
  * Inherit style values from all
  **/

void
subStyleInheritance(void)
{
  /* Inherit styles */
  StyleInherit(&subtle->styles.views,     &subtle->styles.all, False);
  StyleInherit(&subtle->styles.title,     &subtle->styles.all, False);
  StyleInherit(&subtle->styles.sublets,   &subtle->styles.all, False);
  StyleInherit(&subtle->styles.separator, &subtle->styles.all, False);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
