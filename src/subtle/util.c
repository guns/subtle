
 /**
  * @package subtle
  *
  * @file Utility functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <stdarg.h>
#include <signal.h>
#include <sys/time.h>
#include "subtle.h"

 /** subUtilLog {{{
  * @brief Print messages depending on type
  * @param[in]  type    Message type
  * @param[in]  file    File name
  * @param[in]  line    Line number
  * @param[in]  format  Message format
  * @param[in]  ...     Variadic arguments
  **/

void
subUtilLog(int type,
  const char *file,
  int line,
  const char *format,
  ...)
{
  va_list ap;
  char buf[255];

#ifdef DEBUG
  if(!subtle->debug && !type) return;
#endif /* DEBUG */

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  switch(type)
    {
#ifdef DEBUG
      case 0: fprintf(stderr, "<DEBUG> %s:%d: %s", file, line, buf);  break;
#endif /* DEBUG */
      case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGTERM);     break;
      case 2: fprintf(stdout, "<WARNING> %s", buf);                   break;
    }
} /* }}} */

 /** subUtilLogXError {{{
  * @brief Print X error messages
  * @params[in]  display  Display
  * @params[in]  ev       #XErrorEvent
  * @retval 0 Default return value
  **/

int
subUtilLogXError(Display *disp,
  XErrorEvent *ev)
{
#ifdef DEBUG
  if(subtle->debug) return 0;
#endif /* DEBUG */  

  if(BadAccess == ev->error_code && DefaultRootWindow(disp) == ev->resourceid)
    {
      subUtilLogError("Seems there is another WM running. Exiting.\n");
    }
  else if(42 != ev->request_code) /* X_SetInputFocus */
    {
      char error[255];
      XGetErrorText(disp, ev->error_code, error, sizeof(error));
      subUtilLogDebug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
    }
  return 0; 
} /* }}} */

 /** subUtilAlloc {{{
  * @brief Alloc memory and check result
  * @param[in]  n     Number of elements
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subUtilAlloc(size_t n,
  size_t size)
{
  void *mem = calloc(n, size);
  if(!mem) subUtilLogError("Can't alloc memory. Exhausted?\n");
  return mem;
} /* }}} */

 /** subUtilRealloc {{{
  * @brief Realloc memory and check result
  * @param[in]  mem   Memory block
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subUtilRealloc(void *mem,
  size_t size)
{
  mem = realloc(mem, size);
  if(!mem) subUtilLogDebug("Memory has been freed. Expected?\n");
  return mem;
} /* }}} */

 /** subUtilFind {{{
  * @brief Find data with the context manager
  * @param[in]  win  Window
  * @param[in]  id   Context id
  * @return Returns found data pointer or \p NULL
  **/

XPointer *
subUtilFind(Window win,
  XContext id)
{
  XPointer *data = NULL;

  assert(win && id);

  return XFindContext(subtle->disp, win, id, (XPointer *)&data) != XCNOENT ? data : NULL;
} /* }}} */

 /** subUtilTime {{{
  * @brief Get the current time in seconds 
  * @return Returns current time in seconds
  **/

time_t
subUtilTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return tv.tv_sec;
} /* }}} */

 /** subUtilRegexNew {{{ 
  * @brief Create new regex
  * @param[in]  regex  Regex 
  * @return Returns a #regex_t or \p NULL
  **/

regex_t *
subUtilRegexNew(char *regex)
{
  int errcode;
  regex_t *preg = NULL;

  assert(regex);
  
  preg = (regex_t *)subUtilAlloc(1, sizeof(regex_t));

  /* Thread safe error handling */
  if((errcode = regcomp(preg, regex, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
    {
      size_t errsize = regerror(errcode, preg, NULL, 0);
      char *errbuf   = (char *)subUtilAlloc(1, errsize);

      regerror(errcode, preg, errbuf, errsize);

      subUtilLogWarn("Can't compile preg `%s'\n", regex);
      subUtilLogDebug("%s\n", errbuf);

      free(errbuf);
      subUtilRegexKill(preg);

      return NULL;
    }
  return preg;
} /* }}} */

 /** subUtilRegexMatch {{{
  * @brief Check if string match preg
  * @param[in]  preg      A #regex_t
  * @param[in]  string    String
  * @retval  1  If string matches preg
  * @retval  0  If string doesn't match
  **/

int
subUtilRegexMatch(regex_t *preg,
  char *string)
{
  assert(preg);

  return !regexec(preg, string, 0, NULL, 0);
} /* }}} */

 /** subUtilRegexKill {{{
  * @brief Kill preg
  * @param[in]  preg  #regex_t
  **/

void
subUtilRegexKill(regex_t *preg)
{
  assert(preg);

  regfree(preg);
  free(preg);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
