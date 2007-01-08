#include <getopt.h>
#include <lua.h>
#include "subtle.h"

static void
Usage(void)
{
	printf("Usage: %s [OPTIONS]\n\n" \
					"Options:\n" \
					"  -c, --configdir \t Change config dir. (default: ~/.subtle/\n" \
					"  -d, --display   \t Display to connect. (default: $DISPLAY)\n" \
					"  -D, --debug     \t Print debugging messages.\n" \
					"  -v, --version   \t Display version and exit.\n" \
					"  -h, --help      \t Display this help and exit.\n\n" \
					"Please report bugs to <%s>\n", PACKAGE_NAME, PACKAGE_BUGREPORT);
}

static void
Version(void)
{
	printf("%s v%s - Copyright (c) 2005-2006 Christoph Kappel\n" \
					"Released under the GNU Public License\n" \
					"Compiled for X%d and %s\n", PACKAGE_NAME, PACKAGE_VERSION,
					X_PROTOCOL, LUA_VERSION);
}

static void
HandleSignal(int signum)
{
	switch(signum)
		{
			case SIGTERM:
			case SIGINT: 
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
	char *path = NULL, *display = NULL;
	struct sigaction act;
	static struct option long_options[] =
	{
		{ "configdir",	required_argument,		0,	'c' },
		{ "display",		required_argument,		0,	'd' },
#ifdef DEBUG
		{ "debug",			no_argument,					0,	'D' },
#endif
		{ "version",		no_argument,					0,	'v' },
		{ "help",				no_argument,					0,	'h' },
		{ 0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "c:d:Dvh", long_options, NULL)) != -1)
		{
			switch(c)
				{
					case 'c': path		= optarg; 		break;
					case 'd': display = optarg;			break;
#ifdef DEBUG					
					case 'D': subLogToggleDebug();	break;
#endif
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
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);

	subLuaNew();
	if(!subLuaLoad(path, "config.lua")) subLogError("Can't find configuration file.\n");
	subDisplayNew(display);
	subLuaKill();

	subEwmhNew();

	subScreenNew();
	subScreenAdd();

	subSubletNew();
	subLuaNew();
	if(!subLuaLoad(path, "dock.lua")) subLogWarn("Can't find dock file.\n");

	subEventLoop();

	raise(SIGINT);
	
	return(0);
}
