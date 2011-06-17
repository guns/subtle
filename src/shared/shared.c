
 /**
  * @package subtle
  *
  * @file Shared functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "shared.h"

/* Log */

/* Enable default messages */
static int loglevel = DEFAULT_LOGLEVEL;

 /** subSharedLogLevel {{{
  * @brief Set loglevel
  **/

void
subSharedLogLevel(int level)
{
  loglevel |= level;
} /* }}} */

 /** subSharedLog {{{
  * @brief Print messages depending on type
  * @param[in]  level   Message level
  * @param[in]  file    File name
  * @param[in]  line    Line number
  * @param[in]  format  Message format
  * @param[in]  ...     Variadic arguments
  **/

void
subSharedLog(int level,
  const char *file,
  int line,
  const char *format,
  ...)
{
  va_list ap;
  char buf[255];

#ifdef DEBUG
  if(!(loglevel & level)) return;
#endif /* DEBUG */

  /* Get variadic arguments */
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  /* Print according to loglevel */
  if(level & SUB_LOG_WARN)
    fprintf(stdout, "<WARNING> %s", buf);
  else if(level & SUB_LOG_ERROR)
    fprintf(stderr, "<ERROR> %s", buf);
  else if(level & SUB_LOG_SUBLET)
    fprintf(stderr, "<WARNING SUBLET %s> %s", file, buf);
  else if(level & SUB_LOG_DEPRECATED)
    fprintf(stdout, "<DEPRECATED> %s", buf);
#ifdef DEBUG
  else if(level & SUB_LOG_EVENTS)
    fprintf(stderr, "<EVENTS> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_RUBY)
    fprintf(stderr, "<RUBY> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_XERROR)
    fprintf(stderr, "<XERROR> %s", buf);
  else if(level & SUB_LOG_SUBTLEXT)
    fprintf(stderr, "<SUBTLEXT> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_SUBTLE)
    fprintf(stderr, "<SUBTLE> %s:%d: %s", file, line, buf);
  else if(level & SUB_LOG_DEBUG)
    fprintf(stderr, "<DEBUG> %s:%d: %s", file, line, buf);
#endif /* DEBUG */
} /* }}} */

 /** subSharedLogXError {{{
  * @brief Print X error messages
  * @params[in]  disp  Display
  * @params[in]  ev    A #XErrorEvent
  * @return Returns zero
  * @retval  0  Default return value
  **/

int
subSharedLogXError(Display *disp,
  XErrorEvent *ev)
{
#ifdef DEBUG
  if(loglevel) return 0;
#endif /* DEBUG */

  if(42 != ev->request_code) /* X_SetInputFocus */
    {
      char error[255] = { 0 };

      XGetErrorText(disp, ev->error_code, error, sizeof(error));
      subSharedLog(SUB_LOG_XERROR, __FILE__, __LINE__,
        "%s: win=%#lx, request=%d\n",
        error, ev->resourceid, ev->request_code);
    }

  return 0;
} /* }}} */

/* Memory */

 /** subSharedMemoryAlloc {{{
  * @brief Alloc memory and check result
  * @param[in]  n     Number of elements
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subSharedMemoryAlloc(size_t n,
  size_t size)
{
  void *mem = NULL;

  /* Check result */
  if(!(mem = calloc(n, size)))
    {
      subSharedLogError("Failed allocating memory\n");
    }

  return mem;
} /* }}} */

 /** subSharedMemoryRealloc {{{
  * @brief Realloc memory and check result
  * @param[in]  mem   Memory block
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subSharedMemoryRealloc(void *mem,
  size_t size)
{
  /* Check result */
  if(!(mem = realloc(mem, size)))
    {
      subSharedLogDebug("Memory has been freed. Expected?\n");
    }

  return mem;
} /* }}} */

/* Regex */

 /** subSharedRegexNew {{{
  * @brief Create new regex
  * @param[in]  pattern  Regex
  * @return Returns a #regex_t or \p NULL
  **/

regex_t *
subSharedRegexNew(char *pattern)
{
  int ecode = 0;
  regex_t *preg = NULL;
  OnigErrorInfo einfo;

  assert(pattern);

  /* Create onig regex */
  ecode = onig_new(&preg, (UChar *)pattern,
    (UChar *)(pattern + strlen(pattern)),
    ONIG_OPTION_EXTEND|ONIG_OPTION_SINGLELINE|ONIG_OPTION_IGNORECASE,
    ONIG_ENCODING_ASCII, ONIG_SYNTAX_RUBY, &einfo);

  /* Check for compile errors */
  if(ecode)
    {
      UChar ebuf[ONIG_MAX_ERROR_MESSAGE_LEN] = { 0 };

      onig_error_code_to_str((UChar*)ebuf, ecode, &einfo);

      subSharedLogWarn("Failed compiling regex `%s': %s\n", pattern, ebuf);

      free(preg);

      return NULL;
    }

  return preg;
} /* }}} */

 /** subSharedRegexMatch {{{
  * @brief Check if string match preg
  * @param[in]  preg      A #regex_t
  * @param[in]  string    String
  * @retval  1  If string matches preg
  * @retval  0  If string doesn't match
  **/

int
subSharedRegexMatch(regex_t *preg,
  char *string)
{
  assert(preg);

  return ONIG_MISMATCH != onig_match(preg, (UChar *)string,
    (UChar *)(string + strlen(string)), (UChar *)string, NULL,
    ONIG_OPTION_NONE);
} /* }}} */

 /** subSharedRegexKill {{{
  * @brief Kill preg
  * @param[in]  preg  #regex_t
  **/

void
subSharedRegexKill(regex_t *preg)
{
  assert(preg);

  onig_free(preg);
} /* }}} */

/* Property */

 /** subSharedPropertyGet {{{
  * @brief Get window property
  * @param[in]     disp  Display
  * @param[in]     win   Client window
  * @param[in]     type  Property type
  * @param[in]     prop  Property
  * @param[inout]  size  Size of the property
  * return Returns the property
  **/

char *
subSharedPropertyGet(Display *disp,
  Window win,
  Atom type,
  Atom prop,
  unsigned long *size)
{
  int format = 0;
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  Atom rtype = None;

  assert(win);

  /* Get property */
  if(Success != XGetWindowProperty(disp, win, prop, 0L, 4096,
      False, type, &rtype, &format, &nitems, &bytes, &data))
    {
      subSharedLogDebug("Failed getting property `%ld'\n", prop);

      return NULL;
    }

  /* Check result */
  if(type != rtype)
    {
      subSharedLogDebug("Property: prop=%ld, type=%ld, rtype=%ld\n",
        prop, type, rtype);
      XFree(data);

      return NULL;
    }

  if(size) *size = nitems;

  return (char *)data;
} /* }}} */

 /** subSharedPropertyGetStrings {{{
  * @brief Get property list
  * @param[in]     disp Display
  * @param[in]     win     Client window
  * @param[in]     prop    Property
  * @param[inout]  nlist   Size of the property list
  * return Returns the property list
  **/

char **
subSharedPropertyGetStrings(Display *disp,
  Window win,
  Atom prop,
  int *nlist)
{
  char **list = NULL;
  XTextProperty text;

  assert(win && nlist);

  /* Check UTF8 and XA_STRING */
  if((XGetTextProperty(disp, win, &text, prop) ||
      XGetTextProperty(disp, win, &text, XA_STRING)) && text.nitems)
    {
      XmbTextPropertyToTextList(disp, &text, &list, nlist);

      XFree(text.value);
    }

  return list;
} /* }}} */

 /** subSharedPropertySetStrings {{{
  * @brief Convert list to multibyte and set property
  * @param[in]  disp    Display
  * @param[in]  win     Window
  * @param[in]  prop    Property
  * @param[in]  list    String list
  * @param[in]  nsize   Number of elements
  * return Returns the property list
  **/

void
subSharedPropertySetStrings(Display *disp,
  Window win,
  Atom prop,
  char **list,
  int nlist)
{
  XTextProperty text;

  /* Convert list to multibyte text property */
  XmbTextListToTextProperty(disp, list, nlist, XUTF8StringStyle, &text);
  XSetTextProperty(disp, win, &text, prop);

  XFree(text.value);
} /* }}} */

 /** subSharedPropertyName {{{
  * @brief Get window name
  * @warning Must be free'd
  * @param[in]     disp      Display
  * @param[in]     win       A #Window
  * @param[inout]  name      Window WM_NAME
  * @param[in]     fallback  Fallback name
  **/

void
subSharedPropertyName(Display *disp,
  Window win,
  char **name,
  char *fallback)
{
 char **list = NULL;
  XTextProperty text;

  /* Get text property */
  XGetTextProperty(disp, win, &text, XInternAtom(disp, "_NET_WM_NAME", False));
  if(0 == text.nitems)
    {
      XGetTextProperty(disp, win, &text, XA_WM_NAME);
      if(0 == text.nitems)
        {
          *name = strdup(fallback);

          return;
        }
    }

  /* Handle encoding */
  if(XA_STRING == text.encoding)
    {
      *name = strdup((char *)text.value);
    }
  else ///< Utf8 string
    {
      int nlist = 0;

      /* Convert text property */
      if(Success == XmbTextPropertyToTextList(disp, &text, &list, &nlist) &&
          list)
        {
          if(0 < nlist && *list)
            {
              /* FIXME strdup() allocates not enough memory to hold string */
              *name = subSharedMemoryAlloc(text.nitems + 2, sizeof(char));
              strncpy(*name, *list, text.nitems);
            }
          XFreeStringList(list);
        }
    }

  if(text.value) XFree(text.value);

  /* Fallback */
  if(!*name) *name = strdup(fallback);
} /* }}} */

 /** subSharedPropertyClass {{{
  * @brief Get window class
  * @warning Must be free'd
  * @param[in]     disp  Display
  * @param[in]     win      A #Window
  * @param[inout]  inst     Window instance name
  * @param[inout]  klass    Window class name
  **/

void
subSharedPropertyClass(Display *disp,
  Window win,
  char **inst,
  char **klass)
{
  int size = 0;
  char **klasses = NULL;

  assert(win);

  klasses = subSharedPropertyGetStrings(disp, win, XA_WM_CLASS, &size);

  /* Sanitize instance/class names */
  if(inst)  *inst  = strdup(0 < size ? klasses[0] : "subtle");
  if(klass) *klass = strdup(1 < size ? klasses[1] : "subtle");

  if(klasses) XFreeStringList(klasses);
} /* }}} */

 /** subSharedPropertyGeometry {{{
  * @brief Get window geometry
  * @param[in]     disp   Display
  * @param[in]     win       A #Window
  * @param[inout]  geometry  A #XRectangle
  **/

void
subSharedPropertyGeometry(Display *disp,
  Window win,
  XRectangle *geometry)
{
  Window root = None;
  unsigned int bw = 0, depth = 0;
  XRectangle r = { 0 };

  assert(win && geometry);

  XGetGeometry(disp, win, &root, (int *)&r.x, (int *)&r.y,
    (unsigned int *)&r.width, (unsigned int *)&r.height, &bw, &depth);

  *geometry = r;
} /* }}} */

 /** subSharedPropertyDelete {{{
  * @brief Deletes the property
  * @param[in]  disp  Display
  * @param[in]  win   A #Window
  * @param[in]  prop  Property
  **/

void
subSharedPropertyDelete(Display *disp,
  Window win,
  Atom prop)
{
  assert(win);

  XDeleteProperty(disp, win, prop);
} /* }}} */

/* Text */

 /** subSharedTextNew {{{
  * @brief Parse text
  * @return New #SubText
  **/

SubText *
subSharedTextNew(void)
{
  return TEXT(subSharedMemoryAlloc(1, sizeof(SubText)));
} /* }}} */

 /** subSharedTextParse {{{
  * @brief Parse text
  * @param[in]  disp    Display
  * @param[in]  f       A #SubFont
  * @param[in]  t       A #SubText
  * @param[in]  textfg  Default text color
  * @param[in]  iconfg  Default icon color
  * @param[in]  text    String to parse
  **/

int
subSharedTextParse(Display *disp,
  SubFont *f,
  SubText *t,
  unsigned long textfg,
  unsigned long iconfg,
  char *text)
{
  int i = 0, left = 0, right = 0;
  char *tok = NULL;
  unsigned long color = -1, pixmap = 0;
  SubTextItem *item = NULL;

  assert(f && t);

  t->width = 0;

  /* Split and iterate over tokens */
  while((tok = strsep(&text, SEPARATOR)))
    {
      if('#' == *tok) color = strtol(tok + 1, NULL, 0); ///< Color
      else if('\0' != *tok) ///< Text or icon
        {
          /* Re-use items to save alloc cycles */
          if(i < t->nitems && (item = ITEM(t->items[i])))
            {
              if(!(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP)) &&
                  item->data.string)
                free(item->data.string);

              item->flags &= ~(SUB_TEXT_EMPTY|SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP);
            }
          else if((item = ITEM(subSharedMemoryAlloc(1, sizeof(SubTextItem)))))
            {
              /* Add icon to array */
              t->items = (SubTextItem **)subSharedMemoryRealloc(t->items,
                (t->nitems + 1) * sizeof(SubTextItem *));
              t->items[(t->nitems)++] = item;
            }

          /* Get geometry of bitmap/pixmap */
          if(('!' == *tok || '&' == *tok) &&
              (pixmap = strtol(tok + 1, NULL, 0)))
            {
              XRectangle geometry = { 0 };

              subSharedPropertyGeometry(disp, pixmap, &geometry);

              item->flags    |= ('!' == *tok ? SUB_TEXT_BITMAP :
                SUB_TEXT_PIXMAP);
              item->data.num  = pixmap;
              item->width     = geometry.width;
              item->height    = geometry.height;

              /* Add spacing and check if icon is first */
              t->width += item->width + (0 == i ? 3 : 6);

              item->color = -1 != iconfg ? iconfg : color;
            }
          else ///< Ordinary text
            {
              item->data.string = strdup(tok);
              item->width       = subSharedTextWidth(disp, f, tok,
                strlen(tok), &left, &right, False);

              /* Remove left bearing from first text item */
              t->width += item->width - (0 == i ? left : 0);

              item->color = -1 != textfg ? textfg : color;
            }

          i++;
        }
    }

  /* Mark other items a clean */
  for(; i < t->nitems; i++)
    ITEM(t->items[i])->flags |= SUB_TEXT_EMPTY;

  /* Fix spacing of last item */
  if(item)
    {
      if(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP))
        t->width -= 2;
      else
        {
          t->width    -= right;
          item->width -= right;
        }
    }

  return t->width;
} /* }}} */

 /** subSharedTextRender {{{
  * @brief Render text
  * @param[in]  disp  Display
  * @param[in]  gc    GC
  * @param[in]  f     A #SubFont
  * @param[in]  win   A #Window
  * @param[in]  x     X position
  * @param[in]  y     Y position
  * @param[in]  fg    Foreground color
  * @param[in]  bg    Background color
  * @param[in]  t     A #SubText
  **/

