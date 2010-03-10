
 /**
  * @package subtle
  *
  * @file Main functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <getopt.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include "subtle.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

SubSubtle *subtle = NULL;

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
      case SIGHUP: subRubyReloadConfig(); break; ///< Reload config 
      case SIGINT:
        if(subtle) 
          {
            subtle->flags &= ~SUB_SUBTLE_RUN;
            XNoOp(subtle->dpy); ///< Pushing some data for poll
            XSync(subtle->dpy, True);
          }
        break;
      case SIGSEGV:
#ifdef HAVE_EXECINFO_H
        size = backtrace(array, 10);

        printf("Last %zd stack frames:\n", size);

#endif /* HAVE_EXECINFO_H */

        printf("Please report this bug to <%s>\n", PKG_BUGREPORT);
        abort();
      case SIGCHLD: wait(NULL); break;
    }
} /* }}} */

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
         "  -r, --replace           Replace current window manager\n" \
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
  printf("%s %s - Copyright (c) 2005-2010 Christoph Kappel\n" \
         "Released under the GNU General Public License\n" \
         "Compiled for X%dR%d and Ruby %s\n", 
         PKG_NAME, PKG_VERSION, X_PROTOCOL, X_PROTOCOL_REVISION, RUBY_VERSION);
} /* }}} */

 /** subSubtleFind {{{
  * @brief Find data with the context manager
  * @param[in]  win  A #Window
  * @param[in]  id   Context id
  * @return Returns found data pointer or \p NULL
  **/

XPointer *
subSubtleFind(Window win,
  XContext id)
{
  XPointer *data = NULL;

  return XCNOENT != XFindContext(subtle->dpy, win, id, (XPointer *)&data) ? data : NULL;
} /* }}} */

 /** subSubtleTime {{{
  * @brief Get the current time in seconds 
  * @return Returns time in seconds
  **/

time_t
subSubtleTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return tv.tv_sec;
} /* }}} */

 /** subSubtleFocus {{{
  * @brief Get pointer window and focus it
  * @param[in]  focus  Focus next client
  * @return New focus window
  **/

Window
subSubtleFocus(int focus)
{
  int dummy;
  Window win;
  SubClient *c = NULL;

  /* Focus */
  XQueryPointer(subtle->dpy, ROOT, (Window *)&dummy, &win,
    &dummy, &dummy, &dummy, &dummy, (unsigned int *)&dummy);
  
  /* Find next client */
  if((c = CLIENT(subSubtleFind(win, CLIENTID)))) 
    {
      subClientFocus(c);

      return c->win;
    }
  else if(focus)
    {
      int i;

      for(i = 0; i < subtle->clients->ndata; i++)
        {
          c = CLIENT(subtle->clients->data[i]);

          /* Check screen and visibility */
          if(c->screen == subtle->sid && VISIBLE(CURVIEW, c))
            {
              subClientWarp(c);
              subClientFocus(c);

              return c->win;
            }
        }
    }

  XSetInputFocus(subtle->dpy, ROOT, RevertToParent, CurrentTime); ///< Fallback

  return ROOT;
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  int c, check = 0;
  char *display = NULL;
  struct sigaction sa;
  const struct option long_options[] =
  {
    { "config",  required_argument, 0, 'c' },
    { "display", required_argument, 0, 'd' },
    { "help",    no_argument,       0, 'h' },
    { "check",   no_argument,       0, 'k' },
    { "replace", no_argument,       0, 'r' },
    { "sublets", required_argument, 0, 's' },
    { "version", no_argument,       0, 'v' },
#ifdef DEBUG
    { "debug",   no_argument,       0, 'D' },
#endif /* DEBUG */
    { 0, 0, 0, 0}
  };

  /* Create subtle */
  subtle = (SubSubtle *)(subSharedMemoryAlloc(1, sizeof(SubSubtle)));
  subtle->flags |= SUB_SUBTLE_RUN;

  /* Parse arguments */
  while(-1 != (c = getopt_long(argc, argv, "c:d:hkrs:vD", long_options, NULL)))
    {
      switch(c)
        {
          case 'c': subtle->paths.config = optarg;       break;
          case 'd': display = optarg;                    break;
          case 'h': SubtleUsage();                       return 0;
          case 'k': check = 1;                           break;
          case 'r': subtle->flags |= SUB_SUBTLE_REPLACE; break;
          case 's': subtle->paths.sublets = optarg;      break;
          case 'v': SubtleVersion();                     return 0;
#ifdef DEBUG          
          case 'D': 
            subtle->flags |= SUB_SUBTLE_DEBUG;
            subSharedDebug();
            break;
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
  
  /* Signal handler */
  sa.sa_handler = SubtleSignal;
  sa.sa_flags   = 0;
  memset(&sa.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigemptyset(&sa.sa_mask);

  sigaction(SIGHUP,  &sa, NULL);
  sigaction(SIGINT,  &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  /* Load and check config */
  if(check)
    {
      subRubyInit();
      subRubyLoadConfig();
      subRubyFinish();

      printf("Syntax OK\n");

      free(subtle);

      return 0;
    }

  /* Alloc arrays */
  subtle->clients   = subArrayNew();
  subtle->grabs     = subArrayNew();
  subtle->gravities = subArrayNew();
  subtle->hooks     = subArrayNew();
  subtle->panels    = subArrayNew();
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

  /* Load */
  subRubyLoadConfig();
  subRubyLoadSublets();
  subRubyLoadPanels();

  /* Display */
  subDisplayConfigure();
  subDisplayScan();

  subEventLoop();
  subEventFinish();

  return 0;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
