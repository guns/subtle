
 /**
  * @package subtle
  *
  * @file subtle program
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <getopt.h>
#include <sys/wait.h>
#include <signal.h>
#include "subtle.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

static char *config = NULL;
SubSubtle *subtle = NULL;

/* SubtleUsage {{{ */
static void
SubtleUsage(void)
{
  printf("Usage: %s [OPTIONS]\n\n" \
         "Options:\n" \
         "  -c, --config=FILE       Load config (default: %s/%s/%s)\n" \
         "  -d, --display=DISPLAY   Connect to DISPLAY (default: %s)\n" \
         "  -h, --help              Show this help and exit\n" \
         "  -k, --check             Check config syntax\n" \
         "  -s, --sublets=DIR       Load sublets from DIR (default: %s/%s/sublets)\n" \
         "  -v, --version           Show version info and exit\n" \
         "  -D, --debug             Print debugging messages\n" \
         "Please report bugs to <%s>\n", 
         PKG_NAME, getenv("XDG_CONFIG_HOME"), PKG_NAME, PKG_CONFIG, getenv("DISPLAY"), 
         getenv("XDG_DATA_HOME"), PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* SubtleVersion {{{ */
static void
SubtleVersion(void)
{
  printf("%s %s - Copyright (c) 2005-2009 Christoph Kappel\n" \
         "Released under the GNU General Public License\n" \
         "Compiled for X%dR%d and Ruby %s\n", 
         PKG_NAME, PKG_VERSION, X_PROTOCOL, X_PROTOCOL_REVISION, RUBY_VERSION);
} /* }}} */

/* SubtleSignal {{{ */
static void
SubtleSignal(int signum)
{
#ifdef HAVE_EXECINFO_H
  void *array[10];
  size_t size;
#endif /* HAVE_EXECINFO_H */

  switch(signum)
    {
      case SIGHUP: ///< Reload config
        subArrayClear(subtle->grabs, True);
        subArrayClear(subtle->tags,  True);
        subArrayClear(subtle->views, True); 

        subRubyLoadConfig(config);
        subDisplayConfigure();

        subViewConfigure(subtle->cv);
        subViewRender();
        subSubletRender();

        printf("Reloaded config\n");
        break;
      case SIGTERM:
      case SIGINT: ///< Tidy up
        subArrayKill(subtle->clients, True);
        subArrayKill(subtle->grabs,   True);
        subArrayKill(subtle->sublets, True);
        subArrayKill(subtle->tags,    True);
        subArrayKill(subtle->trays,   True);
        subArrayKill(subtle->views,   True);

        subDisplayFinish();
        subRubyFinish();

        free(subtle);
       
        exit(0);
        break;
      case SIGSEGV: 
#ifdef HAVE_EXECINFO_H
        size = backtrace(array, 10);

        printf("Last %zd stack frames:\n", size);
        backtrace_symbols_fd(array, size, 0);
#endif /* HAVE_EXECINFO_H */

        printf("Please report this bug to <%s>\n", PKG_BUGREPORT);
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
  int c, check = 0;
  char *sublets = NULL, *display = NULL;
  struct sigaction act;
  const struct option long_options[] =
  {
    { "config",  required_argument, 0, 'c' },
    { "display", required_argument, 0, 'd' },
    { "help",    no_argument,       0, 'h' },
    { "check",   no_argument,       0, 'k' },
    { "sublets", required_argument, 0, 's' },
    { "version", no_argument,       0, 'v' },
#ifdef DEBUG
    { "debug",   no_argument,       0, 'D' },
#endif /* DEBUG */
    { 0, 0, 0, 0}
  };

#ifdef DEBUG
  int debug = 0;
#endif /* DEBUG */  

  while(-1 != (c = getopt_long(argc, argv, "c:d:hks:vD", long_options, NULL)))
    {
      switch(c)
        {
          case 'c': config  = optarg; break;
          case 'd': display = optarg; break;
          case 'h': SubtleUsage();    return 0;
          case 'k': check = 1;        break;
          case 's': sublets = optarg; break;
          case 'v': SubtleVersion();  return 0;
#ifdef DEBUG          
          case 'D': debug = 1;        break;
#else /* DEBUG */
          case 'D': 
            printf("Please recompile %s with `debug=yes'\n", PKG_NAME); 
            return 0;
#endif /* DEBUG */
          case '?':
            printf("Try `%s --help' for more information\n", PKG_NAME);
            return -1;
        }
    }

  act.sa_handler = SubtleSignal;
  act.sa_flags   = 0;
  memset(&act.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigaction(SIGHUP,  &act, NULL);
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT,  &act, NULL);
  sigaction(SIGSEGV, &act, NULL);
  sigaction(SIGCHLD, &act, NULL);

  subtle = SUBTLE(subSharedMemoryAlloc(1, sizeof(SubSubtle)));
  if(check) ///< Load and check config
    {
      subRubyInit();
      subRubyLoadConfig(config);
      subRubyFinish();

      free(subtle);

      printf("Config OK\n");

      return 0;
    }

  /* Alloc */
  subtle->clients = subArrayNew();
  subtle->grabs   = subArrayNew();
  subtle->sublets = subArrayNew();
  subtle->tags    = subArrayNew();
  subtle->trays   = subArrayNew();
  subtle->views   = subArrayNew();

#ifdef DEBUG
  subtle->debug = debug;
#endif /* DEBUG */

  /* Init */
  SubtleVersion();
  subDisplayInit(display);
  subRubyInit();
  subEwmhInit();
  subGrabInit();
  subTagInit();

  /* Load */
  subRubyLoadConfig(config);
  subRubyLoadSublets(sublets);

  /* Display */
  subDisplayConfigure();
  subDisplayPublish();
  subDisplayScan();

  subEventLoop();
  
  return 0; ///< Make compiler happy
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
