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

/* Prototypes {{{ */
static void StyleInherit(SubStyle *s1, SubStyle *s2);
/* }}} */

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

/* StyleInheritStates {{{ */
static void
StyleInheritStates(SubStyle *s)
{
  if(s->states)
    {
      int i;

      /* Inherit all values from parent state */
      for(i = 0; i < s->states->ndata; i++)
        {
          SubStyle *state = STYLE(s->states->data[i]);

          StyleInherit(state, s);
        }
    }
} /* }}} */

/* StyleInherit {{{ */
static void
StyleInherit(SubStyle *s1,
  SubStyle *s2)
{
  assert(s1 && s2);

  /* Inherit colors */
  if(-1 == s1->fg)     s1->fg     = s2->fg;
  if(-1 == s1->bg)     s1->bg     = s2->bg;
  if(-1 == s1->top)    s1->top    = s2->top;
  if(-1 == s1->right)  s1->right  = s2->right;
  if(-1 == s1->bottom) s1->bottom = s2->bottom;
  if(-1 == s1->left)   s1->left   = s2->left;

  /* Inherit border, padding and margin */
  StyleInheritSides(&s1->border,  &s2->border);
  StyleInheritSides(&s1->padding, &s2->padding);
  StyleInheritSides(&s1->margin,  &s2->margin);

  /* Inherit states */
  StyleInheritStates(s1);
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

 /** subStyleAddState {{{
  * @brief Add state to style
  * @param[in]  s      A #SubStyle
  * @param[in]  state  A #SubStyle
  **/

void
subStyleAddState(SubStyle *s,
  SubStyle *state)
{
  assert(s);

  if(!s->states) s->states = subArrayNew();

  /* Add state to style */
  subArrayPush(s->states, (void *)state);

  subSharedLogDebugSubtle("Add state: total=%d\n", s->states->ndata);
} /* }}} */

 /** subStyleFindState {{{
  * @brief Find style state
  * @param[in]    s      A #SubStyle
  * @param[in]    name   Name of style state
  * @param[inout] idx    Index of found state
  * @return Returns found #SubStyle or \p NULL
  **/

SubStyle *
subStyleFindState(SubStyle *s,
  char *name,
  int *idx)
{
  SubStyle *found = NULL;

  assert(s);

  if(s->states && name)
    {
      int i;

      /* Check each state */
      for(i = 0; i < s->states->ndata; i++)
        {
          SubStyle *state = STYLE(s->states->data[i]);

          /* Compare state name */
          if(0 == strcmp(name, state->name))
            {
              found = state;
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
  s->fg = s->bg = s->top  = s->right = s->bottom = s->left = val;
  s->border.top  = s->border.right  = s->border.bottom  = s->border.left  = val;
  s->padding.top = s->padding.right = s->padding.bottom = s->padding.left = val;
  s->margin.top  = s->margin.right  = s->margin.bottom  = s->margin.left  = val;

  /* Remove states */
  if(s->states) subArrayKill(s->states, True);
  s->states = NULL;
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
  if(s->states) subArrayKill(s->states, True);
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
  StyleInherit(&subtle->styles.views,     &subtle->styles.all);
  StyleInherit(&subtle->styles.title,     &subtle->styles.all);
  StyleInherit(&subtle->styles.sublets,   &subtle->styles.all);
  StyleInherit(&subtle->styles.separator, &subtle->styles.all);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
