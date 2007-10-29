
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include <stdarg.h>
#include <signal.h>
#include "subtle.h"

#ifdef DEBUG
static short debug = 0;

 /**
	* Toggle debugging messages
	**/

void
subUtilLogToggle(void)
{
	debug = !debug;
}
#endif /* DEBUG */

 /**
	* Print messages depending on their type
	* @param type Message type
	* @param file File name
	* @param line Line number
	* @param format Message format
	* @param ... Variadic arguments
	**/

void
subUtilLog(short type,
	const char *file,
	short line,
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

 /**
	* Alloc memory and check for result
	* @param n Number of elements
	* @param size Size of the memory block
	* @return 
	**/

void *
subUtilAlloc(size_t n,
	size_t size)
{
	void *mem = calloc(n, size);
	if(!mem) subUtilLogError("Can't alloc memory. Exhausted?\n");
	return(mem);
}

 /**
	* Realloc memory and check for result
	* @param mem Memory block
	* @param size Size of the memory block
	* @return 
	**/

void *
subUtilRealloc(void *mem,
	size_t size)
{
	mem = realloc(mem, size);
	if(!mem) subUtilLogError("Can't alloc memory. Exhausted?\n");
	return(mem);
}

 /**
	* Find data with the context manager
	* @param win A #Window
	* @param id Context id
	* @return Return the data associated with the window or NULL
	**/

XPointer *
subUtilFind(Window win,
	XContext id)
{
	XPointer *data = NULL;

	assert(win && id);
	return(XFindContext(d->disp, win, id, (XPointer *)&data) != XCNOENT ? data : NULL);
}
