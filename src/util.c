
 /**
	* @package subtle
	*
	* @file Utility functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include <stdarg.h>
#include <signal.h>
#include "subtle.h"

#ifdef DEBUG
static int debug = 0;

 /** subUtilLogSetDebug {{{
	* @brief Enable debugging messages
	**/

void
subUtilLogSetDebug(void)
{
	debug = !debug;
} /* }}} */
#endif /* DEBUG */

 /** subUtilLog {{{
	* @brief Print messages depending on type
	* @param[in] type		Message type
	* @param[in] file		File name
	* @param[in] line		Line number
	* @param[in] format Message format
	* @param[in] ...		Variadic arguments
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
			case 0: fprintf(stderr, "<DEBUG> %s:%d: %s", file, line, buf);	break;
#endif /* DEBUG */
			case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGTERM);			break;
			case 2: fprintf(stdout, "<WARNING> %s", buf);										break;
		}
} /* }}} */

 /** subUtilAlloc {{{
	* @brief Alloc memory and check result
	* @param[in] n		Number of elements
	* @param[in] size Size of the memory block
	* @return Returns new memory block or \p NULL
	**/

void *
subUtilAlloc(size_t n,
	size_t size)
{
	void *mem = calloc(n, size);
	if(!mem) subUtilLogError("Can't alloc memory. Exhausted?\n");
	return(mem);
} /* }}} */

 /** subUtilRealloc {{{
	* @brief Realloc memory and check result
	* @param[in] mem	Memory block
	* @param[in] size Size of the memory block
	* @return Returns new memory block or \p NULL
	**/

void *
subUtilRealloc(void *mem,
	size_t size)
{
	mem = realloc(mem, size);
	if(!mem) subUtilLogDebug("Memory has been freed. Expected?\n");
	return(mem);
} /* }}} */

 /** subUtilFind {{{
	* @brief Find data with the context manager
	* @param[in] win	Window
	* @param[in] id		Context id
	* @return Returns found data pointer or \p NULL
	**/

XPointer *
subUtilFind(Window win,
	XContext id)
{
	XPointer *data = NULL;

	assert(win && id);
	return(XFindContext(d->disp, win, id, (XPointer *)&data) != XCNOENT ? data : NULL);
} /* }}} */

 /** subUtilTime {{{
	* @brief Get the current time in seconds 
	* @return Returns current time in seconds
	**/

time_t
subUtilTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return(tv.tv_sec);
} /* }}} */
