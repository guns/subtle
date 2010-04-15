
 /**
  * @package subtle
  *
  * @file Ruby functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <ctype.h>
#include <ruby.h>
#include "subtle.h"

#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))

#define PANELSLENGTH  3
#define GRABSLENGTH  15
#define TAGSLENGTH    9
#define HOOKSLENGTH  12

static VALUE shelter = Qnil, subtlext = Qnil; ///< Globals

/* Typedef {{{ */
typedef struct rubygrabs_t
{
  VALUE         sym;
  unsigned long flags, extra;
} RubyGrabs;

typedef struct rubygravities_t
{
  VALUE      sym;
  XRectangle geometry;
} RubyGravities;

typedef struct rubypanels_t
{
  VALUE    sym;
  SubPanel *panel;
} RubyPanels;

typedef struct rubysymbol_t
{
  VALUE sym;
  int   flags;
} RubySymbols;

typedef struct rubymethods_t
{
  VALUE sym, real;
  int   flags, arity;
} RubyMethods;
/* }}} */

/* RubyBacktrace {{{ */
static VALUE
RubyBacktrace(void)
{
  int i;
  VALUE lasterr = Qnil, message = Qnil, klass = Qnil, backtrace = Qnil, entry = Qnil;

  /* Fetching backtrace data */
  lasterr   = rb_gv_get("$!");
  message   = rb_obj_as_string(lasterr);
  klass     = rb_class_path(CLASS_OF(lasterr));
  backtrace = rb_funcall(lasterr, rb_intern("backtrace"), 0, NULL);

  /* Print error and backtrace */
  subSharedLogWarn("%s: %s\n", RSTRING_PTR(klass), RSTRING_PTR(message));
  for(i = 0; Qnil != (entry = rb_ary_entry(backtrace, i)); ++i)
    printf("\tfrom %s\n", RSTRING_PTR(entry));

  return Qnil;
} /* }}} */

/* RubySignal {{{ */
static void
RubySignal(int signum)
{
  if(SIGALRM == signum) ///< Catch SIGALRM
    rb_raise(rb_eInterrupt, "Execution time (%ds) expired", EXECTIME);
} /* }}} */

/* RubyFilter {{{ */
static inline int
RubyFilter(const struct dirent *entry)
{
  return !fnmatch("*.rb", entry->d_name, FNM_PATHNAME);
} /* }}} */

/* RubyConvert {{{ */
static VALUE
RubyConvert(void *data)
{
  SubClient *c = NULL;
  VALUE object = Qnil;

  /* Convert object to ruby */
  if((c = CLIENT(data)))
    {
      int id = 0;
      VALUE klass = Qnil;

      subRubyLoadSubtlext(); ///< Load subtlext on demand

      XSync(subtle->dpy, False); ///< Sync before going on

      if(c->flags & SUB_TYPE_CLIENT) /* {{{ */
        {
          int flags = 0;
          char *role = NULL;

          /* Create client instance */
          id     = subArrayIndex(subtle->clients, (void *)c);
          klass  = rb_const_get(subtlext, rb_intern("Client"));
          object = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(c->win));
          role   = subSharedPropertyGet(subtle->dpy, LONG2NUM(c->win),
            XA_STRING, subEwmhGet(SUB_EWMH_WM_WINDOW_ROLE), NULL);

          /* Translate flags */
          if(c->flags & SUB_MODE_FULL)  flags |= SUB_EWMH_FULL;
          if(c->flags & SUB_MODE_FLOAT) flags |= SUB_EWMH_FLOAT;
          if(c->flags & SUB_MODE_STICK) flags |= SUB_EWMH_STICK;

          /* Set properties */
          rb_iv_set(object, "@id",       INT2FIX(id));
          rb_iv_set(object, "@name",     rb_str_new2(c->name));
          rb_iv_set(object, "@instance", rb_str_new2(c->instance));
          rb_iv_set(object, "@klass",    rb_str_new2(c->klass));
          rb_iv_set(object, "@role",     role ? rb_str_new2(role) : Qnil);
          rb_iv_set(object, "@flags",    INT2FIX(flags));

          if(role) free(role);
        } /* }}} */
      else if(c->flags & SUB_TYPE_VIEW) /* {{{ */
        {
          SubView *v = VIEW(c);

          /* Create view instance */
          id     = subArrayIndex(subtle->views, (void *)v);
          klass  = rb_const_get(subtlext, rb_intern("View"));
          object = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(v->name));

          /* Set properties */
          rb_iv_set(object, "@id",  INT2FIX(id));
          rb_iv_set(object, "@win", LONG2NUM(v->button));
        } /* }}} */
      else if(c->flags & SUB_TYPE_TAG) /* {{{ */
        {
          SubTag *t = TAG(c);

          /* Create tag instance */
          id     = subArrayIndex(subtle->tags, (void *)t);
          klass  = rb_const_get(subtlext, rb_intern("Tag"));
          object = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(t->name));

          /* Set properties */
          rb_iv_set(object, "@id", INT2FIX(id));
        } /* }}} */
    }

  return object;
} /* }}} */

/* Fetch */

/* RubyFetchKey {{{ */
static VALUE
RubyFetchKey(VALUE hash,
  char *key,
  int type,
  int optional)
{
  VALUE value = Qnil, sym = Qnil;

  assert(key);

  /* Fetch key */
  sym = CHAR2SYM(key);
  if(RTEST(hash) && Qtrue == rb_funcall(hash, rb_intern("has_key?"), 1, sym))
    value = rb_funcall(hash, rb_intern("fetch"), 1, sym);

  /* Check value type */
  if(type && rb_type(value) != type) value = Qnil;
  if(!optional && NIL_P(value)) subSharedLogWarn("Failed reading key `%s'\n", key);

  return value;
} /* }}} */

/* RubyFetchGeometry {{{ */
static int
RubyFetchGeometry(VALUE ary,
  XRectangle *geometry)
{
  int data[4] = { 0 }, ret = False;

  assert(geometry);

  /* Check value type */
  if(T_ARRAY == rb_type(ary))
    {
      int i;
      VALUE value = Qnil, meth = rb_intern("at");

      if((4 == FIX2INT(rb_funcall(ary, rb_intern("size"), 0, NULL))))
        {
          for(i = 0; i < 4; i++) ///< Safely fetching array values
            {
              value   = rb_funcall(ary, meth, 1, INT2FIX(i)) ;
              data[i] = RTEST(value) ? FIX2INT(value) : 0;
            }

          ret = True;
        }
    }

  /* Assign data to rect */
  geometry->x      = data[0];
  geometry->y      = data[1];
  geometry->width  = data[2];
  geometry->height = data[3];

  return ret;
} /* }}} */

/* Get */

/* RubyGetString {{{ */
static char *
RubyGetString(VALUE hash,
  char *key,
  char *fallback)
{
  VALUE value = RubyFetchKey(hash, key, T_STRING, NULL == fallback);

  return Qnil != value ? RSTRING_PTR(value) : fallback;
} /* }}} */

/* RubyGetFixnum {{{ */
static int
RubyGetFixnum(VALUE hash,
  char *key,
  int fallback)
{
  VALUE value = RubyFetchKey(hash, key, T_FIXNUM, 0 == fallback);

  return Qnil != value ? FIX2INT(value) : fallback;
} /* }}} */

/* RubyGetBool {{{ */
static int
RubyGetBool(VALUE hash,
  char *key)
{
  VALUE value = RubyFetchKey(hash, key, 0, False);

  return Qtrue == value ? True : (Qfalse == value ? False : -1);
} /* }}} */

/* RubyGetGeometry {{{ */
static int
RubyGetGeometry(VALUE hash,
  char *key,
  XRectangle *geometry)
{
  int ret = False;
  VALUE ary = Qnil;

  if(Qnil != (ary = RubyFetchKey(hash, key, T_ARRAY, True)))
    ret = RubyFetchGeometry(ary, geometry);

  return ret;
} /* }}} */

/* RubyGetGravity {{{ */
static int
RubyGetGravity(VALUE hash)
{
  int gravity = -1;
  VALUE value = RubyFetchKey(hash, "gravity", 0, True);

  /* Check value type */
  switch(rb_type(value))
    {
      case T_FIXNUM:
        gravity = FIX2INT(value);
        break;
      case T_SYMBOL:
        gravity = subGravityFind(SYM2CHAR(value), 0);
        break;
    }

  return 0 <= gravity && gravity < subtle->gravities->ndata ? gravity : -1;
} /* }}} */

/* Foreach */

