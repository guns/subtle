
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

#include <getopt.h>
#include <lua.h>
#include "subtle.h"

static char *config = NULL;

static void
Usage(void)
{
	printf("Usage: %s [OPTIONS]\n\n" \
					"Options:\n" \
					"  -c, --configdir DIR     \t Look for config in DIR (default: ~/.%s/\n" \
					"  -s, --subletdir DIR     \t Look for sublets in DIR (default: ~/.%s/sublets" \
					"  -d, --display   DISPLAY \t Connect to DISPLAY (default: $DISPLAY)\n" \
					"  -D, --debug             \t Print debugging messages\n" \
					"  -v, --version           \t Enable debugging output\n" \
					"  -h, --help              \t Show this help and exit\n\n" \
					"Please report bugs to <%s>\n", 
					PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT);
}

static void
Version(void)
{
	printf("%s v%s - Copyright (c) 2005-2007 Christoph Kappel\n" \
					"Released under the GNU General Public License\n" \
					"Compiled for X%d and %s\n", PACKAGE_NAME, PACKAGE_VERSION,
					X_PROTOCOL, LUA_VERSION);
}

static void
HandleSignal(int signum)
{
	switch(signum)
		{
			case SIGHUP:
				printf("Reloading config..\n");
				subLuaLoadConfig(config);
				break;
			case SIGTERM:
			case SIGINT: 
				subKeyKill();
				subScreenKill();
				subLuaKill();
				subDisplayKill();
				exit(1);
			case SIGSEGV: 
				printf("Please report this bug to <%s>\n", PACKAGE_BUGREPORT);
				abort();
		}
}

int
main(int argc,
	char *argv[])
{
	int c;
	char *sublets = NULL, *display = NULL;
	struct sigaction act;
	static struct option long_options[] =
	{
		{ "configdir",	required_argument,		0,	'c' },
		{ "subletdir",	required_argument,		0,	's' },
		{ "display",		required_argument,		0,	'd' },
#ifdef DEBUG
		{ "debug",			no_argument,					0,	'D' },
#endif /* DEBUG */
		{ "version",		no_argument,					0,	'v' },
		{ "help",				no_argument,					0,	'h' },
		{ 0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "c:s:d:Dvh", long_options, NULL)) != -1)
		{
			switch(c)
				{
					case 'c': config	= optarg; 		break;
					case 's': sublets	= optarg;			break;
					case 'd': display = optarg;			break;
#ifdef DEBUG					
					case 'D': subLogToggleDebug();	break;
#endif /* DEBUG */
					case 'v': Version(); 						return(0);
					case 'h': Usage(); 							return(0);
					case '?':
						printf("Try `%s --help for more information\n", PACKAGE_NAME);
						return(-1);
				}
		}

	Version();
	act.sa_handler	= HandleSignal;
	act.sa_flags		= 0;
	memset(&act.sa_mask, 0, sizeof(sigset_t)); /* Avoid uninitialized values */
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);

	subDisplayNew(display);
	subLuaLoadConfig(config);

	subEwmhInit();

	subScreenInit();
	subScreenNew();

	subSubletInit();
	subLuaLoadSublets(sublets);

	subEventLoop();

	raise(SIGTERM);
	
	return(0);
}
