
 /**
  * @package subtle
  *
  * @file Array functions
  * @copyright 2005-2008 Christoph Kappel <unexist@dorfelite.net>
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
  return (SubArray *)subUtilAlloc(1, sizeof(SubArray));
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
      a->data = (void **)subUtilRealloc(a->data, (a->ndata + 1) * sizeof(void *));
      a->data[(a->ndata)++] = e;
    }
} /* }}} */

 /** subArrayPop {{{
  * @brief Pop element from array
  * @param[in]  a  A #SubArray
  * @param[in]  e  Array element
  **/

void
subArrayPop(SubArray *a,
  void *e)
{
  int idx, i;

  assert(a && e);

  idx = subArrayIndex(a, e);
  if(idx >= 0)
    {
      for(i = idx; i < a->ndata - 1; i++) 
        a->data[i] = a->data[i + 1];

      a->ndata--;
      a->data = (void **)subUtilRealloc(a->data, a->ndata * sizeof(void *));
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

 /** subArrayFind {{{
  * @brief Find element
  * @param[in]   a     A #SubArray
  * @param[in]   name  Name of element
  * @param[out]  id    Array id
  * @return Returns an element or \p NULL
  **/

void *
subArrayFind(SubArray *a,
  char *name,
  int *id)
{
  int i;
  SubClient *c = NULL;

  assert(a);

  /* @todo Linear search.. */
  for(i = 0; i < a->ndata; i++)
    {
      c = CLIENT(a->data[i]);

      if(!strncmp(c->name, name, strlen(c->name)))
        {
          if(id) *id = i;
          return c;
        }
    }
  
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

  for(i = 0; i < a->ndata; i++) if(a->data[i] == e) return i;
  return -1;
} /* }}} */

 /** subArraySplice {{{
  * @brief Splice array at idx with len
  * @param[in]  a     A #SubArray
  * @param[in]  idx   Array index
  * @param[in]  len   Length
  **/

void
subArraySplice(SubArray *a,
  int idx,
  int len)
{
  int i;
  assert(a && idx >= 0 && idx <= a->ndata && len > 0);

  a->ndata += len;
  a->data = (void **)subUtilRealloc(a->data, (a->ndata + 1) * sizeof(void *));

  for(i = a->ndata; i > idx; i--)
    a->data[i] = a->data[i - len];
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
  assert(a && a->ndata > 0 && compar);

  qsort(a->data, a->ndata, sizeof(void *), compar);
} /* }}} */

 /** subArrayKill {{{
  * @brief Kill array with all elements
  * @param[in]  a      A #SubArray
  * @param[in]  clean  Free elements or not
  **/

void
subArrayKill(SubArray *a,
  int clean)
{
  int i;

  assert(a);

  for(i = 0; clean && i < a->ndata; i++)
    {
      /* Check type and kill */
      SubClient *c = CLIENT(a->data[i]);

      /* Common types first */
      if(c->flags & SUB_TYPE_CLIENT)      subClientKill(CLIENT(c));
      else if(c->flags & SUB_TYPE_TAG)    subTagKill(TAG(c));
      else if(c->flags & SUB_TYPE_VIEW)   subViewKill(VIEW(c));
      else if(c->flags & SUB_TYPE_LAYOUT) subLayoutKill(LAYOUT(c));
      else if(c->flags & SUB_TYPE_GRAB)   subGrabKill(GRAB(c));
      else if(c->flags & SUB_TYPE_SUBLET) subSubletKill(SUBLET(c));
      else if(c->flags & SUB_TYPE_TRAY)   subTrayKill(TRAY(c));
      else free(a->data[i]); 
    }

  if(a->data) free(a->data);
  free(a);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
