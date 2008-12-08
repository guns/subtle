
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

#include <X11/Xatom.h>
#include "subtle.h"

static Atom atoms[SUB_EWMH_TOTAL];

 /** subEwmhInit {{{
  * @brief Init and register ICCCM/EWMH atoms
  **/

void
subEwmhInit(void)
{
  int len = 0;
  char *selection = NULL, *names[] =
  {
    /* ICCCM */
    "WM_NAME", "WM_CLASS", "WM_STATE", "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "WM_NORMAL_HINTS", "WM_SIZE_HINTS",

    /* EWMH */
    "_NET_SUPPORTED", "_NET_CLIENT_LIST", "_NET_CLIENT_LIST_STACKING", "_NET_NUMBER_OF_DESKTOPS",
    "_NET_DESKTOP_NAMES", "_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_VIEWPORT", "_NET_CURRENT_DESKTOP", "_NET_ACTIVE_WINDOW",
    "_NET_WORKAREA", "_NET_SUPPORTING_WM_CHECK", "_NET_VIRTUAL_ROOTS", "_NET_CLOSE_WINDOW",
    "_NET_WM_NAME", "_NET_WM_PID", "_NET_WM_DESKTOP", "_NET_SHOWING_DESKTOP",
    "_NET_WM_STATE", "_NET_WM_STATE_MODAL", "_NET_WM_STATE_HIDDEN", "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP", "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_ALLOWED_ACTIONS", "_NET_WM_ACTION_MOVE", "_NET_WM_ACTION_RESIZE",
    "_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_CHANGE_DESKTOP", "_NET_WM_ACTION_CLOSE",
    "_NET_SYSTEM_TRAY_OPCODE", "_NET_SYSTEM_TRAY_MESSAGE_DATA", "_NET_SYSTEM_TRAY_S",

    /* Misc */
    "UTF8_STRING", "MANAGER",

    /* XEmbed */
    "_XEMBED", "_XEMBED_INFO",

    /* subtle */
    "SUBTLE_CLIENT_TAG", "SUBTLE_CLIENT_UNTAG", "SUBTLE_CLIENT_TAGS",
    "SUBTLE_TAG_NEW", "SUBTLE_TAG_KILL", "SUBTLE_TAG_LIST",
    "SUBTLE_VIEW_NEW", "SUBTLE_VIEW_KILL", "SUBTLE_VIEW_LIST", "SUBTLE_VIEW_TAG", "SUBTLE_VIEW_UNTAG", "SUBTLE_VIEW_TAGS"
  };

  /* Tray selection */
  len       = strlen(names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION]) + 5; ///< @todo For high screen counts
  selection = (char *)subUtilAlloc(len, sizeof(char)); 

  snprintf(selection, len, "%s%u", names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION], SCREEN);
  printf("Selection: len=%d, name=%s\n", len, selection);
  names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION] = selection;

  XInternAtoms(subtle->disp, names, SUB_EWMH_TOTAL, 0, atoms);

  free(selection);
} /* }}} */

 /** subEwmhGet {{{
  * @brief Find intern atoms
  * @param[in]  e  A #SubEwmh
  * @return Returns the desired #Atom
  **/

Atom
subEwmhGet(SubEwmh e)
{
  assert(e <= SUB_EWMH_TOTAL);

  return atoms[e];
} /* }}} */

 /** subEwmhFind {{{
  * @brief Find id for intern atom
  * @param[in]  e  #SubEwmh
  * @retval  >=0  Found index
  * @retval  -1   Atom was not found
  **/

SubEwmh
subEwmhFind(Atom atom)
{
  int i;

  assert(atom);

  for(i = 0; i < SUB_EWMH_TOTAL; i++)
    if(atoms[i] == atom) return i;

  return -1;
} /* }}} */

 /** subEwmhGetProperty {{{
  * @brief Get property from window
  * @param[in]   win   Window
  * @param[in]   type  Atom type
  * @param[in]   e     A #SubEwmh
  * @param[out]  size  Size of items
  * @return Returns property data
  **/