/* RubyForeachGrab {{{ */
static int
RubyForeachGrab(VALUE key,
  VALUE value,
  VALUE extra)
{
  int i, type = -1;
  void *object = NULL;
  VALUE *rargs = (VALUE *)extra;
  SubData data = { None };

  switch(rb_type(value)) ///< Check value type
    {
      case T_SYMBOL: /* {{{ */
          {
            RubyGrabs *grabs = (RubyGrabs *)rargs[1];

            /* Find symbol and add flag */
            for(i = 0; GRABSLENGTH > i; i++)
              {
                if(value == grabs[i].sym)
                  {
                    type = grabs[i].flags;
                    data = DATA(grabs[i].extra);
                    break;
                  }
              }

            if(-1 == type) ///< Symbols with parameters
              {
                const char *name = SYM2CHAR(value);

                if(!strncmp(name, "ViewJump", 8)) ///< View jump
                  {
                    if((name = (char *)name + 8))
                      {
                        type = SUB_GRAB_VIEW_JUMP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "ScreenJump", 10)) ///< Screen jump
                  {
                    if((name = (char *)name + 10))
                      {
                        type = SUB_GRAB_SCREEN_JUMP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "WindowScreen", 12)) ///< Window screen
                  {
                    if((name = (char *)name + 12))
                      {
                        type = SUB_GRAB_WINDOW_SCREEN;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
              }
          }
        break; /* }}} */
      case T_ARRAY: /* {{{ */
          {
            int j = 0, id = -1, size = 0;
            VALUE entry = Qnil;
            char *gravities = NULL;

            /* Create gravity string */
            size      = FIX2INT(rb_funcall(value, rb_intern("size"), 0, NULL));
            gravities = (char *)subSharedMemoryAlloc(size + 1, sizeof(char));

            /* Add gravities */
            for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); i++)
              {
                if(T_SYMBOL == rb_type(entry))
                  {
                    /* We store ids in a string to ease the whole thing */
                    if(-1 != (id = subGravityFind(SYM2CHAR(entry), 0)))
                      gravities[j++] = id + 65; /// Use letters only
                    else subSharedLogWarn("Failed finding gravity `%s'\n", SYM2CHAR(entry));
                  }
              }

            /* Prevent empty grabs */
            if(0 < j)
              {
                type = SUB_GRAB_WINDOW_GRAVITY;
                data = DATA(gravities);
              }
          }
        break; /* }}} */
      case T_STRING: /* {{{ */
        type = SUB_GRAB_SPAWN;
        data = DATA(strdup(RSTRING_PTR(value)));
        break; /* }}} */
      case T_DATA: /* {{{ */
        type = SUB_GRAB_PROC;
        data = DATA(value);

        rb_ary_push(shelter, value); ///< Protect from GC
        break; /* }}} */
    }

  if(-1 != type && (object = (void *)subGrabNew(RSTRING_PTR(key), type, data)))
    subArrayPush(subtle->grabs, object);
  else subSharedLogWarn("Failed parsing grab `%s'\n", RSTRING_PTR(key));

  return Qnil;
} /* }}} */

/* RubyForeachGravity {{{ */
static int
RubyForeachGravity(VALUE key,
  VALUE value,
  VALUE extra)
{
  XRectangle geometry = { 0 };

  /* Check value type */
  if(T_SYMBOL == rb_type(key) && T_ARRAY == rb_type(value) &&
      RubyFetchGeometry(value, &geometry))
    {
      SubGravity *g = subGravityNew(SYM2CHAR(key), &geometry);

      subArrayPush(subtle->gravities, (void *)g);
    }
  else subSharedLogWarn("Failed parsing gravity `%s'\n", SYM2CHAR(key));

  return Qnil;
} /* }}} */

/* RubyForeachTag {{{ */
static int
RubyForeachTag(VALUE key,
  VALUE value,
  VALUE extra)
{
  /* Check key type */
  if(T_STRING == rb_type(key))
    {
      int i;
      SubTag *t = NULL;
      VALUE *rargs = (VALUE *)extra;

      switch(rb_type(value)) ///< Check value type
        {
          case T_STRING: /* {{{ */
            if((t = subTagNew(RSTRING_PTR(key), RSTRING_PTR(value))))
              {
                t->flags |= (SUB_TAG_MATCH_INSTANCE|SUB_TAG_MATCH_CLASS); ///< Set default matching
                subArrayPush(subtle->tags, (void *)t);
              }
            break; /* }}} */
          case T_HASH: /* {{{ */
            {
              if((t = subTagNew(RSTRING_PTR(key),
                  RubyGetString(value, "regex", NULL))))
                {
                  RubySymbols *tags = (RubySymbols *)rargs[1];

                  /* Collect flags */
                  for(i = 0; TAGSLENGTH > i; i++)
                    {
                      if(RTEST(rb_hash_lookup(value, tags[i].sym)))
                        t->flags |= tags[i].flags;
                    }

                  /* Check values */
                  if(t->flags & SUB_TAG_GRAVITY)
                    {
                      if(-1 == (t->gravity = RubyGetGravity(value)))
                        t->flags &= ~SUB_TAG_GRAVITY; ///< Sanity
                    }

                  if(t->flags & SUB_TAG_SCREEN)
                    {
                      t->screen = RubyGetFixnum(value, "screen",  0);
                      if(0 > t->screen ||
                          t->screen > subtle->screens->ndata - 1) ///< Sanity
                        t->flags &= ~SUB_TAG_SCREEN;
                    }

                  if(t->flags & SUB_TAG_GEOMETRY)
                    {
                      if(!RubyGetGeometry(value, "geometry", &t->geometry)) ///< Sanity
                        t->flags &= ~SUB_TAG_GEOMETRY;
                    }

                  /* Check tri-state properties */
                  if(t->flags & SUB_MODE_FULL &&
                      (False == RubyGetBool(value, "full")))
                    t->flags ^= (SUB_MODE_FULL|SUB_MODE_NONFULL);

                  if(t->flags & SUB_MODE_FLOAT &&
                      (False == RubyGetBool(value, "float")))
                    t->flags ^= (SUB_MODE_FLOAT|SUB_MODE_NONFLOAT);

                  if(t->flags & SUB_MODE_STICK &&
                      (False == RubyGetBool(value, "stick")))
                    t->flags ^= (SUB_MODE_STICK|SUB_MODE_NONSTICK);

                  if(t->flags & SUB_MODE_URGENT &&
                      (False == RubyGetBool(value, "urgent")))
                    t->flags ^= (SUB_MODE_URGENT|SUB_MODE_NONURGENT);

                  if(t->flags & SUB_MODE_RESIZE &&
                      (False == RubyGetBool(value, "resize")))
                    t->flags ^= (SUB_MODE_RESIZE|SUB_MODE_NONRESIZE);

                  /* Check matching properties */
                  if(t->flags & SUB_TAG_MATCH)
                    {
                      VALUE match = Qnil;

                      if(Qnil != (match = RubyFetchKey(value, "match",
                          T_ARRAY, True)))
                        {
                          if(Qtrue == rb_ary_includes(match, CHAR2SYM("name")))
                            t->flags |= SUB_TAG_MATCH_NAME;

                          if(Qtrue == rb_ary_includes(match, CHAR2SYM("instance")))
                            t->flags |= SUB_TAG_MATCH_INSTANCE;

                          if(Qtrue == rb_ary_includes(match, CHAR2SYM("class")))
                            t->flags |= SUB_TAG_MATCH_CLASS;

                          if(Qtrue == rb_ary_includes(match, CHAR2SYM("role")))
                            t->flags |= SUB_TAG_MATCH_ROLE;
                        }
                    }

                  /* Enable default if no matcher is present */
                  if(!(t->flags & (SUB_TAG_MATCH_NAME|SUB_TAG_MATCH_INSTANCE| \
                      SUB_TAG_MATCH_CLASS|SUB_TAG_MATCH_ROLE)))
                    t->flags |= (SUB_TAG_MATCH_INSTANCE|SUB_TAG_MATCH_CLASS);

                  subArrayPush(subtle->tags, (void *)t);
                }
              }
            break; /* }}} */
          default: /* {{{ */
            subSharedLogWarn("Failed parsing value of tag `%s'\n", RSTRING_PTR(key));
            return Qnil; /* }}} */
        }
    }
  else subSharedLogWarn("Failed parsing tag name `%s'\n", RSTRING_PTR(key));

  return Qnil;
} /* }}} */