void
subSharedTextRender(Display *disp,
  GC gc,
  SubFont *f,
  Window win,
  int x,
  int y,
  long fg,
  long bg,
  SubText *t)
{
  int i, width = x;

  assert(t);

  /* Render text items */
  for(i = 0; i < t->nitems; i++)
    {
      SubTextItem *item = ITEM(t->items[i]);

      if(item->flags & SUB_TEXT_EMPTY) ///< Empty text
        {
          break; ///< Break loop
        }
      else if(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP)) ///< Icons
        {
          int icony = 0, dx = (0 == i) ? 0 : 3; ///< Add spacing when icon isn't first

          icony = item->height > f->height ?
            y - f->y - ((item->height - f->height) / 2): y - item->height;

          subSharedTextIconDraw(disp, gc, win, width + dx, icony,
            item->width, item->height, (-1 == item->color) ? fg : item->color,
            bg, (Pixmap)item->data.num, (item->flags & SUB_TEXT_BITMAP));

          /* Add spacing when icon isn't last */
          width += item->width + dx + (i != t->nitems - 1 ? 3 : 0);
        }
      else ///< Text
        {
          subSharedTextDraw(disp, gc, f, win, width, y,
            (-1 == item->color) ? fg : item->color, bg, 
            item->data.string, strlen(item->data.string));

          width += item->width;
        }
    }
} /* }}} */

 /** subSharedTextWidth {{{
  * @brief Get width of the smallest enclosing box
  * @param[in]     disp    Display
  * @param[in]     text    The text
  * @param[in]     len     Length of the string
  * @param[inout]  left    Left bearing
  * @param[inout]  right   Right bearing
  * @param[in]     center  Center text
  * @return Width of the box
  **/

