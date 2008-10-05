
 /**
  * @package subtle
  *
  * @file EWMH functions
  * @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

#define NATOMS 53
static Atom atoms[NATOMS];

 /** subEwmhInit {{{
  * @brief Init and register ICCCM/EWMH atoms
  **/

void
subEwmhInit(void)
{
  int n = NATOMS;
  char *names[] =
  {
    /* ICCCM */
    "WM_NAME", "WM_CLASS", "WM_STATE", "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "WM_NORMAL_HINTS", "WM_SIZE_HINTS",

    /* EWMH */
    "_NET_SUPPORTED", "_NET_CLIENT_LIST", "_NET_CLIENT_LIST_STACKING", "_NET_NUMBER_OF_DESKTOPS",
    "_NET_DESKTOP_NAMES", "_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_VIEWPORT", "_NET_CURRENT_DESKTOP", "_NET_ACTIVE_WINDOW",
    "_NET_WORKAREA", "_NET_SUPPORTING_WM_CHECK", "_NET_VIRTUAL_ROOTS", "_NET_CLOSE_WINDOW",
    "_NET_WM_NAME", "_NET_WM_PID", "_NET_WM_DESKTOP", 
    "_NET_WM_STATE", "_NET_WM_STATE_MODAL", "_NET_WM_STATE_SHADED", "_NET_WM_STATE_HIDDEN", "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP", "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_ALLOWED_ACTIONS", "_NET_WM_ACTION_MOVE", "_NET_WM_ACTION_RESIZE", "_NET_WM_ACTION_SHADE",
    "_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_CHANGE_DESKTOP", "_NET_WM_ACTION_CLOSE",

    /* Misc */
    "UTF8_STRING",

    /* subtle */
    "SUBTLE_CLIENT_TAG", "SUBTLE_CLIENT_UNTAG", "SUBTLE_CLIENT_TAGS",
    "SUBTLE_TAG_NEW", "SUBTLE_TAG_KILL", "SUBTLE_TAG_LIST",
    "SUBTLE_VIEW_NEW", "SUBTLE_VIEW_KILL", "SUBTLE_VIEW_LIST", "SUBTLE_VIEW_TAG", "SUBTLE_VIEW_UNTAG", "SUBTLE_VIEW_TAGS"
  };
  long data[4] = { 0, 0, 0, 0 }, pid = (long)getpid();

  XInternAtoms(subtle->disp, names, n, 0, atoms);

  /* EWMH: Window manager information */
  subEwmhSetString(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_WM_NAME, PKG_NAME);
  subEwmhSetCardinals(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_WM_PID, &pid, 1);
  subEwmhSetCardinals(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&data, 2);
  subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_SUPPORTING_WM_CHECK, 
    &DefaultRootWindow(subtle->disp), 1);

  /* EWMH: Workarea size */
  data[2] = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)); 
  data[3] = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));
  subEwmhSetCardinals(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_WORKAREA, (long *)&data, 4);

  /* EWMH: Desktop sizes */
  data[0] = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp));
  data[1] = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));
  subEwmhSetCardinals(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

  /* EWMH: Supported window states */
  data[0] = atoms[SUB_EWMH_NET_WM_STATE_MODAL];
  data[1] = atoms[SUB_EWMH_NET_WM_STATE_SHADED];
  data[2] = atoms[SUB_EWMH_NET_WM_STATE_HIDDEN];
  data[3] = atoms[SUB_EWMH_NET_WM_STATE_FULLSCREEN];
  subEwmhSetCardinals(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_SUPPORTED, (long *)&data, 4);  

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST, NULL, 0);
  subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST_STACKING, NULL, 0);
} /* }}} */

 /** subEwmhFind {{{
  * @brief Find intern atoms
  * @param[in]  hint  Hint number
  * @return Returns a #Atom
  **/

Atom
subEwmhFind(int hint)
{
  assert(hint <= NATOMS);
  return(atoms[hint]);
} /* }}} */

 /** subEwmhGetProperty {{{
  * @brief Get property from window
  * @param[in]   win   Window
  * @param[in]   type  Atom type
  * @param[in]   hint  Hint number
  * @param[out]  size  Size of items
  * @return Returns property data
  **/

char *
subEwmhGetProperty(Window win,
  Atom type,
  int hint,
  unsigned long *size)
{
  unsigned long nitems, bytes;
  unsigned char *data = NULL;
  int format;
  Atom rtype;

  if(XGetWindowProperty(subtle->disp, win, atoms[hint], 0L, 1024, False, type, &rtype, 
    &format, &nitems, &bytes, &data) != Success)
    {
      subUtilLogDebug("Failed to get property (%d)\n", hint);
      return(NULL);
    }
  if(type != rtype)
    {
      subUtilLogDebug("Invalid type for property (%d)\n", hint);
      XFree(data);
      return(NULL);
    }
  if(size) *size = (unsigned long)(format / 8) * nitems;

  return((char *)data);
} /* }}} */

 /** subEwmhSetWindows {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  hint    Hint number
  * @param[in]  values  Window list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetWindows(Window win,
  int hint,
  Window *values,
  int size)
{
  XChangeProperty(subtle->disp, win, atoms[hint], XA_WINDOW, 32, PropModeReplace, (unsigned char *)values, size);
} /* }}} */

 /** subEwmhSetCardinals {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  hint    Hint number
  * @param[in]  values  Cardinal list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetCardinals(Window win,
  int hint,
  long *values,
  int size)
{
  XChangeProperty(subtle->disp, win, atoms[hint], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)values, size);
} /* }}} */

 /** subEwmhSetString {{{
  * @brief Change window property
  * @param[in]  win    Window
  * @param[in]  hint   Hint number
  * @param[in]  value  String value
  **/

void
subEwmhSetString(Window win,
  int hint,
  char *value)
{
  XChangeProperty(subtle->disp, win, atoms[hint], atoms[SUB_EWMH_UTF8], 8, 
    PropModeReplace, (unsigned char *)value, strlen(value));
} /* }}} */

 /** subEwmhSetStrings {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  hint    Hint number
  * @param[in]  values  String list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetStrings(Window win,
  int hint,
  char **values,
  int size)
{
  int i, len = 0, pos = 0;
  char *str = NULL;

  for(i = 0; i < size; i++) len += strlen(values[i]);

  str = (char *)subUtilAlloc(len + i + 1, sizeof(char *));

  for(i = 0; i < size; i++)
    {
      strncpy(str + pos, values[i], strlen(values[i]));
      pos += strlen(values[i]);
      str[pos] = '\0';
      pos++;
    }

  XChangeProperty(subtle->disp, win, atoms[hint], atoms[SUB_EWMH_UTF8], 8, PropModeReplace, (unsigned char *)str, pos);
  free(str);
} /* }}} */