/* RubyForeachView {{{ */
static int
RubyForeachView(VALUE key,
  VALUE value,
  VALUE extra)
{
  /* Check key type */
  if(T_STRING == rb_type(key))
    {
      SubView *v = NULL;

      /* Check value type */
      switch(rb_type(value))
        {
          case T_STRING: /* {{{ */
            if((v = subViewNew(RSTRING_PTR(key), RSTRING_PTR(value))))
              subArrayPush(subtle->views, (void *)v);
            break; /* }}} */
          case T_HASH: /* {{{ */
            if((v = subViewNew(RSTRING_PTR(key),
                RubyGetString(value, "regex", NULL))))
              {
                if(True == RubyGetBool(value, "dynamic"))
                  v->flags |= (SUB_VIEW_DYNAMIC|SUB_PANEL_HIDDEN);

                subArrayPush(subtle->views, (void *)v);
              }

            break; /* }}} */
          default: /* {{{ */
            subSharedLogWarn("Failed parsing value of view `%s'\n", RSTRING_PTR(key));
            return Qnil; /* }}} */
        }
    }
  else subSharedLogWarn("Failed parsing view name `%s'\n", RSTRING_PTR(key));

  return Qnil;
} /* }}} */

/* RubyForeachHook {{{ */
static int
RubyForeachHook(VALUE key,
  VALUE value,
  VALUE extra)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(key) && T_DATA == rb_type(value))
    {
      int i;
      SubHook *h = NULL;
      VALUE *rargs = (VALUE *)extra;
      RubySymbols *hooks = (RubySymbols *)rargs[1];

      for(i = 0; HOOKSLENGTH > i; i++)
        {
          if(key == hooks[i].sym)
            {
              h = subHookNew(hooks[i].flags|SUB_CALL_HOOKS,
                value, NULL);

              subArrayPush(subtle->hooks, (void *)h);
              rb_ary_push(shelter, value); ///< Protect from GC

              subSharedLogDebug("hook=%s\n", SYM2CHAR(hooks[i].sym));
              break;
            }
        }
    }
  else subSharedLogWarn("Failed parsing hook `%s'\n", SYM2CHAR(key));

  return Qnil;
} /* }}} */

/* RubyForeachPanel {{{ */
int
RubyForeachPanel(VALUE hash,
  char *key,
  RubyPanels *panels)
{
  int enabled = False;
  VALUE value = Qnil;

  /* Check value type */
  if(T_ARRAY == rb_type(value = RubyFetchKey(hash, key, T_ARRAY, False)))
    {
      int i, j, flags = 0;
      Window panel = subtle->windows.panel1;
      SubPanel *p = NULL;
      VALUE entry = Qnil, spacer = Qnil, separator, sublets = Qnil;

      /* Get syms */
      spacer    = CHAR2SYM("spacer");
      separator = CHAR2SYM("separator");
      sublets   = CHAR2SYM("sublets");

      /* Check position of panel */
      if(!strncmp(key, "bottom", 6))
        {
          flags |= SUB_PANEL_BOTTOM;
          panel  = subtle->windows.panel2;
        }

      /* Parse array */
      for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); ++i)
        {
          void *item = NULL;

          if(entry == spacer)
            {
              /* Add separator after panel item */
              if(p && flags & SUB_PANEL_SEPARATOR1)
                {
                  p->flags |= SUB_PANEL_SEPARATOR2;
                  flags    &= ~SUB_PANEL_SEPARATOR1;
                }

              flags |= SUB_PANEL_SPACER1;
            }
          else if(entry == separator)
            {
              /* Add spacer after panel item */
              if(p && flags & SUB_PANEL_SEPARATOR1)
                p->flags |= SUB_PANEL_SEPARATOR2;

              flags |= SUB_PANEL_SEPARATOR1;
            }
          else if(entry == sublets)
            {
              if(0 < subtle->sublets->ndata)
                {
                  /* Add dummy panel as entry point for sublets */
                  flags |= SUB_PANEL_SUBLETS;
                  item   = subSharedMemoryAlloc(1, sizeof(SubPanel));
                }
            }
          else
            {
              /* Check for real panels */
              for(j = 0; !item && PANELSLENGTH > j; j++)
                {
                  if(entry == panels[j].sym)
                    {
                      panels[j].panel->flags |= SUB_TYPE_PANEL;
                      item                    = (void *)panels[j].panel;
                    }
                }

              /* Check for sublets */
              for(j = 0; !item && j < subtle->sublets->ndata; j++)
                {
                  SubSublet *s = SUBLET(subtle->sublets->data[j]);

                  if(entry == CHAR2SYM(s->name))
                    {
                      s->flags |= SUB_SUBLET_PANEL;
                      item      = (void *)s;
                    }
                }
            }

          /* Add to panel list */
          if(item)
            {
              p         = PANEL(item);
              p->flags |= flags;
              flags     = 0;
              enabled   = True; ///< Enable this panel

              subArrayPush(subtle->panels, item);
              XReparentWindow(subtle->dpy, p->win, panel, 0, 0);
            }
        }

      /* Add stuff to last item */
      if(p && flags & SUB_PANEL_SPACER1)    p->flags |= SUB_PANEL_SPACER2;
      if(p && flags & SUB_PANEL_SEPARATOR1) p->flags |= SUB_PANEL_SEPARATOR2;
    }

  return enabled;
} /* }}} */

/* Wrap */