int
subSharedTextWidth(Display *disp,
  SubFont *f,
  const char *text,
  int len,
  int *left,
  int *right,
  int center)
{
  int width = 0, lbearing = 0, rbearing = 0;

  assert(f);

  /* Get text extents based on font */
  if(text && 0 < len)
    {
#ifdef HAVE_X11_XFT_XFT_H
      if(f->xft) ///< XFT
        {
          XGlyphInfo extents;

          XftTextExtentsUtf8(disp, f->xft, (XftChar8 *)text, len, &extents);

          width    = extents.xOff;
          lbearing = extents.x;
        }
      else ///< XFS
#endif /* HAVE_X11_XFT_XFT_H */
        {
          XRectangle overall_ink = { 0 }, overall_logical = { 0 };

          XmbTextExtents(f->xfs, text, len,
            &overall_ink, &overall_logical);

          width    = overall_logical.width;
          lbearing = overall_logical.x;
        }

      /* Get left and right spacing */
      if(left)  *left  = lbearing;
      if(right) *right = rbearing;
    }

  return center ? width - abs(lbearing - rbearing) : width;
} /* }}} */

 /** subSharedTextDraw {{{
  * @brief Draw text
  * @param[in]  disp  Display
  * @param[in]  gc    GC
  * @param[in]  f     A #SubFont
  * @param[in]  win   Target window
  * @param[in]  x     X position
  * @param[in]  y     Y position
  * @param[in]  fg    Foreground color
  * @param[in]  bg    Background color
  * @param[in]  text  Text to draw
  * @param[in]  len   Text length
  **/