char *
subEwmhGetProperty(Window win,
  Atom type,
  SubEwmh e,
  unsigned long *size)
{
  unsigned long nitems, bytes;
  unsigned char *data = NULL;
  int format;
  Atom rtype;

  if(XGetWindowProperty(subtle->disp, win, atoms[e], 0L, 1024, False, type, &rtype,
    &format, &nitems, &bytes, &data) != Success)
    {
      subUtilLogDebug("Failed to get property (%d)\n", e);
      return NULL;
    }
  if(type != rtype)
    {
      subUtilLogDebug("Invalid type for property (%d <=> %d)\n", type, rtype);
      XFree(data);
      return NULL;
    }
  if(size) *size = (unsigned long)(format / 8) * nitems;

  return (char *)data;
} /* }}} */

 /** subEwmhGetWMState {{{
  * @brief Get WM state from window
  * @param[in]  win  A window
  * @return Returns window WM state
  **/

long
subEwmhGetWMState(Window win)
{
  Atom type;
  int format;
  unsigned long unused, bytes;
  long *data = NULL, state = WithdrawnState;

  assert(win);

  if(Success == XGetWindowProperty(subtle->disp, win, atoms[SUB_EWMH_WM_STATE], 0L, 2L, False,
      atoms[SUB_EWMH_WM_STATE], &type, &format, &bytes, &unused, 
      (unsigned char **)&data) && bytes)
    {
      state = *data;
      XFree(data);
    }

  return state;
} /* }}} */

 /** subEwmhSetWindows {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  e       A #SubEwmh
  * @param[in]  values  Window list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetWindows(Window win,
  SubEwmh e,
  Window *values,
  int size)
{
  XChangeProperty(subtle->disp, win, atoms[e], XA_WINDOW, 32, PropModeReplace,
    (unsigned char *)values, size);
} /* }}} */

 /** subEwmhSetCardinals {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  e       A #SubEwmh
  * @param[in]  values  Cardinal list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetCardinals(Window win,
  SubEwmh e,
  long *values,
  int size)
{
  XChangeProperty(subtle->disp, win, atoms[e], XA_CARDINAL, 32, PropModeReplace,
    (unsigned char *)values, size);
} /* }}} */

 /** subEwmhSetString {{{
  * @brief Change window property
  * @param[in]  win    Window
  * @param[in]  e      A #SubEwmh
  * @param[in]  value  String value
  **/

void
subEwmhSetString(Window win,
  SubEwmh e,
  char *value)
{
  XChangeProperty(subtle->disp, win, atoms[e], atoms[SUB_EWMH_UTF8], 8,
    PropModeReplace, (unsigned char *)value, strlen(value));
} /* }}} */

 /** subEwmhSetStrings {{{
  * @brief Change window property
  * @param[in]  win     Window
  * @param[in]  e       A #SubEwmh
  * @param[in]  values  String list
  * @param[in]  size    Size of the list
  **/

void
subEwmhSetStrings(Window win,
  SubEwmh e,
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

  XChangeProperty(subtle->disp, win, atoms[e], atoms[SUB_EWMH_UTF8], 8,
    PropModeReplace, (unsigned char *)str, pos);
  free(str);
} /* }}} */

 /** subEwmhSetWMState {{{
  * @brief Set WM state for window
  * @param[in]  win    A window
  * @param[in]  state  New state for the window
  **/

void
subEwmhSetWMState(Window win,
  long state)
{
  CARD32 data[2];
  data[0] = state;
  data[1] = None; /* No icons */

  assert(win);

  XChangeProperty(subtle->disp, win, atoms[SUB_EWMH_WM_STATE], 
    atoms[SUB_EWMH_WM_STATE], 32, PropModeReplace, (unsigned char *)data, 2);
} /* }}} */

 /** subEwmhMessage {{{
  * @brief Send message
  * @param[in]  dst    Destination window
  * @param[in]  win    Target window
  * @param[in]  e      A #SubEwmh
  * @param[in]  data0  Data part 1
  * @param[in]  data1  Data part 2
  * @param[in]  data2  Data part 3
  * @param[in]  data3  Data part 4
  * @param[in]  data4  Data part 5
  * @retval  0   Error
  * @retval  >0  Success
  **/

int
subEwmhMessage(Window dst,
  Window win,
  SubEwmh e,
  long data0,
  long data1,
  long data2,
  long data3,
  long data4)
{
  XClientMessageEvent ev;

  /* Assemble message */
  ev.type         = ClientMessage;
  ev.message_type = atoms[e];
  ev.window       = win;
  ev.format       = 32;
  ev.data.l[0]    = data0;
  ev.data.l[1]    = data1;
  ev.data.l[2]    = data2;
  ev.data.l[3]    = data3;
  ev.data.l[4]    = data4;

  return XSendEvent(subtle->disp, dst, False, NoEventMask, (XEvent *)&ev);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
