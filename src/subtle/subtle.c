
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
#include "subtle.h"

SubSubtle *subtle = NULL;

/* SubtleUsage {{{ */
static void
SubtleUsage(void)
{
  printf("Usage: %s [OPTIONS]\n\n" \
         "Options:\n" \
         "  -c, --config=FILE       Load config\n" \
         "  -d, --display=DISPLAY   Connect to DISPLAY\n" \
         "  -h, --help              Show this help and exit\n" \
         "  -k, --check             Check config syntax\n" \
         "  -s, --sublets=DIR       Load sublets from DIR\n" \
         "  -v, --version           Show version info and exit\n" \
         "  -D, --debug             Print debugging messages\n" \
         "Please report bugs to <%s>\n", 
         PKG_NAME, PKG_BUGREPORT);
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

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  int c, check = 0;
  char *display = NULL;
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


  subtle = SUBTLE(subSharedMemoryAlloc(1, sizeof(SubSubtle)));
  subtle->flags |= SUB_SUBTLE_RUN;

  while(-1 != (c = getopt_long(argc, argv, "c:d:hks:vD", long_options, NULL)))
    {
      switch(c)
        {
          case 'c': subtle->paths.config = optarg;     break;
          case 'd': display = optarg;                  break;
          case 'h': SubtleUsage();                     return 0;
          case 'k': check = 1;                         break;
          case 's': subtle->paths.sublets = optarg;    break;
          case 'v': SubtleVersion();                   return 0;
#ifdef DEBUG          
          case 'D': subtle->flags |= SUB_SUBTLE_DEBUG; break;
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

  if(check) ///< Load and check config
    {
      subRubyInit();
      subRubyLoadConfig();
      subRubyFinish();

      printf("Config OK\n");

      free(subtle);

      return 0;
    }

  /* Alloc */
  subtle->clients   = subArrayNew();
  subtle->grabs     = subArrayNew();
  subtle->gravities = subArrayNew();
  subtle->icons     = subArrayNew();
  subtle->screens   = subArrayNew();
  subtle->sublets   = subArrayNew();
  subtle->tags      = subArrayNew();
  subtle->trays     = subArrayNew();
  subtle->views     = subArrayNew();

  /* Init */
  SubtleVersion();
  subDisplayInit(display);
  subRubyInit();
  subEwmhInit();
  subGrabInit();
  subGravityInit();

  /* Load */
  subRubyLoadConfig();
  subRubyLoadSublets();

  /* Display */
  subDisplayConfigure();
  subDisplayScan();

  subEventLoop();

  subEventFinish();

  return 0;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