void
subSharedTextDraw(Display *disp,
  GC gc,
  SubFont *f,
  Window win,
  int x,
  int y,
  long fg,
  long bg,
  const char *text,
  int len)
{
  XGCValues gvals;

  assert(f && text);

  /* Draw text */
#ifdef HAVE_X11_XFT_XFT_H
  if(f->xft) ///< XFT
    {
      XftColor color = { 0 };
      XColor xcolor = { 0 };

      /* Get color values */
      xcolor.pixel = fg;
      XQueryColor(disp, DefaultColormap(disp, DefaultScreen(disp)), &xcolor);

      color.pixel       = xcolor.pixel;
      color.color.red   = xcolor.red;
      color.color.green = xcolor.green;
      color.color.blue  = xcolor.blue;
      color.color.alpha = 0xffff;

      XftDrawChange(f->draw, win);
      XftDrawStringUtf8(f->draw, &color, f->xft, x, y, (XftChar8 *)text, len);
    }
  else ///< XFS
#endif /* HAVE_X11_XFT_XFT_H */
    {
      /* Draw text */
      gvals.foreground = fg;
      gvals.background = bg;

      XChangeGC(disp, gc, GCForeground|GCBackground, &gvals);
      XmbDrawString(disp, win, f->xfs, gc, x, y, text, len);
    }
} /* }}} */

 /** subSharedTextIconDraw {{{
  * @brief Draw text
  * @param[in]  disp    Display
  * @param[in]  gc      GC
  * @param[in]  win     Target window
  * @param[in]  x       X position
  * @param[in]  y       Y position
  * @param[in]  width   Width of pixmap
  * @param[in]  height  Height of pixmap
  * @param[in]  fg      Foreground color
  * @param[in]  bg      Background color
  * @param[in]  pixmap  Pixmap to draw
  * @param[in]  bitmap  Whether icon is a bitmap
  **/

