
 /**
  * @package subtle
  *
  * @file Main functions
  * @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <getopt.h>
#include <lua.h>
#include <sys/wait.h>
#include "subtle.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

static char *config = NULL;
SubSubtle *subtle = NULL;

/* Usage {{{ */
static void
Usage(void)
{
  printf("Usage: %s [OPTIONS]\n\n" \
          "Options:\n" \
          "  -c, --configdir=DIR   \t Look for config in DIR (default: ~/.%s/\n" \
          "  -d, --display=DISPLAY \t Connect to DISPLAY (default: $DISPLAY)\n" \
          "  -h, --help            \t Show this help and exit\n\n" \
          "  -s, --subletdir=DIR   \t Look for sublets in DIR (default: ~/.%s/sublets" \
          "  -v, --version         \t Show version info and exit\n" \
          "  -D, --debug           \t Print debugging messages\n" \
          "Please report bugs to <%s>\n", 
          PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT);
} /* }}} */

/* Version {{{ */
static void
Version(void)
{
  printf("%s %s - Copyright (c) 2005-2008 Christoph Kappel\n" \
          "Released under the GNU General Public License\n" \
          "Compiled for X%d and %s\n", PACKAGE_NAME, PACKAGE_VERSION,
          X_PROTOCOL, LUA_VERSION);
} /* }}} */

/* Signal {{{ */
static void
Signal(int signum)
{
#ifdef HAVE_EXECINFO_H
  void *array[10];
  size_t size;
#endif /* HAVE_EXECINFO_H */

  switch(signum)
    {
      case SIGHUP:
        printf("Reloading config..\n");
        subLuaLoadConfig(config);
        break;
      case SIGTERM:
      case SIGINT: 
        subArrayKill(subtle->tags, True);
        subArrayKill(subtle->views, True);
        subArrayKill(subtle->clients, False);
        subArrayKill(subtle->sublets, True);
        subArrayKill(subtle->keys, True);

        subLuaKill();
        subDisplayKill();

        free(subtle);
        exit(1);
      case SIGSEGV: 
#ifdef HAVE_EXECINFO_H
        size = backtrace(array, 10);

        printf("Last %zd stack frames:\n", size);
        backtrace_symbols_fd(array, size, 0);
#endif /* HAVE_EXECINFO_H */

        printf("Please report this bug to <%s>\n", PACKAGE_BUGREPORT);
        abort();
      case SIGCHLD:
        wait(NULL);
        break;
    }
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  int c;
  char *sublets = NULL, *display = NULL;
  struct sigaction act;
  static struct option long_options[] =
  {
    { "configdir",  required_argument,    0,  'c' },
    { "display",    required_argument,    0,  'd' },
    { "help",       no_argument,          0,  'h' },
    { "subletdir",  required_argument,    0,  's' },
    { "version",    no_argument,          0,  'v' },
#ifdef DEBUG
    { "debug",      no_argument,          0,  'D' },
#endif /* DEBUG */
    { 0, 0, 0, 0}
  };

  while((c = getopt_long(argc, argv, "c:d:hs:vD", long_options, NULL)) != -1)
    {
      switch(c)
        {
          case 'c': config = optarg;       break;
          case 'd': display = optarg;      break;
          case 'h': Usage();               return(0);
          case 's': sublets = optarg;      break;
          case 'v': Version();             return(0);
#ifdef DEBUG          
          case 'D': subUtilLogSetDebug();  break;
#endif /* DEBUG */
          case '?':
            printf("Try `%s --help for more information\n", PACKAGE_NAME);
            return(-1);
        }
    }

  Version();
  act.sa_handler  = Signal;
  act.sa_flags    = 0;
  memset(&act.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigaction(SIGHUP, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGSEGV, &act, NULL);
  sigaction(SIGCHLD, &act, NULL);

  subtle = (SubSubtle *)subUtilAlloc(1, sizeof(SubSubtle));

  subDisplayNew(display);

  subEwmhInit();
  subKeyInit();

  subLuaLoadConfig(config);
  subLuaLoadSublets(sublets);

  subDisplayScan();
  subEventLoop();

  raise(SIGTERM);
  
  return(0);
} /* }}} */
