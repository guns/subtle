
 /**
  * @package subtle
  *
  * @file Array functions
  * @copyright 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subArrayNew {{{
  * @brief Create new array and init it
  * @return Returns a #SubArray or \p NULL
  **/

SubArray *
subArrayNew(void)
{
  return (SubArray *)subSharedMemoryAlloc(1, sizeof(SubArray));
} /* }}} */

 /** subArrayPush {{{
  * @brief Push element to array
  * @param[in]  a  A #SubArray
  * @param[in]  e  New element
  **/

void
subArrayPush(SubArray *a,
  void *e)
{
  assert(a);

  if(e)
    {
      a->data = (void **)subSharedMemoryRealloc(a->data, (a->ndata + 1) * sizeof(void *));
      a->data[(a->ndata)++] = e;
    }
} /* }}} */

 /** subArrayRemove {{{
  * @brief Remove element from array
  * @param[in]  a  A #SubArray
  * @param[in]  e  Array element
  **/

void
subArrayRemove(SubArray *a,
  void *e)
{
  int i, idx;

  assert(a && e);

  if(0 <= (idx = subArrayIndex(a, e)))
    {
      for(i = idx; i < a->ndata - 1; i++) 
        a->data[i] = a->data[i + 1];

      a->ndata--;
      a->data = (void **)subSharedMemoryRealloc(a->data, a->ndata * sizeof(void *));
    }
} /* }}} */

 /** subArrayGet {{{
  * @brief Get id after boundary check
  * @param[in]  a    A #SubArray
  * @param[in]  id   Array index
  * @return Returns an element or \p NULL
  **/

void *
subArrayGet(SubArray *a,
  int id)
{
  assert(a);

  if(0 <= id && id <= a->ndata)
    return a->data[id];

  return NULL;
} /* }}} */

 /** subArrayIndex {{{
  * @brief Find array id of element
  * @param[in]  a  A #SubArray
  * @param[in]  e  Element
  * @return Returns found idx or \p -1
  **/

int
subArrayIndex(SubArray *a,
  void *e)
{
  int i;

  assert(a && e);

  for(i = 0; i < a->ndata; i++) 
    if(a->data[i] == e) return i;

  return -1;
} /* }}} */

  /** subArraySort {{{ 
   * @brief Sort array with given compare function
   * @param[in]  a       A #SubArray
   * @param[in]  compar  Compare function
   **/

void
subArraySort(SubArray *a,
  int(*compar)(const void *a, const void *b))
{
  assert(a && compar);

  if(0 < a->ndata) qsort(a->data, a->ndata, sizeof(void *), compar);
} /* }}} */

 /** subArrayClear {{{
  * @brief Delete all elements
  * @param[in]  a      A #SubArray
  * @param[in]  clean  Pass clean to elements
  **/

void
subArrayClear(SubArray *a,
  int clean)
{
  int i;

  assert(a);

  for(i = 0; i < a->ndata; i++)
    {
      /* Check type and kill */
      SubClient *c = CLIENT(a->data[i]);

      /* Common types first */
      if(c->flags & SUB_TYPE_CLIENT)      subClientKill(c, clean);
      else if(c->flags & SUB_TYPE_GRAB)   subGrabKill(GRAB(c), clean);
      else if(c->flags & SUB_TYPE_ICON)   subIconKill(ICON(c));
      else if(c->flags & SUB_TYPE_SCREEN) subScreenKill(SCREEN(c));
      else if(c->flags & SUB_TYPE_SUBLET) subSubletKill(SUBLET(c), clean);
      else if(c->flags & SUB_TYPE_TAG)    subTagKill(TAG(c));
      else if(c->flags & SUB_TYPE_TRAY)   subTrayKill(TRAY(c));
      else if(c->flags & SUB_TYPE_VIEW)   subViewKill(VIEW(c));
      else if(c->flags & SUB_TYPE_TEXT)
        {
          SubText *t = TEXT(c);

          if(t->flags & SUB_DATA_STRING && t->data.string) 
            free(t->data.string); ///< Special case

          free(t);
        }
      else free(a->data[i]); 
    }

  if(a->data) free(a->data);

  a->data  = NULL;
  a->ndata = 0;
} /* }}} */

 /** subArrayKill {{{
  * @brief Kill array with all elements
  * @param[in]  a      A #SubArray
  * @param[in]  clean  Pass clean to elements
  **/

void
subArrayKill(SubArray *a,
  int clean)
{
  assert(a);

  subArrayClear(a, clean);

  free(a);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