void
subSharedTextIconDraw(Display *disp,
  GC gc,
  Window win,
  int x,
  int y,
  int width,
  int height,
  long fg,
  long bg,
  Pixmap pixmap,
  int bitmap)
{
  XGCValues gvals;

  /* Plane color */
  gvals.foreground = fg;
  gvals.background = bg;
  XChangeGC(disp, gc, GCForeground|GCBackground, &gvals);

  /* Copy icon to destination window */
  if(bitmap)
    XCopyPlane(disp, pixmap, win, gc, 0, 0, width, height, x, y, 1);
#ifdef HAVE_X11_XPM_H
  else XCopyArea(disp, pixmap, win, gc, 0, 0, width, height, x, y);
#endif /* HAVE_X11_XPM_H */
} /* }}} */

 /** subSharedTextFree {{{
  * @brief Free text
  * @param[in]  t  A #SubText
  **/

void
subSharedTextFree(SubText *t)
{
  int i;

  assert(t);

  for(i = 0; i < t->nitems; i++)
    {
      SubTextItem *item = (SubTextItem *)t->items[i];

      if(!(item->flags & (SUB_TEXT_BITMAP|SUB_TEXT_PIXMAP)) &&
          item->data.string)
        free(item->data.string);

      free(t->items[i]);
    }

  free(t->items);
  free(t);
} /* }}} */

