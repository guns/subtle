
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License along
	* with this program; if not, write to the Free Software Foundation, Inc.,
	* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
subLogToggleDebug(void)
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
