#include <stdarg.h>
#include <signal.h>

#include "subtle.h"

#ifdef DEBUG
static short debug = 0;

 /**
	* Toggle debugging messages.
	**/

void
subLogToggleDebug(void)
{
	debug = !debug;
}
#endif /* DEBUG */

 /**
	* Print messages depending on their type.
	* @param type Message type
	* @param file File name
	* @param line Line number
	* @param format Message format
	* @param ... Variadic arguments
	**/

void
subLog(short type,
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
			case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGINT);		break;
			case 2: fprintf(stdout, "<WARNING> %s", buf);									break;
			case 3: fprintf(stdout, "%s", buf);														break;
		}
}
