
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
	* @return Success: A #SubArray
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
	* @param[in] elem New element
	**/

void
subArrayPush(SubArray *a,
	void *elem)
{
	assert(a && elem);

	a->data = (void **)subUtilRealloc(a->data, a->ndata + 1 * sizeof(void *));
	a->data[(a->ndata)++] = elem;
} /* }}} */

 /** subArrayPop {{{
	* Pop element from array
	* @param[in] a #SubArray
	* @param[in] elem Array element
	**/

void
subArrayPop(SubArray *a,
	void *elem)
{
	int i, j;

	assert(a && elem);

	for(i = 0; i < a->ndata; i--)
	{
		if(a->data[i] == elem)
			{
				for(j = i; j < a->ndata; j++) a->data[j] = a->data[j + 1];
				a->ndata--;
				a->data = (void **)subUtilRealloc(a->data, a->ndata * sizeof(void *));
				return;
			}
		}

} /* }}} */

/** subArrayFind {{{
 * Find array id of element
 * @param[in] a A #SubArray
 * @param[in] elem Element
 * @return Success: Found id
 * 				 Failure: -1
 **/

int
subArrayFind(SubArray *a,
	void *elem)
{
	int i;

	assert(a && elem);

	for(i = 0; i < a->ndata; i++)
		if(a->data[i] == elem) return(i);
	return(-1);
} /* }}} */

 /** subArraySwap {{{
	* Swap elements a and b
	* @param[in] arr A #SubArray
	* @param[in] elem1 Element 1
	* @param[in] elem2 Element 2
	**/

void
subArraySwap(SubArray *a,
	void *elem1,
	void *elem2)
{
	int i;

	assert(a && a->ndata >= 1);

	for(i = 0; i < a->ndata; i++) if(a->data[i] == elem1) a->data[i] = elem2;
} /* }}} */

 /** subArrayKill {{{
	* Kill array and free it's data
	* @param[in] a A #SubArray
	**/

void
subArrayKill(SubArray *a)
{
	int i;

	assert(a);

	for(i = 0; i < a->ndata; i++)
		free(a->data[i]);
	free(a);
} /* }}} */