/* Font */

 /** subSharedFontNew {{{
  * @brief Create new font
  * @param[in]  disp  Display
  * @param[in]  name  Font name
  * @return  New #SubFont
  **/

SubFont *
subSharedFontNew(Display *disp,
  const char *name)
{
  int n = 0;
  char *def = NULL, **missing = NULL, **names = NULL;
  XFontStruct **xfonts = NULL;
  SubFont *f = NULL;

  /* Create new font */
  f = FONT(subSharedMemoryAlloc(1, sizeof(SubFont)));

  /* Load font */
#ifdef HAVE_X11_XFT_XFT_H
  if(!strncmp(name, "xft:", 4))
    {
      /* Load XFT font */
      if(!(f->xft = XftFontOpenName(disp, DefaultScreen(disp), name + 4)))
        {
          subSharedLogWarn("Failed loading font `%s' - using default\n", name);

          f->xft = XftFontOpenXlfd(disp, DefaultScreen(disp), name + 4);
        }

      if(f->xft)
        {
          f->draw = XftDrawCreate(disp, DefaultRootWindow(disp),
            DefaultVisual(disp, DefaultScreen(disp)),
            DefaultColormap(disp, DefaultScreen(disp)));

          /* Font metrics */
          f->height = f->xft->ascent + f->xft->descent + 2;
          f->y      = (f->height - 2 + f->xft->ascent) / 2;
        }

    }
  else
#endif /* HAVE_X11_XFT_XFT_H */
    {
      /* Load font set */
      if(!(f->xfs = XCreateFontSet(disp, name, &missing, &n, &def)))
        {
          subSharedLogWarn("Failed loading font `%s' - using default\n", name);

          if(!(f->xfs = XCreateFontSet(disp, DEFFONT, &missing, &n, &def)))
            {
              subSharedLogError("Failed loading fallback font `%s`\n",
                DEFFONT);

              if(missing) XFreeStringList(missing); ///< Ignore this
              free(f);

              return NULL;
            }
        }

      XFontsOfFontSet(f->xfs, &xfonts, &names);

      /* Font metrics */
      f->height = xfonts[0]->max_bounds.ascent +
        xfonts[0]->max_bounds.descent + 2;
      f->y      = (f->height - 2 + xfonts[0]->max_bounds.ascent) / 2;

      if(missing) XFreeStringList(missing); ///< Ignore this
    }

  return f;
} /* }}} */

 /** subSharedFontKill {{{
  * @brief Kill a font
  * @param[in]  disp  Display
  * @param[in]  f     A #SubFont
  **/

void
subSharedFontKill(Display *disp,
  SubFont *f)
{
  assert(f);

#ifdef HAVE_X11_XFT_XFT_H
  if(f->xft)
    {
      XftFontClose(disp, f->xft);
      XftDrawDestroy(f->draw);
    }
  else
#endif /* HAVE_X11_XFT_XFT_H */
    {
      XFreeFontSet(disp, f->xfs);
    }

  free(f);
} /* }}} */

/* Misc */

 /** subSharedParseColor {{{
  * @brief Parse and load color
  * @param[in]  disp  Display
  * @param[in]  name     Color string
  * @return Color pixel value
  **/

unsigned long
subSharedParseColor(Display *disp,
  char *name)
{
  XColor xcolor = { 0 }; ///< Default color

  assert(name);

  /* Parse and store color */
  if(!XParseColor(disp, DefaultColormap(disp, DefaultScreen(disp)),
      name, &xcolor))
    {
      subSharedLogWarn("Failed loading color `%s'\n", name);
    }
  else if(!XAllocColor(disp, DefaultColormap(disp, DefaultScreen(disp)),
      &xcolor))
    subSharedLogWarn("Failed allocating color `%s'\n", name);

  return xcolor.pixel;
} /* }}} */

 /** subSharedParseKey {{{
  * @brief Parse key
  * @param[in]     disp     Display
  * @param[in]     key      Key string
  * @param[inout]  code     Key code
  * @param[inout]  state    Key state
  * @param[inout]  mouse    Whether this is mouse press
  * @return Color keysym or \p NoSymbol
  **/

