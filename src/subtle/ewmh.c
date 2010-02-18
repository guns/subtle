
 /**
  * @package subtle
  *
  * @file EWMH functions
  * @copyright Copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <sys/types.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include "subtle.h"

static Atom atoms[SUB_EWMH_TOTAL];

/* Typedef {{{ */
typedef struct xembedinfo_t
{
  CARD32 version, flags;
} XEmbedInfo; /* }}} */

 /** subEwmhInit {{{
  * @brief Init and register ICCCM/EWMH atoms
  **/

void
subEwmhInit(void)
{
  int len = 0;
  long data[4] = { 0, 0, 0, 0 }, pid = (long)getpid();
  char *selection = NULL, *names[] =
  {
    /* ICCCM */
    "WM_NAME", "WM_CLASS", "WM_STATE", "WM_PROTOCOLS", "WM_TAKE_FOCUS", 
    "WM_DELETE_WINDOW", "WM_NORMAL_HINTS", "WM_SIZE_HINTS", "WM_HINTS",
    "WM_WINDOW_ROLE",

    /* EWMH */
    "_NET_SUPPORTED", "_NET_CLIENT_LIST", "_NET_CLIENT_LIST_STACKING", 
    "_NET_NUMBER_OF_DESKTOPS", "_NET_DESKTOP_NAMES", "_NET_DESKTOP_GEOMETRY",
    "_NET_DESKTOP_VIEWPORT", "_NET_CURRENT_DESKTOP", "_NET_ACTIVE_WINDOW",
    "_NET_WORKAREA", "_NET_SUPPORTING_WM_CHECK", "_NET_VIRTUAL_ROOTS",
    "_NET_CLOSE_WINDOW", "_NET_RESTACK_WINDOW", "_NET_MOVERESIZE_WINDOW",
    "_NET_SHOWING_DESKTOP",

    "_NET_WM_NAME", "_NET_WM_PID", "_NET_WM_DESKTOP", "_NET_WM_STRUT",
    "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DIALOG", "_NET_WM_STATE",
    "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_STICKY",

    /* Tray */
    "_NET_SYSTEM_TRAY_OPCODE", "_NET_SYSTEM_TRAY_MESSAGE_DATA",
    "_NET_SYSTEM_TRAY_S",

    /* Misc */
    "UTF8_STRING", "MANAGER",

    /* XEmbed */
    "_XEMBED", "_XEMBED_INFO",

    /* subtle */
    "SUBTLE_WINDOW_TAG", "SUBTLE_WINDOW_UNTAG", "SUBTLE_WINDOW_TAGS",
    "SUBTLE_WINDOW_GRAVITY", "SUBTLE_WINDOW_SCREEN", "SUBTLE_WINDOW_FLAGS",
    "SUBTLE_GRAVITY_NEW", "SUBTLE_GRAVITY_LIST", "SUBTLE_GRAVITY_KILL",
    "SUBTLE_TAG_NEW", "SUBTLE_TAG_LIST", "SUBTLE_TAG_KILL",
    "SUBTLE_TRAY_LIST", "SUBTLE_TRAY_KILL",
    "SUBTLE_VIEW_NEW", "SUBTLE_VIEW_KILL",
    "SUBTLE_SUBLET_NEW", "SUBTLE_SUBLET_UPDATE", "SUBTLE_SUBLET_DATA",
    "SUBTLE_SUBLET_BACKGROUND", "SUBTLE_SUBLET_LIST", "SUBTLE_SUBLET_KILL",
    "SUBTLE_RELOAD_CONFIG", "SUBTLE_RELOAD_SUBLETS", "SUBTLE_QUIT"
  };

  /* Apply tray selection */
  len       = strlen(names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION]) + 5; ///< For high screen counts
  selection = (char *)subSharedMemoryAlloc(len, sizeof(char)); 

  snprintf(selection, len, "%s%u", names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION], SCRN);
  subSharedLogDebug("Selection: len=%d, name=%s\n", len, selection);
  names[SUB_EWMH_NET_SYSTEM_TRAY_SELECTION] = selection;

  XInternAtoms(subtle->dpy, names, SUB_EWMH_TOTAL, 0, atoms); ///< Register atoms

  subtle->flags |= SUB_SUBTLE_EWMH; ///< Set EWMH flag

  /* EWMH: Supported hints */
  XChangeProperty(subtle->dpy, ROOT, atoms[SUB_EWMH_NET_SUPPORTED], XA_ATOM, 
    32, PropModeReplace, (unsigned char *)&atoms, SUB_EWMH_TOTAL);

  /* EWMH: Window manager information */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_SUPPORTING_WM_CHECK, 
    &subtle->windows.support, 1);
  subEwmhSetString(subtle->windows.support, SUB_EWMH_NET_WM_NAME, PKG_NAME);
  subEwmhSetString(subtle->windows.support, SUB_EWMH_WM_CLASS, PKG_NAME);
  subEwmhSetCardinals(subtle->windows.support, SUB_EWMH_NET_WM_PID, &pid, 1);
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&data, 2);
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_SHOWING_DESKTOP, (long *)&data, 1);

  /* EWMH: Workarea size */
  data[2] = SCREENW; 
  data[3] = SCREENH;
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_WORKAREA, (long *)&data, 4);

  /* EWMH: Desktop sizes */
  data[0] = SCREENW;
  data[1] = SCREENH;
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, NULL, 0);
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, NULL, 0);  

  subTraySelect(); ///< Finally select

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
  * @param[in]  e  A #SubEwmh
  * @retval  >=0  Found index
  * @retval  -1   Atom was not found
  **/