/* RubyWrapLoadConfig {{{ */
static VALUE
RubyWrapLoadConfig(VALUE data)
{
  int i;
  char *str = NULL;
  VALUE config = Qnil, entry = Qnil, rargs[2] = { 0 };

  /* Foreach arguments {{{ */
  RubyGrabs grabs[] =
  {
    { CHAR2SYM("SubletsReload"), SUB_GRAB_SUBLETS_RELOAD, None             },
    { CHAR2SYM("SubtleReload"),  SUB_GRAB_SUBTLE_RELOAD,  None             },
    { CHAR2SYM("SubtleQuit"),    SUB_GRAB_SUBTLE_QUIT,    None             },
    { CHAR2SYM("WindowMove"),    SUB_GRAB_WINDOW_MOVE,    None             },
    { CHAR2SYM("WindowResize"),  SUB_GRAB_WINDOW_RESIZE,  None             },
    { CHAR2SYM("WindowFloat"),   SUB_GRAB_WINDOW_TOGGLE,  SUB_MODE_FLOAT   },
    { CHAR2SYM("WindowFull"),    SUB_GRAB_WINDOW_TOGGLE,  SUB_MODE_FULL    },
    { CHAR2SYM("WindowStick"),   SUB_GRAB_WINDOW_TOGGLE,  SUB_MODE_STICK   },
    { CHAR2SYM("WindowRaise"),   SUB_GRAB_WINDOW_STACK,   Above            },
    { CHAR2SYM("WindowLower"),   SUB_GRAB_WINDOW_STACK,   Below            },
    { CHAR2SYM("WindowLeft"),    SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_LEFT  },
    { CHAR2SYM("WindowDown"),    SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_DOWN  },
    { CHAR2SYM("WindowUp"),      SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_UP    },
    { CHAR2SYM("WindowRight"),   SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_RIGHT },
    { CHAR2SYM("WindowKill"),    SUB_GRAB_WINDOW_KILL,    None             },
  };

  RubySymbols tags[] =
  {
    { CHAR2SYM("gravity"),  SUB_TAG_GRAVITY  },
    { CHAR2SYM("screen"),   SUB_TAG_SCREEN   },
    { CHAR2SYM("geometry"), SUB_TAG_GEOMETRY },
    { CHAR2SYM("full"),     SUB_MODE_FULL    },
    { CHAR2SYM("float"),    SUB_MODE_FLOAT   },
    { CHAR2SYM("stick"),    SUB_MODE_STICK   },
    { CHAR2SYM("urgent"),   SUB_MODE_URGENT  },
    { CHAR2SYM("resize"),   SUB_MODE_RESIZE  },
    { CHAR2SYM("match"),    SUB_TAG_MATCH    }
  };

  RubySymbols hooks[] =
  {
    { CHAR2SYM("HookStart"),           SUB_HOOK_START            },
    { CHAR2SYM("HookReload"),          SUB_HOOK_RELOAD           },
    { CHAR2SYM("HookExit"),            SUB_HOOK_EXIT             },
    { CHAR2SYM("HookClientCreate"),    SUB_HOOK_CLIENT_CREATE    },
    { CHAR2SYM("HookClientConfigure"), SUB_HOOK_CLIENT_CONFIGURE },
    { CHAR2SYM("HookClientFocus"),     SUB_HOOK_CLIENT_FOCUS     },
    { CHAR2SYM("HookClientKill"),      SUB_HOOK_CLIENT_KILL      },
    { CHAR2SYM("HookViewCreate"),      SUB_HOOK_VIEW_CREATE      },
    { CHAR2SYM("HookViewConfigure"),   SUB_HOOK_VIEW_CONFIGURE   },
    { CHAR2SYM("HookViewJump"),        SUB_HOOK_VIEW_JUMP        },
    { CHAR2SYM("HookViewKill"),        SUB_HOOK_VIEW_KILL        },
    { CHAR2SYM("HookTagCreate"),       SUB_HOOK_TAG_CREATE       },
    { CHAR2SYM("HookTagKill"),         SUB_HOOK_TAG_KILL         }
  };
  /* }}} */

  /* Gravities need to be parsed first */
  config   = rb_const_get(rb_cObject, rb_intern("GRAVITIES"));
  rargs[0] = SUB_TYPE_GRAVITY;

  rb_hash_foreach(config, RubyForeachGravity, (VALUE)&rargs);

  if(1 == subtle->gravities->ndata)
    {
      subSharedLogError("No gravities found\n");
      subEventFinish();
    }

  subGravityPublish();

  /* Config: Options */
  config = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw    = RubyGetFixnum(config, "border", 2);
  subtle->step  = RubyGetFixnum(config, "step",   5);
  subtle->snap  = RubyGetFixnum(config, "snap",   10);
  subtle->limit = RubyGetFixnum(config, "limit",  EXECTIME);
  RubyGetGeometry(config, "padding", &subtle->strut);
  if(-1 == (subtle->gravity = RubyGetGravity(config))) subtle->gravity = 0;
  if(True == RubyGetBool(config, "urgent")) subtle->flags |= SUB_SUBTLE_URGENT;
  if(True == RubyGetBool(config, "resize")) subtle->flags |= SUB_SUBTLE_RESIZE;

  /* Config: Font */
  str = RubyGetString(config, "font", NULL);
  if(!(subtle->font = subSharedFontNew(subtle->dpy, str))) subEventFinish();
  subtle->th = subtle->font->height;

  /* Config: Colors */
  config = rb_const_get(rb_cObject, rb_intern("COLORS"));
  subtle->colors.fg_panel   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "fg_panel",      "#757575"));
  subtle->colors.fg_views   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "fg_views",      "#757575"));
  subtle->colors.fg_sublets = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "fg_sublets",    "#757575"));
  subtle->colors.fg_focus   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "fg_focus",      "#fecf35"));
  subtle->colors.fg_urgent  = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "fg_urgent",     "#FF9800"));
  subtle->colors.bg_panel   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "bg_panel",      "#202020"));
  subtle->colors.bg_views   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "bg_views",      "#202020"));
  subtle->colors.bg_sublets = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "bg_sublets",    "#202020"));
  subtle->colors.bg_focus   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "bg_focus",      "#202020"));
  subtle->colors.bg_urgent  = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "bg_urgent",     "#202020"));
  subtle->colors.bo_focus   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "border_focus",  "#303030"));
  subtle->colors.bo_normal  = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "border_normal", "#202020"));
  subtle->colors.bo_panel   = subSharedParseColor(subtle->dpy,
    RubyGetString(config, "border_panel", "#202020"));

  /* Root background */
  if((str = RubyGetString(config, "background", NULL)))
    {
      subtle->flags     |= SUB_SUBTLE_BACKGROUND;
      subtle->colors.bg  = subSharedParseColor(subtle->dpy, str);
    }

  /* Config: Grabs */
  config   = rb_const_get(rb_cObject, rb_intern("GRABS"));
  rargs[0] = SUB_TYPE_GRAB;
  rargs[1] = (VALUE)&grabs;

  rb_hash_foreach(config, RubyForeachGrab, (VALUE)&rargs);

  if(0 == subtle->grabs->ndata)
    {
      subSharedLogWarn("No grabs found\n");
    }
  else subArraySort(subtle->grabs, subGrabCompare);

  /* Config: Tags */
  config   = rb_const_get(rb_cObject, rb_intern("TAGS"));
  rargs[0] = SUB_TYPE_TAG;
  rargs[1] = (VALUE)&tags;

  /* Check default tag */
  if(T_HASH != rb_type(config) ||
      Qtrue != rb_funcall(config, rb_intern("has_key?"), 1, rb_str_new2("default")))
    {
      SubTag *t = subTagNew("default", NULL); ///< Add default tag

      subArrayPush(subtle->tags, (void *)t);
    }
  else ///< Fetch default tag
    {
      VALUE key = Qnil, value = Qnil;

      key   = rb_str_new2("default");
      value = rb_funcall(config, rb_intern("delete"), 1, key);

      RubyForeachTag(key, value, (VALUE)&rargs);
    }

  if(T_HASH == rb_type(config)) ///< Parse tags hash
    rb_hash_foreach(config, RubyForeachTag, (VALUE)&rargs);

  subTagPublish();

  if(1 == subtle->tags->ndata) subSharedLogWarn("No tags found\n");

  /* Config: Views */
  config   = rb_const_get(rb_cObject, rb_intern("VIEWS"));
  rargs[0] = SUB_TYPE_VIEW;

  switch(rb_type(config)) ///< Allow hashes or arrays for ordering
    {
      case T_HASH: rb_hash_foreach(config, RubyForeachView, (VALUE)&rargs); break;
      case T_ARRAY:
        for(i = 0; Qnil != (entry = rb_ary_entry(config, i)); ++i)
          {
            if(T_HASH == rb_type(entry))
              rb_hash_foreach(entry, RubyForeachView, (VALUE)&rargs);
            else subSharedLogWarn("Failed parsing view entry %d\n", i);
          }
        break;
      default: break;
    }

  /* Views */
  if(0 == subtle->views->ndata) ///< Create default view
    {
      SubView *v = subViewNew("subtle", "default");

      subArrayPush(subtle->views, (void *)v);
      subSharedLogWarn("No views found\n");
    }
  else ///< Check default tag
    {
      SubView *v = NULL;

      /* Check for view with default tag */
      for(i = subtle->views->ndata - 1; 0 <= i; i--)
        if((v = VIEW(subtle->views->data[i])) && v->tags & DEFAULTTAG)
          {
            subSharedLogDebug("Default view: name=%s\n", v->name);
            break;
          }

      v->tags |= DEFAULTTAG; ///< Set default tag

      /* EWMH: Tags */
      subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1);
    }

  subViewUpdate();
  subViewPublish();

  /* Config: Hooks */
  config   = rb_const_get(rb_cObject, rb_intern("HOOKS"));
  rargs[0] = SUB_TYPE_HOOK;
  rargs[1] = (VALUE)&hooks;

  rb_hash_foreach(config, RubyForeachHook, (VALUE)&rargs);

  return Qnil;
} /* }}} */

/* RubyWrapLoadSubtlext {{{ */
static VALUE
RubyWrapLoadSubtlext(VALUE data)
{
  /* Load subtlext */
  if(NIL_P(subtlext))
    {
      rb_require("subtle/subtlext");

      subtlext = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

      printf("Loaded subtlext\n");
    }

  return Qnil;
}/* }}} */

