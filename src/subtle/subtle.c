
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
#include <unistd.h>
#include "subtle.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

SubSubtle *subtle = NULL;

/* SubtleSignal {{{ */
static void
SubtleSignal(int signum)
{
  switch(signum)
    {
      case SIGCHLD: wait(NULL); break;
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
          {
#ifdef HAVE_EXECINFO_H
            int i, frames = 0;
            void *callstack[10] = { 0 };
            char **strings = NULL;

            frames  = backtrace(callstack, 10);
            strings = backtrace_symbols(callstack, frames);

            printf("\n\nLast %d stack frames:\n", frames);

            for(i = 0; i < frames; i++)
              printf("%s\n", strings[i]);

            free(strings);
#endif /* HAVE_EXECINFO_H */

            printf("\nPlease report this bug to <%s>\n", PKG_BUGREPORT);
            abort();
          }
        break;
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
         "  -n, --no-randr          Disable RandR extension (required for Twinview)\n" \
         "  -r, --replace           Replace current window manager\n" \
         "  -s, --sublets=DIR       Load sublets from DIR\n" \
         "  -v, --version           Show version info and exit\n" \
         "  -D, --debug             Print debugging messages\n" \
         "\nPlease report bugs at %s\n",
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
  int dummy = 0;
  Window win = None;
  SubClient *c = NULL;
  SubScreen *s = NULL;

  /* Get pointer window */
  XQueryPointer(subtle->dpy, ROOT, (Window *)&dummy, &win,
    &dummy, &dummy, &dummy, &dummy, (unsigned int *)&dummy);

  /* Find pointer window */
  if((c = CLIENT(subSubtleFind(win, CLIENTID))))
    {
      subClientFocus(c);

      return c->win;
    }
  else if(focus) ///< Find next window
    {
      int i, sid = 0;

      /* Get current screen */
      s = subScreenCurrent(&sid);

      for(i = 0; i < subtle->clients->ndata; i++)
        {
          c = CLIENT(subtle->clients->data[i]);

          /* Check visibility on current screen */
          if(c->screen == sid && VISIBLE(subtle->visible_tags, c))
            {
              subClientFocus(c);
              subClientWarp(c, True);

              return c->win;
            }
        }
    }

  /* Fallback to root */
  XSetInputFocus(subtle->dpy, ROOT, RevertToPointerRoot, CurrentTime);

  subScreenUpdate();
  subScreenRender();

  /* EWMH: Current destop */
  s = subScreenCurrent(NULL);

  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP,
    (long *)&s->vid, 1);

  return ROOT;
} /* }}} */

 /** subSubtleFinish {{{
  * @brief Finish subtle
  **/

void
subSubtleFinish(void)
{
  if(subtle)
    {
      if(subtle->dpy)
        XSync(subtle->dpy, False); ///< Sync before going on

      /* Hook: Exit */
      subHookCall(SUB_HOOK_EXIT, NULL);

      /* Clear hooks first to stop calling */
      subArrayClear(subtle->hooks, True);

      /* Kill arrays */
      subArrayKill(subtle->clients,   True);
      subArrayKill(subtle->grabs,     True);
      subArrayKill(subtle->gravities, True);
      subArrayKill(subtle->screens,   True);
      subArrayKill(subtle->sublets,   False);
      subArrayKill(subtle->tags,      True);
      subArrayKill(subtle->trays,     True);
      subArrayKill(subtle->views,     True);
      subArrayKill(subtle->hooks,     False);

      subEventFinish();
      subRubyFinish();
      subEwmhFinish();
      subDisplayFinish();

      if(subtle->separator.string) free(subtle->separator.string);

      free(subtle);
    }
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  int c, restart = 0;
  char *display = NULL;
  struct sigaction sa;
  const struct option long_options[] =
  {
    { "config",   required_argument, 0, 'c' },
    { "display",  required_argument, 0, 'd' },
    { "help",     no_argument,       0, 'h' },
    { "check",    no_argument,       0, 'k' },
    { "no-randr", no_argument,       0, 'n' },
    { "replace",  no_argument,       0, 'r' },
    { "sublets",  required_argument, 0, 's' },
    { "version",  no_argument,       0, 'v' },
#ifdef DEBUG
    { "debug",    no_argument,       0, 'D' },
#endif /* DEBUG */
    { 0, 0, 0, 0}
  };

  /* Create subtle */
  subtle = (SubSubtle *)(subSharedMemoryAlloc(1, sizeof(SubSubtle)));
  subtle->flags |= (SUB_SUBTLE_XRANDR|SUB_SUBTLE_XINERAMA);

  /* Parse arguments */
  while(-1 != (c = getopt_long(argc, argv, "c:d:hkrs:vD", long_options, NULL)))
    {
      switch(c)
        {
          case 'c': subtle->paths.config = optarg;        break;
          case 'd': display = optarg;                     break;
          case 'h': SubtleUsage();                        return 0;
          case 'k': subtle->flags |= SUB_SUBTLE_CHECK;    break;
          case 'n': subtle->flags &= ~SUB_SUBTLE_XRANDR;  break;
          case 'r': subtle->flags |= SUB_SUBTLE_REPLACE;  break;
          case 's': subtle->paths.sublets = optarg;       break;
          case 'v': SubtleVersion();                      return 0;
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

  /* Load and check config only */
  if(subtle->flags & SUB_SUBTLE_CHECK)
    {
      subRubyInit();

      if(subRubyLoadConfig())
        printf("Syntax OK\n");

      subRubyFinish();

      free(subtle);

      return 0;
    }

  /* Alloc arrays */
  subtle->clients   = subArrayNew();
  subtle->grabs     = subArrayNew();
  subtle->gravities = subArrayNew();
  subtle->hooks     = subArrayNew();
  subtle->screens   = subArrayNew();
  subtle->sublets   = subArrayNew();
  subtle->tags      = subArrayNew();
  subtle->trays     = subArrayNew();
  subtle->views     = subArrayNew();

  /* Init */
  SubtleVersion();
  subDisplayInit(display);
  subEwmhInit();
  subScreenInit();
  subRubyInit();
  subGrabInit();

  /* Load */
  subRubyLoadConfig();
  subRubyLoadSublets();
  subRubyLoadPanels();

  /* Display */
  subDisplayConfigure();
  subDisplayScan();

  subEventLoop();

  /* Save restart flag */
  if(subtle->flags & SUB_SUBTLE_RESTART) restart++;

  subSubtleFinish();

  /* Restart if necessary */
  if(restart)
    {
      printf("Restarting\n");

      execvp(argv[0], argv);
    }

  printf("Exit\n");

  return 0;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