SubEwmh
subEwmhFind(Atom atom)
{
  int i;

  for(i = 0; atom && i < SUB_EWMH_TOTAL; i++)
    if(atoms[i] == atom) return i;

  return -1;
} /* }}} */

 /** subEwmhGetWMState {{{
  * @brief Get WM state from window
  * @param[in]  win  A window
  * @return Returns window WM state
  **/

long
subEwmhGetWMState(Window win)
{
  Atom type = None;
  int format = 0;
  unsigned long unused, bytes;
  long *data = NULL, state = WithdrawnState;

  assert(win);

  if(Success == XGetWindowProperty(subtle->dpy, win, atoms[SUB_EWMH_WM_STATE], 0L, 2L, False,
      atoms[SUB_EWMH_WM_STATE], &type, &format, &bytes, &unused, 
      (unsigned char **)&data) && bytes)
    {
      state = *data;
      XFree(data);
    }

  return state;
} /* }}} */

 /** subEwmhGetXEmbedState {{{
  * @brief Get window Xembed state
  * @param[in]  win  A window
  **/

long
subEwmhGetXEmbedState(Window win)
{
  long flags = 0;
  XEmbedInfo *info = NULL;

  /* Get xembed data */
  if((info = (XEmbedInfo *)subSharedPropertyGet(win, subEwmhGet(SUB_EWMH_XEMBED_INFO), 
      SUB_EWMH_XEMBED_INFO, NULL))) 
    {
      flags = (long)info->flags;

      subSharedLogDebug("XEmbedInfo: win=%#lx, version=%ld, flags=%ld, mapped=%ld\n", 
        win, info->version, info->flags, info->flags & XEMBED_MAPPED);
      
      XFree(info);
    }

  return flags;
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
  XChangeProperty(subtle->dpy, win, atoms[e], XA_WINDOW, 32, PropModeReplace,
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
  XChangeProperty(subtle->dpy, win, atoms[e], XA_CARDINAL, 32, PropModeReplace,
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
  XChangeProperty(subtle->dpy, win, atoms[e], atoms[SUB_EWMH_UTF8], 8,
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
  XTextProperty text;

  XStringListToTextProperty(values, size, &text);
  XSetTextProperty(subtle->dpy, win, &text, atoms[e]);

  XFree (text.value);
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

  XChangeProperty(subtle->dpy, win, atoms[SUB_EWMH_WM_STATE], 
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

  return XSendEvent(subtle->dpy, dst, False, NoEventMask, (XEvent *)&ev);
} /* }}} */

 /** subEwmhFinish {{{
  * @brief Delete set ICCCM/EWMH atoms
  **/

void
subEwmhFinish(void)
{
  if(subtle->flags & SUB_SUBTLE_EWMH) ///< Delete properties only on real shutdown
    {
      /* EWMH properties */
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_SUPPORTED);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_SUPPORTING_WM_CHECK);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_DESKTOP_NAMES);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_NUMBER_OF_DESKTOPS);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_DESKTOP_VIEWPORT);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_SHOWING_DESKTOP);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_DESKTOP_GEOMETRY);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_VIRTUAL_ROOTS);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_WORKAREA);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_CLIENT_LIST);
      subSharedPropertyDelete(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING);

      /* subtle extension */
      subSharedPropertyDelete(ROOT, SUB_EWMH_SUBTLE_TAG_LIST);
      subSharedPropertyDelete(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST);
    }

  subSharedLogDebug("kill=ewmh\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