/* RubyWrapLoadPanels {{{ */
static VALUE
RubyWrapLoadPanels(VALUE data)
{
  int i, j, pos;
  VALUE config = Qnil;
  Window panel = subtle->windows.panel1;
  RubyPanels panels[] =
  {
    { CHAR2SYM("views"), &subtle->windows.views },
    { CHAR2SYM("title"), &subtle->windows.title },
    { CHAR2SYM("tray"),  &subtle->windows.tray  }
  };
  /* Config: Panels */

  config = rb_const_get(rb_cObject, rb_intern("PANEL"));
  if(RubyForeachPanel(config, "top", panels))
    subtle->flags |= SUB_SUBTLE_PANEL1;
  if(RubyForeachPanel(config, "bottom", panels))
    subtle->flags |= SUB_SUBTLE_PANEL2;
  if(True == RubyGetBool(config, "stipple"))
    subtle->flags |= SUB_SUBTLE_STIPPLE;
   subtle->pbw = RubyGetFixnum(config, "border", 0);

  /* Separator */
  if(subtle->separator.string) free(subtle->separator.string);
  subtle->separator.string = strdup(RubyGetString(config, "separator", "|"));
  subtle->separator.width  = subSharedTextWidth(subtle->font,
    subtle->separator.string, strlen(subtle->separator.string),
    NULL, NULL, True);

  /* Update sizes */
  if(0 < subtle->separator.width) subtle->separator.width += 6; ///< Add spacings
  if(0 < subtle->pbw)
    {
      subtle->th = subtle->font->height + 2 * subtle->pbw;
      subDisplayConfigure();

      subViewUpdate();
      subSubletUpdate();
    }

  /* Update panel border */
  for(i = 0; i < subtle->views->ndata; i++)
    {
      SubView *v = VIEW(subtle->views->data[i]);

      XSetWindowBorder(subtle->dpy, v->button, subtle->colors.bo_panel);
      XSetWindowBorderWidth(subtle->dpy, v->button, subtle->pbw);
    }

  /* Update title border */
  XSetWindowBorder(subtle->dpy, subtle->windows.title.win, subtle->colors.bo_panel);
  XSetWindowBorderWidth(subtle->dpy, subtle->windows.title.win, subtle->pbw);

  /* Add remaining sublets if any */
  if(0 < subtle->sublets->ndata)
    {
      for(i = 0; i < subtle->panels->ndata; i++)
        {
          SubPanel *p = PANEL(subtle->panels->data[i]);

          /* Set borders */
          XSetWindowBorder(subtle->dpy, p->win, subtle->colors.bo_panel);
          XSetWindowBorderWidth(subtle->dpy, p->win, subtle->pbw);

          if(p->flags & SUB_PANEL_BOTTOM) panel = subtle->windows.panel2;
          if(p->flags & SUB_PANEL_SUBLETS)
            {
              SubSublet *s = NULL;

              pos = i;

              subArrayRemove(subtle->panels, (void *)p);

              /* Find panel sublets */
              for(j = 0; j < subtle->sublets->ndata; j++)
                {
                  s = SUBLET(subtle->sublets->data[j]);

                  if(!(s->flags & SUB_SUBLET_PANEL))
                    {
                      s->flags |= SUB_PANEL_SEPARATOR2;

                      subArrayInsert(subtle->panels, pos++, (void *)s);
                      XReparentWindow(subtle->dpy, s->win, panel, 0, 0);

                      /* Set borders */
                      XSetWindowBorder(subtle->dpy, s->win, subtle->colors.bo_panel);
                      XSetWindowBorderWidth(subtle->dpy, s->win, subtle->pbw);
                    }
                }

              /* Check spacers and separators in first and last sublet */
              if((s = SUBLET(subArrayGet(subtle->panels, i))))
                s->flags |= (p->flags &
                  (SUB_PANEL_BOTTOM|SUB_PANEL_SPACER1|SUB_PANEL_SEPARATOR1));

              if((s = SUBLET(subArrayGet(subtle->panels, pos - 1))))
                {
                  s->flags |= (p->flags & SUB_PANEL_SPACER2);
                  if(!(p->flags & SUB_PANEL_SEPARATOR2))
                    s->flags &= ~SUB_PANEL_SEPARATOR2;
                }

              free(p);

              break;
            }
        }

      /* Finally sort and update sublets */
      subArraySort(subtle->sublets, subSubletCompare);
      subSubletUpdate();
    }
  subSubletPublish();

  return Qnil;
} /* }}} */

/* RubyWrapCall {{{ */
static VALUE
RubyWrapCall(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  /* Check call type */
  if((int)rargs[0] & SUB_CALL_HOOKS)
    {
      rb_funcall(rargs[1], rb_intern("call"),
        MINMAX(rb_proc_arity(rargs[1]), 0, 1), RubyConvert((VALUE *)rargs[3]));
    }
  else if((int)rargs[0] & SUB_CALL_SUBLET_HOOKS)
    {
      SubSublet *s = SUBLET(rargs[2]);

      rb_funcall(rargs[1], rb_intern("call"),
        MINMAX(rb_proc_arity(rargs[1]), 1, 2), s->instance,
        RubyConvert((VALUE *)rargs[3]));
    }
  else
    {
      /* Check call type */
      switch((int)rargs[0])
        {
          case SUB_CALL_SUBLET_CONFIGURE: /* {{{ */
            rb_funcall(rargs[1], rb_intern("__configure"), 1, rargs[1]);
            break; /* }}} */
          case SUB_CALL_SUBLET_RUN: /* {{{ */
            rb_funcall(rargs[1], rb_intern("__run"), 1, rargs[1]);
            break; /* }}} */
          case SUB_CALL_SUBLET_WATCH: /* {{{ */
            rb_funcall(rargs[1], rb_intern("__watch"), 1, rargs[1]);
            break; /* }}} */
          case SUB_CALL_SUBLET_OVER: /* {{{ */
            rb_funcall(rargs[1], rb_intern("__over"), 1, rargs[1]);
            break; /* }}} */
          case SUB_CALL_SUBLET_OUT: /* {{{ */
            rb_funcall(rargs[1], rb_intern("__out"), 1, rargs[1]);
            break; /* }}} */
          case SUB_CALL_SUBLET_DOWN: /* {{{ */
              {
                XButtonEvent *ev = (XButtonEvent *)rargs[2];

                rb_funcall(rargs[1], rb_intern("__down"), 4, rargs[1],
                  INT2FIX(ev->x), INT2FIX(ev->y), INT2FIX(ev->button));
              }
            break; /* }}} */
        }
    }

  return Qnil;
} /* }}} */

/* RubyWrapRemove {{{ */
static VALUE
RubyWrapRemove(VALUE name)
{
  VALUE object = rb_const_get(rb_mKernel, rb_intern("Object"));

  return rb_funcall(object, rb_intern("send"), 2, rb_str_new2("remove_const"), name);
} /* }}} */

/* RubyWrapRelease {{{ */
static VALUE
RubyWrapRelease(VALUE value)
{
  if(Qtrue == rb_funcall(shelter, rb_intern("include?"), 1, value))
    rb_funcall(shelter, rb_intern("delete"), 1, value);

  return Qnil;
} /* }}} */

/* RubyWrapRead {{{ */
static VALUE
RubyWrapRead(VALUE file)
{
  VALUE str = rb_funcall(rb_cFile, rb_intern("read"), 1, file);

  return str;
} /* }}} */

/* Kernel */

/* RubyKernelConfigure {{{ */
/*
 * call-seq: configure -> nil
 *
 * Configure block for Sublet
 *
 *  configure :sublet do |s|
 *    s.interval = 60
 *  end
 */

