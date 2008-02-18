
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Id$
	**/

#include <stdarg.h>
#include <signal.h>
#include "subtle.h"

#ifdef DEBUG
static int debug = 0;

 /** subUtilLogDebug {{{
	* Toggle debugging messages
	**/

void
subUtilLogSetDebug(void)
{
	debug = !debug;
}
/* }}} */
#endif /* DEBUG */

 /** subUtilLog {{{
	* Print messages depending on type
	* @param[in] type Message type
	* @param[in] file File name
	* @param[in] line Line number
	* @param[in] format Message format
	* @param[in] ... Variadic arguments
	**/

void
subUtilLog(int type,
	const char *file,
	int line,
	const char *format,
	...)
{
	va_list ap;
	char buf[255];

#ifdef DEBUG
	if(!debug && !type) return;
#endif /* DEBUG */

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	switch(type)
		{
#ifdef DEBUG
			case 0: fprintf(stderr, "<DEBUG:%s:%d> %s", file, line, buf);	break;
#endif /* DEBUG */
			case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGTERM);		break;
			case 2: fprintf(stdout, "<WARNING> %s", buf);									break;
		}
}
/* }}} */

 /** subUtilAlloc {{{
	* Alloc memory and check for result
	* @param[in] n Number of elements
	* @param[in] size Size of the memory block
	* @return Success: Allocated memory block
	* 				Failure: NULL
	**/

void *
subUtilAlloc(size_t n,
	size_t size)
{
	void *mem = calloc(n, size);
	if(!mem) subUtilLogError("Can't alloc memory. Exhausted?\n");
	return(mem);
}
/* }}} */

 /** subUtilRealloc {{{
	* Realloc memory and check for result
	* @param[in] mem Memory block
	* @param[in] size Size of the memory block
	*	@return Success: Allocated memory block
	* 				Failure: NULL
	**/

void *
subUtilRealloc(void *mem,
	size_t size)
{
	assert(mem);

	mem = realloc(mem, size);
	if(!mem) subUtilLogError("Can't alloc memory. Exhausted?\n");
	return(mem);
}
/* }}} */

 /** subUtilFind {{{
	* Find data with the context manager
	* @param[in] win A #Window
	* @param[in] id Context id
	* @return Success: Found data
	* 				Failure: NULL
	**/

XPointer *
subUtilFind(Window win,
	XContext id)
{
	XPointer *data = NULL;

	assert(win && id);
	return(XFindContext(d->disp, win, id, (XPointer *)&data) != XCNOENT ? data : NULL);
}
/* }}} */

 /** subUtilArrayAppend {{{
	* Append data to array
	* @param[out] arr Array
	* @param[in] narr Array size
	* @param[in] elem New array element
	**/

void
subUtilArrayAppend(void **arr,
	int narr,
	void *elem)
{
	assert(elem);

	arr = (void **)subUtilRealloc(arr, narr + 1 * sizeof(void *));
	arr[narr++] = elem;
}
/* }}} */

 /** subUtilArraySwap {{{
	* Swap elements a and b
	* @param[in] arr Array
	* @param[in] narr Array size
	* @param[in] elem1 Element 1
	* @param[in] elem2 Element 2
	**/

void
subUtilArraySwap(void *arr,
	int narr,
	void *elem1,
	void *elem2)
{
	int i;

	assert(arr && narr >= 1);

	for(i = 0; i < narr; i++) if(arr[i] == elem1) arr[i] = elem2;
}
/* }}} */


