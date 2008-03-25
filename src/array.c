
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Id$
	**/

#include "subtle.h"

 /** subArrayNew {{{
	* Create new array and init it
	* @return Success: #SubArray
	* 				Failure: NULL
	**/

SubArray *
subArrayNew(void)
{
	return((SubArray *)subUtilAlloc(1, sizeof(SubArray)));
} /* }}} */

 /** subArrayPush {{{
	* Push element to array
	* @param[in] a A #SubArray
	* @param[in] e New element
	**/

void
subArrayPush(SubArray *a,
	void *e)
{
	assert(a && e);

	a->data = (void **)subUtilRealloc(a->data, (a->ndata + 1) * sizeof(void *));
	a->data[(a->ndata)++] = e;
} /* }}} */

 /** subArrayPop {{{
	* Pop element from array
	* @param[in] a #SubArray
	* @param[in] e Array element
	**/

void
subArrayPop(SubArray *a,
	void *e)
{
	int i, j;

	assert(a && e);

	for(i = 0; i < a->ndata; i++)
		{
			if(a->data[i] == e)
				{
					for(j = i; j < a->ndata; j++) a->data[j] = a->data[j + 1];
					a->ndata--;
					a->data = (void **)subUtilRealloc(a->data, a->ndata * sizeof(void *));
					return;
				}
		}
} /* }}} */

 /** subArraySet {{{
	* Set array pos
	* @param[in] a A #SubArray
	* @param[in] idx Array index
	* @param[in] e Element
	**/
 
void
subArraySet(SubArray *a,
	int idx,
	void *e)
{
	int i;

	assert(a && idx >= 0 && idx <= a->ndata && e);

	a->ndata++;
	a->data = (void **)subUtilRealloc(a->data, a->ndata * sizeof(void *));

	for(i = idx; i < a->ndata; i++) a->data[i + 1] = a->data[i];
	a->data[idx] = e;
} /* }}} */

 /** subArrayFind {{{
	* Find array id of element
	* @param[in] a A #SubArray
	* @param[in] e Element
	* @return Success: Found id
	* 				 Failure: -1
	**/

int
subArrayFind(SubArray *a,
	void *e)
{
	int i;

	assert(a && e);

	for(i = 0; i < a->ndata; i++)
		if(a->data[i] == e) return(i);
	return(-1);
} /* }}} */

 /** subArraySwap {{{
	* Swap elements a and b
	* @param[in] arr A #SubArray
	* @param[in] e1 Element 1
	* @param[in] e2 Element 2
	**/

void
subArraySwap(SubArray *a,
	void *e1,
	void *e2)
{
	int i;

	assert(a && a->ndata >= 1 && e1 && e2);

	for(i = 0; i < a->ndata; i++) 
		if(a->data[i] == e1) a->data[i] = e2;
} /* }}} */

 /** subArrayKill {{{
	* Kill array with all elements
	* @param[in] a A #SubArray
	* @param[in] clean Free elements or not
	**/

void
subArrayKill(SubArray *a,
	int clean)
{
	int i;

	assert(a);

	if(clean)
		{
			for(i = 0; i < a->ndata; i++)
				{
						/* Check type and kill it */
						SubArrayUndef *u = (SubArrayUndef *)a->data[i];

						if(u->flags & SUB_TYPE_CLIENT) subClientKill((SubClient *)u);
						else if(u->flags & SUB_TYPE_TILE) subTileKill((SubTile *)u);
						else if(u->flags & SUB_TYPE_VIEW) subViewKill((SubView *)u);
						else if(u->flags & SUB_TYPE_RULE) subRuleKill((SubRule *)u);
						else if(u->flags & SUB_TYPE_SUBLET) subSubletKill((SubSublet *)u);
						else free(a->data[i]); 
				}
		}
	free(a);
} /* }}} */