static VALUE
RubyKernelConfigure(VALUE self,
  VALUE name)
{
  rb_need_block();

  if(T_SYMBOL == rb_type(name))
    {
      SubSublet *s = NULL;
      VALUE p = Qnil;

      /* Assume latest sublet */
      p = rb_block_proc();
      s = SUBLET(subtle->sublets->data[subtle->sublets->ndata - 1]);
      s->name = strdup(SYM2CHAR(name));

      /* Define configure method */
      rb_funcall(rb_singleton_class(s->instance), rb_intern("define_method"),
        2, CHAR2SYM("__configure"), p);
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* RubyKernelEvent {{{ */
/*
 * call-seq: on(event, &block) -> nil
 *
 * Event block for Sublet
 *
 *  on :event do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubyKernelEvent(VALUE self,
  VALUE event)
{
  rb_need_block();

  if(0 < subtle->sublets->ndata)
    {
      if(T_SYMBOL == rb_type(event))
        {
          int i, arity = 0, mask = 0;
          SubSublet *s = NULL;
          VALUE p = Qnil, sing = Qnil, meth = Qnil;

          RubyMethods methods[] =
          {
            { CHAR2SYM("run"),        CHAR2SYM("__run"),   SUB_SUBLET_RUN,   1 },
            { CHAR2SYM("watch"),      CHAR2SYM("__watch"), SUB_SUBLET_WATCH, 1 },
            { CHAR2SYM("mouse_down"), CHAR2SYM("__down"),  SUB_SUBLET_DOWN,  4 },
            { CHAR2SYM("mouse_over"), CHAR2SYM("__over"),  SUB_SUBLET_OVER,  1 },
            { CHAR2SYM("mouse_out"),  CHAR2SYM("__out"),   SUB_SUBLET_OUT,   1 }
          };

          RubySymbols hooks[] =
          {
            { CHAR2SYM("exit"),             SUB_HOOK_EXIT             },
            { CHAR2SYM("client_create"),    SUB_HOOK_CLIENT_CREATE    },
            { CHAR2SYM("client_configure"), SUB_HOOK_CLIENT_CONFIGURE },
            { CHAR2SYM("client_focus"),     SUB_HOOK_CLIENT_FOCUS     },
            { CHAR2SYM("client_kill"),      SUB_HOOK_CLIENT_KILL      },
            { CHAR2SYM("tag_create"),       SUB_HOOK_TAG_CREATE       },
            { CHAR2SYM("tag_kill"),         SUB_HOOK_TAG_KILL         },
            { CHAR2SYM("view_create"),      SUB_HOOK_VIEW_CREATE      },
            { CHAR2SYM("view_configure"),   SUB_HOOK_VIEW_CONFIGURE   },
            { CHAR2SYM("view_jump"),        SUB_HOOK_VIEW_JUMP        },
            { CHAR2SYM("view_kill"),        SUB_HOOK_VIEW_KILL        }
          };

          /* Since loading is linear we use the last sublet */
          s     = SUBLET(subtle->sublets->data[subtle->sublets->ndata - 1]);
          p     = rb_block_proc();
          arity = rb_proc_arity(p);
          sing  = rb_singleton_class(s->instance);
          meth  = rb_intern("define_method");

          /* Special hooks */
          for(i = 0; LENGTH(methods) > i; i++)
            {
              if(methods[i].sym == event)
                {
                  /* Check proc arity */
                  if(-1 == arity || (1 <= arity && methods[i].arity >= arity))
                    {
                      s->flags |= methods[i].flags;

                      /* Create instance method from proc */
                      rb_funcall(sing, meth, 2, methods[i].real, p);
                    }
                  else rb_raise(rb_eArgError, "Wrong number of arguments (%d for %d)",
                    arity, methods[i].arity);
                }
            }

          /* Generic hooks */
          for(i = 0; LENGTH(hooks) > i; i++)
            {
              if(hooks[i].sym == event)
                {
                  SubHook *h = subHookNew(hooks[i].flags|SUB_CALL_SUBLET_HOOKS,
                    p, (void *)s);

                  /* Add hook to the hooks array */
                  subArrayPush(subtle->hooks, (void *)h);
                  rb_ary_push(shelter, p); ///< Protect from GC
                }
            }

          /* Hook specific stuff */
          if(s->flags & SUB_SUBLET_DOWN) mask |= ButtonPressMask;
          if(s->flags & SUB_SUBLET_OVER) mask |= EnterWindowMask;
          if(s->flags & SUB_SUBLET_OUT)  mask |= LeaveWindowMask;

          XSelectInput(subtle->dpy, s->win, mask);
        }
      else rb_raise(rb_eArgError, "Unknown value type");
    }

  return Qnil;
} /* }}} */

/* RubyKernelHelper {{{ */
/*
 * call-seq: helper -> nil
 *
 * Helper block for Sublet
 *
 *  helper do |s|
 *    def test
 *      puts "test"
 *    end
 *  end
 */

static VALUE
RubyKernelHelper(VALUE self)
{
  rb_need_block();

  if(0 < subtle->sublets->ndata)
    {
      SubSublet *s = NULL;

      /* Since loading is linear we use the last sublet */
      s = SUBLET(subtle->sublets->data[subtle->sublets->ndata - 1]);

      /* Instance eval the block */
      rb_yield_values(1, s->instance);
      rb_obj_instance_eval(0, 0, s->instance);
    }

  return Qnil;
} /* }}} */

/* Object */

/* RubyObjectDispatcher {{{ */
/*
 * Dispatcher for Subtlext constants - internal use only
 */

static VALUE
RubyObjectDispatcher(VALUE self,
  VALUE missing)
{
  ID id = Qnil;
  VALUE ret = Qnil;
  char *name = NULL;

  id   = SYM2ID(missing);
  name = (char *)rb_id2name(id);

  subSharedLogDebug("Missing: constant=%s\n", name);

  subRubyLoadSubtlext(); ///< Load subtlext on demand

  /* Check if subtlext has this symbol */
  if(rb_const_defined(rb_mKernel, id))
    ret = rb_const_get(rb_mKernel, id);
  else rb_raise(rb_eStandardError, "Failed finding constant `%s'", name);

  return ret;
} /* }}} */

/* Sublet */

/* RubySubletDispatcher {{{ */
/*
 * Dispatcher for Sublet instance variables - internal use only
 */

static VALUE
RubySubletDispatcher(int argc,
  VALUE *argv,
  VALUE self)
{
  char *name = NULL;
  VALUE missing = Qnil, args = Qnil, ret = Qnil;

  rb_scan_args(argc, argv, "1*", &missing, &args);

  name = (char *)rb_id2name(SYM2ID(missing));

  subSharedLogDebug("Dispatcher: missing=%s, args=%d\n", name, argc);

  /* Dispatch calls */
  if(rb_respond_to(self, rb_to_id(missing))) ///< Intance methods
    ret = rb_funcall2(self, rb_to_id(missing), --argc, ++argv);
  else ///< Instance variables
    {
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "@%s", name);

      if(index(name, '='))
        {
          int len = 0;
          VALUE value = Qnil;

          value    = rb_ary_entry(args, 0); ///< Get first arg
          len      = strlen(name);
          buf[len] = '\0'; ///< Overwrite equal sign

          rb_funcall(self, rb_intern("instance_variable_set"), 2, CHAR2SYM(buf), value);
        }
      else ret = rb_funcall(self, rb_intern("instance_variable_get"), 1, CHAR2SYM(buf));
    }

  return ret;
} /* }}} */

/* RubySubletRender {{{ */
/*
 * call-seq: render -> nil
 *
 * Render a Sublet
 *
 *  sublet.render
 *  => nil
 */

static VALUE
RubySubletRender(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  if(s) subSubletRender(s);

  return Qnil;
} /* }}} */

/* RubySubletNameReader {{{ */
/*
 * call-seq: name -> String
 *
 * Get name of Sublet
 *
 *  puts sublet.name
 *  => "sublet"
 */

static VALUE
RubySubletNameReader(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  return s ? rb_str_new2(s->name) : Qnil;
} /* }}} */

/* RubySubletIntervalReader {{{ */
/*
 * call-seq: interval -> Fixnum
 *
 * Get interval time of Sublet
 *
 *  puts sublet.interval
 *  => 60
 */

static VALUE
RubySubletIntervalReader(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  return s ? INT2FIX(s->interval) : Qnil;
} /* }}} */

/* RubySubletIntervalWriter {{{ */
/*
 * call-seq: interval=(fixnum) -> nil
 *
 * Set interval time of Sublet
 *
 *  sublet.interval = 60
 *  => nil
 */

static VALUE
RubySubletIntervalWriter(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  if(s && FIXNUM_P(value))
    {
      s->interval = FIX2INT(value);
      s->time     = subSubtleTime() + s->interval;

      if(0 < s->interval) s->flags |= SUB_SUBLET_INTERVAL;
      else s->flags &= ~SUB_SUBLET_INTERVAL;
    }
  else rb_raise(rb_eArgError, "Unknown value type `%s'", rb_obj_classname(value));

  return Qnil;
} /* }}} */

/* RubySubletDataReader {{{ */
/*
 * call-seq: data -> String or nil
 *
 * Get data of Sublet
 *
 *  puts sublet.data
 *  => "subtle"
 */

static VALUE
RubySubletDataReader(VALUE self)
{
  int i;
  VALUE string = Qnil;
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  /* Concat string */
  if(s && 0 < s->text->nitems)
    {
      /* Assemble string */
      for(i = 0; i < s->text->nitems; i++)
        {
          SubTextItem *item = (SubTextItem *)s->text->items[i];

          if(Qnil == string) rb_str_new2(item->data.string);
          else rb_str_cat(string, item->data.string, strlen(item->data.string));
        }
    }

  return string;
} /* }}} */

/* RubySubletDataWriter {{{ */
/*
 * call-seq: data=(string) -> nil
 *
 * Set data of Sublet
 *
 *  sublet.data = "subtle"
 *  => nil
 */

static VALUE
RubySubletDataWriter(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s && T_STRING == rb_type(value)) ///< Check value type
    {
      s->width = subSharedTextParse(subtle->dpy, subtle->font,
        s->text, RSTRING_PTR(value));
      subSubletRender(s);
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* RubySubletBackgroundWriter {{{ */
/*
 * call-seq: background=(value) -> nil
 *
 * Set the background color of a Sublet
 *
 *  sublet.background("#ffffff")
 *  => nil
 *
 *  sublet.background(Sublet::Color.new("#ffffff"))
 *  => nil
 */

static VALUE
RubySubletBackgroundWriter(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);
  if(s)
    {
      s->bg = subtle->colors.bg_sublets; ///< Set default color

      /* Check value type */
      switch(rb_type(value))
        {
          case T_STRING:
            s->bg = subSharedParseColor(subtle->dpy, RSTRING_PTR(value));
            break;
          case T_OBJECT:
              {
                VALUE mod = Qnil, klass = Qnil;

                mod   = rb_const_get(rb_mKernel, rb_intern("Subtle"));
                klass = rb_const_get(mod, rb_intern("Color"));

                if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
                  s->bg = NUM2LONG(rb_iv_get(self, "@pixel"));
              }
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }

      subSubletRender(s);
    }

  return Qnil;
} /* }}} */

/* RubySubletShow {{{ */
/*
 * call-seq: show -> nil
 *
 * Show sublet
 *
 *  sublet.show
 *  => nil
 */

static VALUE
RubySubletShow(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  if(s)
    {
      s->flags &= ~SUB_PANEL_HIDDEN;

      /* Update panels */
      subPanelUpdate();
      subPanelRender();
    }

  return Qnil;
} /* }}} */

/* RubySubletHide {{{ */
/*
 * call-seq: hide -> nil
 *
 * Hide sublet
 *
 *  sublet.hide
 *  => nil
 */

static VALUE
RubySubletHide(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  if(s)
    {
      s->flags |= SUB_PANEL_HIDDEN;
      XUnmapWindow(subtle->dpy, s->win);

      /* Update panels */
      subPanelUpdate();
      subPanelRender();
    }

  return Qnil;
} /* }}} */

/* RubySubletHidden {{{ */
/*
 * call-seq: hidden? -> Bool
 *
 * Whether sublet is displayed
 *
 *  puts sublet.display
 *  => true
 */

static VALUE
RubySubletHidden(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  return s && s->flags & SUB_PANEL_HIDDEN ? Qtrue : Qfalse;
} /* }}} */

/* RubySubletWatch {{{ */
/*
 * call-seq: watch(source) -> true or false
 *
 * Add watch file via inotify or socket
 *
 *  watch "/path/to/file"
 *  => true
 *
 *  @socket = TCPSocket("localhost", 6600)
 *  watch @socket
 */

static VALUE
RubySubletWatch(VALUE self,
  VALUE value)
{
  int flags = 0;
  VALUE ret = Qfalse;
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s && !(s->flags & (SUB_SUBLET_SOCKET|SUB_SUBLET_INOTIFY)) && RTEST(value))
    {
      if(FIXNUM_P(value) || ///< Socket file descriptor
          rb_respond_to(value, rb_intern("fileno"))) ///< Ruby socket
        {
          s->flags |= SUB_SUBLET_SOCKET;

          /* Get socket file descriptor */
          if(FIXNUM_P(value)) s->watch = FIX2INT(value);
          else
            {
              s->watch = FIX2INT(rb_funcall(value, rb_intern("fileno"),
                0, NULL));
            }

          XSaveContext(subtle->dpy, subtle->windows.panel1, s->watch, (void *)s);
          subEventWatchAdd(s->watch);

          /* Set nonblocking */
          if(-1 == (flags = fcntl(s->watch, F_GETFL, 0))) flags = 0;
          fcntl(s->watch, F_SETFL, flags | O_NONBLOCK);

          ret = Qtrue;
        }
#ifdef HAVE_SYS_INOTIFY_H
      else if(T_STRING == rb_type(value)) /// Inotify file
        {
          char *watch = RSTRING_PTR(value);

          /* Create inotify watch */
          if(0 < (s->watch = inotify_add_watch(subtle->notify, watch, IN_MODIFY)))
            {
              s->flags |= SUB_SUBLET_INOTIFY;

              XSaveContext(subtle->dpy, subtle->windows.panel1, s->watch, (void *)s);
              subSharedLogDebug("Inotify: Adding watch on %s\n", watch);

              ret = Qtrue;
            }
          else subSharedLogWarn("Failed adding watch on file `%s': %s\n",
            watch, strerror(errno));
        }
#endif /* HAVE_SYS_INOTIFY_H */
      else subSharedLogWarn("Failed handling unknown value type\n");
    }

  return ret;
} /* }}} */

/* RubySubletUnwatch {{{ */
/*
 * call-seq: unwatch -> true or false
 *
 * Remove watch from Sublet
 *
 *  unwatch
 *  => true
 */

static VALUE
RubySubletUnwatch(VALUE self)
{
  VALUE ret = Qfalse;
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);
  if(s)
    {
      if(s->flags & SUB_SUBLET_SOCKET) ///< Probably a socket
        {
          XDeleteContext(subtle->dpy, subtle->windows.panel1, s->watch);
          subEventWatchDel(s->watch);

          s->flags &= ~SUB_SUBLET_SOCKET;
          s->watch  = 0;

          ret = Qtrue;
        }
#ifdef HAVE_SYS_INOTIFY_H
      else if(s->flags & SUB_SUBLET_INOTIFY) /// Inotify file
        {
          XDeleteContext(subtle->dpy, subtle->windows.panel1, s->watch);
          inotify_rm_watch(subtle->notify, s->watch);

          s->flags &= ~SUB_SUBLET_INOTIFY;
          s->watch  = 0;

          ret = Qtrue;
        }
#endif /* HAVE_SYS_INOTIFY_H */
    }

  return ret;
} /* }}} */

/* Extern */

 /** subRubyInit {{{
  * @brief Init ruby
  **/

void
subRubyInit(void)
{
  VALUE mod = Qnil, sublet = Qnil;

  void Init_prelude(void);

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  /* FIXME: Fake ruby_init_gems(Qtrue) */
  rb_define_module("Gem");
  Init_prelude();

  /*
   * Document-class: Kernel
   *
   * Ruby Kernel class for Sublet DSL
   */

  rb_define_method(rb_mKernel, "configure", RubyKernelConfigure, 1);
  rb_define_method(rb_mKernel, "on",        RubyKernelEvent,     1);
  rb_define_method(rb_mKernel, "helper",    RubyKernelHelper,    0);

  /*
   * Document-class: Object
   *
   * Ruby Object class dispatcher
   */

  rb_define_singleton_method(rb_cObject, "const_missing", RubyObjectDispatcher, 1);

  /*
   * Document-class: Subtle
   *
   * Subtle is the module for interaction with the window manager
   */

  mod = rb_define_module("Subtle");

  /*
   * Document-class: Subtle::Sublet
   *
   * Sublet class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* Class methods */
  rb_define_method(sublet, "method_missing", RubySubletDispatcher,       -1);
  rb_define_method(sublet, "render",         RubySubletRender,            0);
  rb_define_method(sublet, "name",           RubySubletNameReader,        0);
  rb_define_method(sublet, "interval",       RubySubletIntervalReader,    0);
  rb_define_method(sublet, "interval=",      RubySubletIntervalWriter,    1);
  rb_define_method(sublet, "data",           RubySubletDataReader,        0);
  rb_define_method(sublet, "data=",          RubySubletDataWriter,        1);
  rb_define_method(sublet, "background=",    RubySubletBackgroundWriter,  1);
  rb_define_method(sublet, "show",           RubySubletShow,              0);
  rb_define_method(sublet, "hide",           RubySubletHide,              0);
  rb_define_method(sublet, "hidden?",        RubySubletHidden,            0);
  rb_define_method(sublet, "watch",          RubySubletWatch,             1);
  rb_define_method(sublet, "unwatch",        RubySubletUnwatch,           0);

  /* Bypassing garbage collection */
  shelter = rb_ary_new();
  rb_gc_register_address(&shelter);
} /* }}} */

 /** subRubyLoadConfig {{{
  * @brief Load config file
  * @param[in]  path  Path to config file
  **/

void
subRubyLoadConfig(void)
{
  int state = 0;
  char buf[100], path[50];

  /* Check config paths */
  if(subtle->paths.config)
    snprintf(path, sizeof(path), "%s", subtle->paths.config);
  else
    {
      char *tok = NULL, *bufp = buf, *home = getenv("XDG_CONFIG_HOME"),
        *dirs = getenv("XDG_CONFIG_DIRS");

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.config", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s:%s", home ? home : path, dirs ? dirs : "/etc/xdg");

      /* Search config in XDG path */
      while((tok = strsep(&bufp, ":")))
        {
          snprintf(path, sizeof(path), "%s/%s/%s", tok, PKG_NAME, PKG_CONFIG);

          if(-1 != access(path, R_OK)) ///< Check if config file exists
            break;

          subSharedLogDebug("Checked config=%s\n", path);
        }
    }

  /* Loading config */
  printf("Using config `%s'\n", path);
  rb_load_protect(rb_str_new2(path), 0, &state); ///< Load config
  if(state)
    {
      RubyBacktrace();
      subEventFinish();
      return;
    }

  if(!subtle || !subtle->dpy) return; ///< Exit after config check

  /* Carefully load config */
  rb_protect(RubyWrapLoadConfig, Qnil, &state);
  if(state)
    {
      RubyBacktrace();
      subEventFinish();
    }
} /* }}} */

 /** subRubyReloadConfig {{{
  * @brief Reload config file
  **/

void
subRubyReloadConfig(void)
{
  int i;

  /* Reset before reloading */
  subtle->flags &= (SUB_SUBTLE_DEBUG|SUB_SUBTLE_EWMH|SUB_SUBTLE_RUN);
  subtle->vid = 0;

  /* Reset sublet panel flags */
  for(i = 0; i < subtle->sublets->ndata; i++)
    {
      SubSublet *s = SUBLET(subtle->sublets->data[i]);

      s->flags &= ~(SUB_SUBLET_PANEL|SUB_PANEL_BOTTOM| SUB_PANEL_SPACER1|
        SUB_PANEL_SPACER1| SUB_PANEL_SEPARATOR1|SUB_PANEL_SEPARATOR2);
    }

  /* Unload fonts */
  if(subtle->font)
    {
      subSharedFontKill(subtle->dpy, subtle->font);
      subtle->font = NULL;
    }

  /* Clear arrays */
  subArrayClear(subtle->hooks,     True); ///< Must be first
  subArrayClear(subtle->grabs,     True);
  subArrayClear(subtle->gravities, True);
  subArrayClear(subtle->panels,    False);
  subArrayClear(subtle->tags,      True);
  subArrayClear(subtle->views,     True);

  /* Remove constants */
  subRubyRemove("OPTIONS");
  subRubyRemove("PANEL");
  subRubyRemove("COLORS");
  subRubyRemove("GRAVITIES");
  subRubyRemove("GRABS");
  subRubyRemove("TAGS");
  subRubyRemove("VIEWS");
  subRubyRemove("HOOKS");

  subRubyLoadConfig();
  subRubyLoadPanels();
  subDisplayConfigure();

  /* Update client tags */
  for(i = 0; i < subtle->clients->ndata; i++)
    subClientSetTags(CLIENT(subtle->clients->data[i]));

  subViewJump(subtle->views->data[0], True);

  subPanelUpdate();
  subPanelRender();

  printf("Reloaded config\n");
} /* }}} */

 /** subRubyReloadSublets {{{
  * @brief Reload all sublets
  **/

void
subRubyReloadSublets(void)
{
  subArrayClear(subtle->sublets, True);
  subArrayClear(subtle->panels,  False); ///< Before sublets

  subRubyLoadSublets();
  subRubyLoadPanels();

  subPanelUpdate();
  subPanelRender();

  printf("Reloaded sublets\n");
} /* }}} */

 /** subRubyLoadSublet {{{
  * @brief Load sublets from path
  * @param[in]  path  Path of the sublets
  **/

void
subRubyLoadSublet(const char *file)
{
  int state = 0;
  char buf[100] = { 0 };
  SubSublet *s = NULL;
  VALUE str = Qnil, rargs[1] = { Qnil }, mod = Qnil, klass = Qnil;

  /* Check path */
  if(subtle->paths.sublets)
    snprintf(buf, sizeof(buf), "%s/%s", subtle->paths.sublets, file);
  else
    {
      char *home = getenv("XDG_DATA_HOME"), path[50] = { 0 };

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.local/share", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s/%s/sublets/%s",
        home ? home : path, PKG_NAME, file);
    }
  subSharedLogDebug("sublet=%s\n", buf);

  /* Carefully load file */
  str = rb_protect(RubyWrapRead, rb_str_new2(buf), &state);
  if(state)
    {
      subSharedLogWarn("Failed loading sublet `%s'\n", file);
      RubyBacktrace();
    }

  /* Create sublet */
  s = subSubletNew();
  mod         = rb_const_get(rb_mKernel, rb_intern("Subtle"));
  klass       = rb_const_get(mod, rb_intern("Sublet"));
  s->instance = Data_Wrap_Struct(klass, NULL, NULL, (void *)s);

  /* Create namespace module */
  s->mod = rb_module_new();
  rb_define_singleton_method(s->mod, "const_missing", RubyObjectDispatcher, 1);

  subArrayPush(subtle->sublets, s);
  rb_ary_push(shelter, s->instance); ///< Protect from GC

  rargs[0] = str;
  rb_mod_module_eval(1, rargs, s->mod);

  /* Configure and run sublet first time */
  subRubyCall(SUB_CALL_SUBLET_CONFIGURE, s->instance, (void *)s, NULL);

  if(0 >= s->interval) s->interval = 60; ///< Sanitize
  if(s->flags & SUB_SUBLET_RUN)
    {
      subRubyCall(SUB_CALL_SUBLET_RUN, s->instance,
        (void *)s, NULL); ///< First run
    }

  printf("Loaded sublet (%s)\n", s->name);
} /* }}} */

 /** subRubyLoadPanels {{{
  * @brief Load panels
  **/

