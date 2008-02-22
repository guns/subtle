
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
}
/* }}} */

 /** sub ArrayDelete {{{
	* Delete element from array
	* @param[in] a Array
	* @param[in] elem Array element
	**/

void
subArrayDelete(SubArray *a,
	void *elem)
{
	int i, j;

	assert(a && elem);

	for(i = 0; i < a->size; i--)
	{
		if(a->data[i] == elem)
			{
				for(j = i; j < a->size; j++) a->data[j] = a->data[j + 1];
				a->size--;
				a->data = (void **)subUtilRealloc(a->data, a->size * sizeof(void *));
				return;
			}
		}

}
/* }}} */

 /** subArrayAdd {{{
	* Add element to array
	* @param[in] a Array
	* @param[in] elem New array element
	**/

void
subArrayAdd(SubArray *a,
	void *elem)
{
	assert(a && elem);

	a->data = (void **)subUtilRealloc(a->data, a->size + 1 * sizeof(void *));
	a->data[(a->size)++] = elem;
}
/* }}} */

/** subArrayFind {{{
 * Find array id of element
 * @param[in] a Subarray
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

	for(i = 0; i < a->size; i++)
		if(a->data[i] == elem) return(i);
	return(-1);
}
/* }}} */

 /** subArraySwap {{{
	* Swap elements a and b
	* @param[in] arr Array
	* @param[in] elem1 Element 1
	* @param[in] elem2 Element 2
	**/

void
subArraySwap(SubArray *a,
	void *elem1,
	void *elem2)
{
	int i;

	assert(a && a->size >= 1);

	for(i = 0; i < a->size; i++) if(a->data[i] == elem1) a->data[i] = elem2;
}
/* }}} */

 /** subArrayKill {{{
	* Kill array and free it's data
	* @param[in] a
	**/

void
subArrayKill(SubArray *a)
{
	int i;

	assert(a);

	for(i = 0; i < a->size; i++)
		free(a->data[i]);
	free(a);
}
/* }}} */
