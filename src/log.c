#include <stdarg.h>
#include <signal.h>

#include "subtle.h"

#ifdef DEBUG
static short debug = 0;

 /**
	* Toggle either to show debugging messages or not.
	**/

void
subLogToggleDebug(void)
{
	debug = !debug;
}
#endif

 /**
	* Print messages dependant to their type.
	* @param type Message type
	* @param file File name with the call
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
	va_list vargs;
	char buf[255];

#ifdef DEBUG
	if(debug == 0 && type == 0)
		return;
#endif

	va_start(vargs, format);
	vsnprintf(buf, sizeof(buf), format, vargs);
	va_end(vargs);

	switch(type)
		{
#ifdef DEBUG
			case 0:
				fprintf(stderr, "<DEBUG:%s:%d> %s", file, line, buf);
				break;
#endif
			case 1: 
				fprintf(stderr, "<ERROR> %s", buf);
				raise(SIGINT);
				break;
			case 2: fprintf(stdout, "<WARNING> %s", buf);		break;
			case 3: fprintf(stdout, "%s", buf);							break;
		}
}