void
subRubyLoadPanels(void)
{
  int state = 0;

  /* Carefully load panels */
  rb_protect(RubyWrapLoadPanels, Qnil, &state);
  if(state)
    {
      subSharedLogWarn("Failed loading panels\n");
      RubyBacktrace();
      subEventFinish();
    }
} /* }}} */

 /** subRubyLoadSublets {{{
  * @brief Load sublets from path
  **/

void
subRubyLoadSublets(void)
{
  int i, num;
  char buf[100];
  struct dirent **entries = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  /* Init inotify on demand */
  if(!subtle->notify)
    {
      if(0 > (subtle->notify = inotify_init()))
        {
          subSharedLogWarn("Failed initing inotify\n");
          subSharedLogDebug("Inotify: %s\n", strerror(errno));

          return;
        }
      else fcntl(subtle->notify, F_SETFL, O_NONBLOCK);
    }
#endif /* HAVE_SYS_INOTIFY_H */

  /* Check path */
  if(subtle->paths.sublets)
    snprintf(buf, sizeof(buf), "%s", subtle->paths.sublets);
  else
    {
      char *data = getenv("XDG_DATA_HOME"), path[50] = { 0 };

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.local/share", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s/%s/sublets", data ? data : path, PKG_NAME);
    }
  printf("Loading sublets from `%s'\n", buf);

  /* Scan directory */
  if(0 < ((num = scandir(buf, &entries, RubyFilter, alphasort))))
    {
      for(i = 0; i < num; i++)
        {
          subRubyLoadSublet(entries[i]->d_name);

          free(entries[i]);
        }
      free(entries);
    }
  else subSharedLogWarn("No sublets found\n");

  subSubletUpdate();
} /* }}} */

 /** subRubyLoadSubtlext {{{
  * @brief Load subtlext
  **/

