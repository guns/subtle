
 /**
  * @package subtle
  *
  * @file Main functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
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
      case SIGCHLD: wait(NULL);                                    break;
      case SIGHUP:  if(subtle) subtle->flags |= SUB_SUBTLE_RELOAD; break;
      case SIGINT:  if(subtle) subtle->flags &= ~SUB_SUBTLE_RUN;   break;
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

            printf("\nPlease report this bug at %s\n", PKG_BUGREPORT);
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
         "  -l, --level             Set logging level\n" \
         "  -D, --debug             Print debugging messages\n" \
         "\nPlease report bugs at %s\n",
         PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* SubtleVersion {{{ */
static void
SubtleVersion(void)
{
  printf("%s %s - Copyright (c) 2005-2011 Christoph Kappel\n" \
         "Released under the GNU General Public License\n" \
         "Compiled for X%dR%d and Ruby %s\n",
         PKG_NAME, PKG_VERSION, X_PROTOCOL, X_PROTOCOL_REVISION, RUBY_VERSION);
} /* }}} */

#ifdef DEBUG
/* SubtleLevel {{{ */
static int
SubtleLevel(const char *str)
{
  int level = 0;
  char *tokens = NULL, *tok = NULL;

  tokens = strdup(str);
  tok    = strtok((char *)tokens, ",");

  /* Parse levels */
  while(tok)
    {
      if(0 == strncasecmp(tok, "warnings", 8))
        level |= SUB_LOG_WARN;
      else if(0 == strncasecmp(tok, "error", 5))
        level |= SUB_LOG_ERROR;
      else if(0 == strncasecmp(tok, "sublet", 6))
        level |= SUB_LOG_SUBLET;
      else if(0 == strncasecmp(tok, "depcrecated", 11))
        level |= SUB_LOG_DEPRECATED;
      else if(0 == strncasecmp(tok, "events", 6))
        level |= SUB_LOG_EVENTS;
      else if(0 == strncasecmp(tok, "ruby", 4))
        level |= SUB_LOG_RUBY;
      else if(0 == strncasecmp(tok, "xerror", 6))
        level |= SUB_LOG_XERROR;
      else if(0 == strncasecmp(tok, "subtlext", 8))
        level |= SUB_LOG_SUBTLEXT;
      else if(0 == strncasecmp(tok, "subtle", 6))
        level |= SUB_LOG_SUBTLE;
      else if(0 == strncasecmp(tok, "debug", 4))
        level |= SUB_LOG_DEBUG;

      tok = strtok(NULL, ",");
    }

  free(tokens);

  return level;
} /* }}} */
#endif /* DEBUG */

/* Public */

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
  int sid = 0;
  SubClient *c = NULL;
  SubScreen *s = NULL;

  /* Get current screen */
  s = subScreenCurrent(&sid);

  /* Find next window */
  if(focus)
    {
      int i;

      /* Check focus history */
      for(i = 1; i < HISTORYSIZE; i++)
        {
          if((c = CLIENT(subSubtleFind(subtle->windows.focus[i], CLIENTID))))
            {
              /* Check visibility on current screen */
              if(c->screen == sid && ALIVE(c) &&
                  VISIBLE(subtle->visible_tags, c) &&
                  c->win != subtle->windows.focus[0])
                {
                  subClientFocus(c);
                  subClientWarp(c, True);

                  return c->win;
                }
            }
        }

      /* Check client list */
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          c = CLIENT(subtle->clients->data[i]);

          /* Check visibility on current screen */
          if(c->screen == sid && ALIVE(c) &&
              VISIBLE(subtle->visible_tags, c) &&
              c->win != subtle->windows.focus[0])
            {
              subClientFocus(c);
              subClientWarp(c, True);

              return c->win;
            }
        }
    }

  /* Fallback to root */
  subtle->windows.focus[0] = ROOT;
  subGrabSet(ROOT);
  XSetInputFocus(subtle->dpy, ROOT, RevertToPointerRoot, CurrentTime);

  subScreenUpdate();
  subScreenRender();

  /* EWMH: Current destop */
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
    { "level",    required_argument, 0, 'l' },
    { "debug",    no_argument,       0, 'D' },
#endif /* DEBUG */
    { 0, 0, 0, 0}
  };

  /* Create subtle */
  subtle = (SubSubtle *)(subSharedMemoryAlloc(1, sizeof(SubSubtle)));
  subtle->flags |= (SUB_SUBTLE_XRANDR|SUB_SUBTLE_XINERAMA);

  /* Parse arguments */
  while(-1 != (c = getopt_long(argc, argv, "c:d:hknrs:vl:D", long_options, NULL)))
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
          case 'l':
            subSharedLogLevel(SubtleLevel(optarg));
            break;
          case 'D':
            subtle->flags |= SUB_SUBTLE_DEBUG;
            subSharedLogLevel(DEFAULT_LOGLEVEL|DEBUG_LOGLEVEL);
            break;
#else /* DEBUG */
          case 'l':
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
      int ret = 0;

      subRubyInit();

      if((ret = subRubyLoadConfig()))
        printf("Syntax OK\n");

      subRubyFinish();

      free(subtle);

      return !ret;
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