KeySym
subSharedParseKey(Display *disp,
  const char *key,
  unsigned int *code,
  unsigned int *state,
  int *mouse)
{
  int i;
  KeySym sym = NoSymbol;
  char *tokens = NULL, *tok = NULL, *save = NULL;

  /* Parse keys */
  tokens = strdup(key);
  tok    = strtok_r(tokens, "-", &save);

  while(tok)
    {
      /* Get key sym and modifier */
      if(NoSymbol == ((sym = XStringToKeysym(tok))))
        {
          const char *buttons[] = { "B1", "B2", "B3", "B4", "B5" };

          /* Check mouse buttons */
          for(i = 0; i < LENGTH(buttons); i++)
            if(!strncmp(tok, buttons[i], 2))
              {
                sym = XK_Pointer_Button1 + i; ///< @todo Implementation independent?

                break;
              }

          /* Check if there's still no symbol */
          if(NoSymbol == sym)
            {
              free(tokens);

              return sym;
            }
        }

      /* Modifier mappings */
      switch(sym)
        {
          /* Keys */
          case XK_S: *state |= ShiftMask;   break;
          case XK_C: *state |= ControlMask; break;
          case XK_A: *state |= Mod1Mask;    break;
          case XK_M: *state |= Mod3Mask;    break;
          case XK_W: *state |= Mod4Mask;    break;

          /* Mouse */
          case XK_Pointer_Button1:
          case XK_Pointer_Button2:
          case XK_Pointer_Button3:
          case XK_Pointer_Button4:
          case XK_Pointer_Button5:
            *mouse = True;
            *code  = sym;
            break;
          default:
            *mouse = False;
            *code  = XKeysymToKeycode(disp, sym);
        }

      tok = strtok_r(NULL, "-", &save);
    }

  free(tokens);

  return sym;
} /* }}} */

 /** subSharedSpawn {{{
  * @brief Spawn a command
  * @param[in]  cmd  Command string
  **/

pid_t
subSharedSpawn(char *cmd)
{
  pid_t pid = fork();

  switch(pid)
    {
      case 0:
        setsid();
        execlp("/bin/sh", "sh", "-c", cmd, NULL);

        subSharedLogWarn("Failed executing command `%s'\n", cmd); ///< Never to be reached
        exit(1);
      case -1: subSharedLogWarn("Failed forking `%s'\n", cmd);
    }

  return pid;
} /* }}} */

#ifndef SUBTLE

 /** subSharedMessage {{{
  * @brief Send client message to window
  * @param[in]  disp    Display
  * @param[in]  win     Client window
  * @param[in]  type    Message type
  * @param[in]  data    A #SubMessageData
  * @param[in]  format  Data format
  * @param[in]  xsync   Sync connection
  * @returns
  **/

int
subSharedMessage(Display *disp,
  Window win,
  char *type,
  SubMessageData data,
  int format,
  int xsync)
{
  int status = 0;
  XEvent ev;
  long mask = SubstructureRedirectMask|SubstructureNotifyMask;

  assert(win);

  /* Assemble event */
  ev.xclient.type         = ClientMessage;
  ev.xclient.serial       = 0;
  ev.xclient.send_event   = True;
  ev.xclient.message_type = XInternAtom(disp, type, False);
  ev.xclient.window       = win;
  ev.xclient.format       = format;

  /* Copy data over */
  ev.xclient.data.l[0] = data.l[0];
  ev.xclient.data.l[1] = data.l[1];
  ev.xclient.data.l[2] = data.l[2];
  ev.xclient.data.l[3] = data.l[3];
  ev.xclient.data.l[4] = data.l[4];

  if(!disp || !((status = XSendEvent(disp, DefaultRootWindow(disp),
      False, mask, &ev))))
    subSharedLogWarn("Failed sending client message `%s'\n", type);

  if(True == xsync) XSync(disp, False);

  return status;
} /* }}} */

#endif /* SUBTLE */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