void
subRubyLoadSubtlext(void)
{
  int state = 0;

  /* Carefully load sublext */
  rb_protect(RubyWrapLoadSubtlext, Qnil, &state);
  if(state)
    {
      subSharedLogWarn("Failed loading subtlext\n");
      RubyBacktrace();
      subEventFinish();
    }
} /* }}} */

 /** subRubyCall {{{
  * @brief Safely call ruby script
  * @param[in]  type   Script type
  * @param[in]  proc   Script receiver
  * @param[in]  data1  Extra data
  * @param[in]  data2  Extra data
  * @retval   0  Called script returned false
  * @retval   1  Called script returned true
  * @retval  -1  Calling script failed
  **/

int
subRubyCall(int type,
  unsigned long proc,
  void *data1,
  void *data2)
{
  int state = 0;
  VALUE result = Qnil, rargs[4] = { 0 };

  /* Wrap up data */
  rargs[0] = (VALUE)type;
  rargs[1] = proc;
  rargs[2] = (VALUE)data1;
  rargs[3] = (VALUE)data2;

  /* Limit execution time */
  if(0 < subtle->limit)
    {
      signal(SIGALRM, RubySignal);
      alarm(EXECTIME);
    }

  /* Carefully call */
  result = rb_protect(RubyWrapCall, (VALUE)&rargs, &state);
  if(state)
    {
      subSharedLogWarn("Failed calling %s\n",
        type & (SUB_CALL_SUBLET_RUN|SUB_CALL_SUBLET_DOWN) ?  "sublet" : "proc");
      RubyBacktrace();

      result = Qnil;
    }

  if(0 < subtle->limit) alarm(0); ///< Reset alarm

#ifdef DEBUG
  subSharedLogDebug("GC RUN\n");
  rb_gc_start();
#endif /* DEBUG */

  return Qtrue == result ? 1 : (Qfalse == result ? 0 : -1);
} /* }}} */

 /** subRubyRemove {{{
  * @brief Remove constant
  * @param[in]  name  Name of constant
  **/

int
subRubyRemove(char *name)
{
  int state = 0;

  rb_protect(RubyWrapRemove, rb_str_new2(name), &state);

  return state;
} /* }}} */

 /** subRubyRelease {{{
  * @brief Release value from shelter
  * @param[in]  value  The released value
  **/

int
subRubyRelease(unsigned long value)
{
  int state = 0;

  rb_protect(RubyWrapRelease, value, &state);

  return state;
} /* }}} */

 /** subRubyFinish {{{
  * @brief Finish ruby stack
  **/

void
subRubyFinish(void)
{
  if(Qnil != shelter)
    {
      ruby_finalize();

#ifdef HAVE_SYS_INOTIFY_H
      if(subtle && subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */
    }

  subSharedLogDebug("kill=ruby\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
