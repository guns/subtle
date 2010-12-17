
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
#include <ruby/encoding.h>
#include "subtle.h"

#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))

static VALUE shelter = Qnil, mod = Qnil, config = Qnil; ///< Globals

/* Typedef {{{ */
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
static void
RubyBacktrace(void)
{
  VALUE lasterr = Qnil;

  /* Get last error */
  if(!NIL_P(lasterr = rb_gv_get("$!")))
    {
      int i;
      VALUE message = Qnil, klass = Qnil, backtrace = Qnil, entry = Qnil;

      /* Fetching backtrace data */
      message   = rb_obj_as_string(lasterr);
      klass     = rb_class_path(CLASS_OF(lasterr));
      backtrace = rb_funcall(lasterr, rb_intern("backtrace"), 0, NULL);

      /* Print error and backtrace */
      subSharedLogWarn("%s: %s\n", RSTRING_PTR(klass), RSTRING_PTR(message));
      for(i = 0; Qnil != (entry = rb_ary_entry(backtrace, i)); ++i)
        printf("\tfrom %s\n", RSTRING_PTR(entry));
    }
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
      VALUE subtlext = Qnil, klass = Qnil;

      XSync(subtle->dpy, False); ///< Sync before going on

      subtlext = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

      if(c->flags & SUB_TYPE_CLIENT) /* {{{ */
        {
          int flags = 0;

          /* Create client instance */
          id     = subArrayIndex(subtle->clients, (void *)c);
          klass  = rb_const_get(subtlext, rb_intern("Client"));
          object = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));

          /* Translate flags */
          if(c->flags & SUB_CLIENT_MODE_FULL)  flags |= SUB_EWMH_FULL;
          if(c->flags & SUB_CLIENT_MODE_FLOAT) flags |= SUB_EWMH_FLOAT;
          if(c->flags & SUB_CLIENT_MODE_STICK) flags |= SUB_EWMH_STICK;

          /* Set properties */
          rb_iv_set(object, "@win",      LONG2NUM(c->win));
          rb_iv_set(object, "@name",     rb_str_new2(c->name));
          rb_iv_set(object, "@instance", rb_str_new2(c->instance));
          rb_iv_set(object, "@klass",    rb_str_new2(c->klass));
          rb_iv_set(object, "@role",     c->role ? rb_str_new2(c->role) : Qnil);
          rb_iv_set(object, "@flags",    INT2FIX(flags));
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

/* Get */

/* RubyGetGeometry {{{ */
static int
RubyGetGeometry(VALUE ary,
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

/* RubyGetGravity {{{ */
static int
RubyGetGravity(VALUE value)
{
  int gravity = -1;

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

/* RubyGetGravityString {{{ */
static void
RubyGetGravityString(VALUE value,
  char **string)
{
  /* Check value type */
  if(T_ARRAY == rb_type(value))
    {
      int i = 0, j = 0, id = -1, size = 0;
      VALUE entry = Qnil;

      /* Create gravity string */
      size    = RARRAY_LEN(value);
      *string = (char *)subSharedMemoryAlloc(size + 1, sizeof(char));

      /* Add gravities */
      for(i = 0, j = 0; Qnil != (entry = rb_ary_entry(value, i)); i++)
        {
          /* We store ids in a string to ease the whole thing */
          if(-1 != (id = RubyGetGravity(entry)))
            (*string)[j++] = id + 65; /// Use letters only
          else subSharedLogWarn("Failed finding gravity `%s'\n",
            SYM2CHAR(entry));
        }
    }
} /* }}} */

/* Hash */

/* RubyHashConvert {{{ */
static VALUE
RubyHashConvert(VALUE value)
{
  VALUE hash = Qnil;

  /* Check value type */
  switch(rb_type(value))
    {
      case T_HASH:
        hash = value;
        break;
      case T_NIL:
        /* Ignore this case */
        break;
      default:
        /* Convert to hash */
        hash = rb_hash_new();

        rb_hash_aset(hash, Qnil, value);
    }

  return hash;
} /* }}} */

/* RubyHashUpdate {{{ */
static int
RubyHashUpdate(VALUE key,
  VALUE value,
  VALUE data)
{
  if(key == Qundef) return ST_CONTINUE;

  rb_hash_aset(data, key, value);

  return ST_CONTINUE;
} /* }}} */

/* RubyHashMatch {{{ */
static int
RubyHashMatch(VALUE key,
  VALUE value,
  VALUE data)
{
  int type = 0;
  VALUE regex = Qnil, *rargs = (VALUE *)data;

  if(key == Qundef) return ST_CONTINUE;

  /* Check value type */
  switch(rb_type(key))
    {
      case T_NIL:
        type = SUB_TAG_MATCH_INSTANCE|SUB_TAG_MATCH_CLASS; ///< Defaults
        break;
      case T_SYMBOL:
        if(CHAR2SYM("name") == key)          type = SUB_TAG_MATCH_NAME;
        else if(CHAR2SYM("instance") == key) type = SUB_TAG_MATCH_INSTANCE;
        else if(CHAR2SYM("class") == key)    type = SUB_TAG_MATCH_CLASS;
        else if(CHAR2SYM("role") == key)     type = SUB_TAG_MATCH_ROLE;
        else if(CHAR2SYM("type") == key)     type = SUB_TAG_MATCH_TYPE;
        break;
      case T_ARRAY:
          {
            int i;
            VALUE entry = Qnil;

            /* Check flags in array */
            for(i = 0; Qnil != (entry = rb_ary_entry(key, i)); i++)
              {
                if(CHAR2SYM("name") == entry)          type |= SUB_TAG_MATCH_NAME;
                else if(CHAR2SYM("instance") == entry) type |= SUB_TAG_MATCH_INSTANCE;
                else if(CHAR2SYM("class") == entry)    type |= SUB_TAG_MATCH_CLASS;
                else if(CHAR2SYM("role") == entry)     type |= SUB_TAG_MATCH_ROLE;
                else if(CHAR2SYM("type") == entry)     type |= SUB_TAG_MATCH_TYPE;
              }
          }
        break;
      default: rb_raise(rb_eArgError, "Unknown value type");
    }

  /* Check value type */
  switch(rb_type(value))
    {
      case T_REGEXP:
        regex = rb_funcall(value, rb_intern("source"), 0, NULL);
        break;
      case T_SYMBOL:
        if(CHAR2SYM("desktop") == value)      type |= SUB_CLIENT_TYPE_DESKTOP;
        else if(CHAR2SYM("dock") == value)    type |= SUB_CLIENT_TYPE_DOCK;
        else if(CHAR2SYM("toolbar") == value) type |= SUB_CLIENT_TYPE_TOOLBAR;
        else if(CHAR2SYM("splash") == value)  type |= SUB_CLIENT_TYPE_SPLASH;
        else if(CHAR2SYM("dialog") == value)  type |= SUB_CLIENT_TYPE_DIALOG;
        else regex = rb_sym_to_s(value);
        break;
      case T_STRING: regex = value; break;
      default: rb_raise(rb_eArgError, "Unknown value type");
    }

  /* Finally create regex if there is any and append additional flags */
  if(0 < type)
    {
      subTagRegex(TAG(rargs[0]), type|rargs[1],
        NIL_P(regex) ? NULL : RSTRING_PTR(regex));
    }

  return ST_CONTINUE;
} /* }}} */

/* RubyHashCombine {{{ */
static VALUE
RubyHashCombine(VALUE self,
  VALUE sym,
  VALUE value)
{
  VALUE hash = Qnil, match = Qnil, params = rb_iv_get(self, "@params");

  hash = RubyHashConvert(value);

  /* Check if hash contains key and append or otherwise create it */
  if(T_HASH == rb_type(match = rb_hash_lookup(params, sym)))
    {
      rb_hash_foreach(hash, RubyHashUpdate, match); ///< Merge!
    }
  else rb_hash_aset(params, sym, hash);

  return Qnil;
} /* }}} */

/* Eval */

/* RubyEvalHook {{{ */
static void
RubyEvalHook(VALUE event,
  VALUE proc)
{
  int i;
  SubHook *h = NULL;

  RubySymbols hooks[] =
  {
    { CHAR2SYM("start"),            SUB_HOOK_START                                   },
    { CHAR2SYM("exit"),             SUB_HOOK_EXIT                                    },
    { CHAR2SYM("tile"),             SUB_HOOK_TILE                                    },
    { CHAR2SYM("reload"),           SUB_HOOK_RELOAD                                  },
    { CHAR2SYM("client_create"),    (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_CREATE)    },
    { CHAR2SYM("client_configure"), (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_CONFIGURE) },
    { CHAR2SYM("client_focus"),     (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_FOCUS)     },
    { CHAR2SYM("client_kill"),      (SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_KILL)      },
    { CHAR2SYM("tag_create"),       (SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_CREATE)       },
    { CHAR2SYM("tag_kill"),         (SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_KILL)         },
    { CHAR2SYM("view_create"),      (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_CREATE)      },
    { CHAR2SYM("view_configure"),   (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_CONFIGURE)   },
    { CHAR2SYM("view_jump"),        (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS)       },
    { CHAR2SYM("view_kill"),        (SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_KILL)        }
  };

  if(subtle->flags & SUB_SUBTLE_CHECK) return; ///< Skip on check

  /* Generic hooks */
  for(i = 0; LENGTH(hooks) > i; i++)
    {
      if(hooks[i].sym == event)
        {
          /* Create new hook */
          if((h = subHookNew(hooks[i].flags, proc)))
            {
              subArrayPush(subtle->hooks, (void *)h);
              rb_ary_push(shelter, proc); ///< Protect from GC
            }

          break;
        }
    }
} /* }}} */

/* RubyEvalGrab {{{ */
static void
RubyEvalGrab(VALUE chain,
  VALUE value)
{
  int type = -1;
  SubData data = { None };

  /* Check value type */
  switch(rb_type(value))
    {
      case T_SYMBOL: /* {{{ */
          {
            /* Find symbol and add flag */
            if(CHAR2SYM("ViewNext") == value)
              {
                type = SUB_GRAB_VIEW_SELECT;
                data = DATA((unsigned long)SUB_VIEW_NEXT);
              }
            else if(CHAR2SYM("ViewPrev") == value)
              {
                type = SUB_GRAB_VIEW_SELECT;
                data = DATA((unsigned long)SUB_VIEW_PREV);
              }
            else if(CHAR2SYM("SubtleReload") == value)
              {
                type = SUB_GRAB_SUBTLE_RELOAD;
              }
            else if(CHAR2SYM("SubtleRestart") == value)
              {
                type = SUB_GRAB_SUBTLE_RESTART;
              }
            else if(CHAR2SYM("SubtleQuit") == value)
              {
                type = SUB_GRAB_SUBTLE_QUIT;
              }
            else if(CHAR2SYM("SubtleEscape") == value)
              {
                type           = SUB_GRAB_SUBTLE_ESCAPE;
                subtle->flags |= SUB_SUBTLE_ESCAPE;
              }
            else if(CHAR2SYM("WindowMove") == value)
              {
                type = SUB_GRAB_WINDOW_MOVE;
              }
            else if(CHAR2SYM("WindowResize") == value)
              {
                type = SUB_GRAB_WINDOW_RESIZE;
              }
            else if(CHAR2SYM("WindowFloat") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_FLOAT);
              }
            else if(CHAR2SYM("WindowFull") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_FULL);
              }
            else if(CHAR2SYM("WindowStick") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_CLIENT_MODE_STICK);
              }
            else if(CHAR2SYM("WindowRaise") == value)
              {
                type = SUB_GRAB_WINDOW_STACK;
                data = DATA((unsigned long)Above);
              }
            else if(CHAR2SYM("WindowLower") == value)
              {
                type = SUB_GRAB_WINDOW_STACK;
                data = DATA((unsigned long)Below);
              }
            else if(CHAR2SYM("WindowLeft") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_WINDOW_LEFT);
              }
            else if(CHAR2SYM("WindowDown") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_WINDOW_DOWN);
              }
            else if(CHAR2SYM("WindowUp") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_WINDOW_UP);
              }
            else if(CHAR2SYM("WindowRight") == value)
              {
                type = SUB_GRAB_WINDOW_SELECT;
                data = DATA((unsigned long)SUB_WINDOW_RIGHT);
              }
            else if(CHAR2SYM("WindowKill") == value)
              {
                type = SUB_GRAB_WINDOW_KILL;
              }

            /* Symbols with parameters */
            if(-1 == type)
              {
                const char *name = SYM2CHAR(value);

                /* Numbered grabs */
                if(!strncmp(name, "ViewJump", 8))
                  {
                    if((name = (char *)name + 8))
                      {
                        type = SUB_GRAB_VIEW_JUMP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "ViewSwitch", 10))
                  {
                    if((name = (char *)name + 10))
                      {
                        type = SUB_GRAB_VIEW_SWITCH;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "ScreenJump", 10))
                  {
                    if((name = (char *)name + 10))
                      {
                        type = SUB_GRAB_SCREEN_JUMP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
              }
          }
        break; /* }}} */
      case T_ARRAY: /* {{{ */
        type = (SUB_GRAB_WINDOW_GRAVITY|SUB_RUBY_DATA);
        data = DATA((unsigned long)value);

        rb_ary_push(shelter, value); ///< Protect from GC
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
      default:
        subSharedLogWarn("Unknown value type for grab\n");

        return;
    }

  /* Check value type */
  if(T_STRING == rb_type(chain))
    {
      if(-1 != type)
        {
          /* Skip on checking only */
          if(!(subtle->flags & SUB_SUBTLE_CHECK))
            {
              SubGrab *g = NULL;

              /* Finally create new grab */
              if((g = subGrabNew(RSTRING_PTR(chain), type, data)))
                {
                  subArrayPush(subtle->grabs, (void *)g);

                  /* Store address of escape grab to limit search times */
                  if(g->flags & SUB_GRAB_SUBTLE_ESCAPE) subtle->escape = g;
                }
            }
        }
      else subSharedLogWarn("Unknown grab action `:%s'\n", SYM2CHAR(value));
    }
  else rb_raise(rb_eArgError, "Unknown value type for grab");
} /* }}} */

/* RubyEvalPanel {{{ */
void
RubyEvalPanel(VALUE ary,
  int position,
  SubScreen *s)
{
  /* Check value type */
  if(T_ARRAY == rb_type(ary))
    {
      int i, j, flags = 0;
      Window panel = s->panel1;
      VALUE entry = Qnil, tray = Qnil, spacer = Qnil, separator = Qnil;
      VALUE sublets = Qnil, views = Qnil, title = Qnil, center = Qnil;
      SubPanel *p = NULL, *last = NULL;;

      /* Get syms */
      tray      = CHAR2SYM("tray");
      spacer    = CHAR2SYM("spacer");
      center    = CHAR2SYM("center");
      separator = CHAR2SYM("separator");
      sublets   = CHAR2SYM("sublets");
      views     = CHAR2SYM("views");
      title     = CHAR2SYM("title");

      /* Set position of panel */
      if(SUB_SCREEN_PANEL2 == position)
        {
          flags |= SUB_PANEL_BOTTOM;
          panel  = s->panel2;
        }

      /* Parse array and assemble panel */
      for(i = 0; Qnil != (entry = rb_ary_entry(ary, i)); ++i)
        {
          p = NULL;

          /* Parse array and assemble panel */
          if(entry == spacer || entry == center || entry == separator)
            {
              /* Add spacer after panel item (except separator) */
              if(last && entry != separator && flags & SUB_PANEL_SPACER1)
                {
                  last->flags |= SUB_PANEL_SPACER2;
                  flags       &= ~SUB_PANEL_SPACER1;
                }

              /* Add separator after panel item (except spacer) */
              if(last && entry != spacer && flags & SUB_PANEL_SEPARATOR1)
                {
                  last->flags |= SUB_PANEL_SEPARATOR2;
                  flags       &= ~SUB_PANEL_SEPARATOR1;
                }

              /* Add flags */
              if(entry == spacer)         flags |= SUB_PANEL_SPACER1;
              else if(entry == center)    flags |= SUB_PANEL_CENTER;
              else if(entry == separator) flags |= SUB_PANEL_SEPARATOR1;
            }
          else if(entry == sublets)
            {
              if(0 < subtle->sublets->ndata)
                {
                  /* Add dummy panel as entry point for sublets */
                  flags |= SUB_PANEL_SUBLETS;
                  p      = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
                }
            }
          else if(entry == tray)
            {
              flags |= (SUB_TYPE_PANEL|SUB_PANEL_TRAY);
              p      = &subtle->windows.tray;
            }
          else if(entry == views)
            {
              p = subPanelNew(SUB_PANEL_VIEWS);
            }
          else if(entry == title)
            {
              p = subPanelNew(SUB_PANEL_TITLE);
            }
          else
            {
              /* Check for sublets */
              for(j = 0; j < subtle->sublets->ndata; j++)
                {
                  SubPanel *sublet = PANEL(subtle->sublets->data[j]);

                  if(entry == CHAR2SYM(sublet->sublet->name))
                    {
                      p = sublet;

                      break;
                    }
                }
            }

          /* Finally add to panel */
          if(p)
            {
              p->flags  |= flags;
              p->screen  = s;
              s->flags  |= position; ///< Enable this panel
              flags      = 0;
              last       = p;

              subArrayPush(s->panels, (void *)p);
              XReparentWindow(subtle->dpy, p->win, panel, 0, 0);
            }
        }

      /* Add stuff to last item */
      if(last && flags & SUB_PANEL_SPACER1)    last->flags |= SUB_PANEL_SPACER2;
      if(last && flags & SUB_PANEL_SEPARATOR1) last->flags |= SUB_PANEL_SEPARATOR2;

      subRubyRelease(ary);
    }
} /* }}} */

/* RubyEvalConfig {{{ */
static void
RubyEvalConfig(void)
{
  int i;

  /* FIXME: Check and update colors */
  if(0 == subtle->colors.separator)
    subtle->colors.separator = subtle->colors.fg_panel;

  /* Update panel height */
  subtle->th = subtle->font->height + 2 * subtle->pbw +
    subtle->padding.width + subtle->padding.height;

  /* Set separator */
  if(subtle->separator.string)
    {
      subtle->separator.width = subSharedTextWidth(subtle->dpy, subtle->font,
        subtle->separator.string, strlen(subtle->separator.string), NULL,
        NULL, True);

      /* Add spacing for separator */
      if(0 < subtle->separator.width)
        subtle->separator.width += 6;
    }

  /* Check and update grabs */
  if(0 == subtle->grabs->ndata)
    {
      subSharedLogWarn("No grabs found\n");
      subSubtleFinish();

      exit(-1);
    }
  else
    {
      subArraySort(subtle->grabs, subGrabCompare);

      /* Update grabs and gravites */
      for(i = 0; i < subtle->grabs->ndata; i++)
        {
          SubGrab *g = GRAB(subtle->grabs->data[i]);

          /* Update gravity grab */
          if(g->flags & SUB_GRAB_WINDOW_GRAVITY)
            {
              char *string = NULL;

              /* Create gravity string */
              RubyGetGravityString(g->data.num, &string);
              subRubyRelease(g->data.num);
              g->data.string  = string;
              g->flags       &= ~SUB_RUBY_DATA;
            }
        }
    }

  /* Check gravities */
  if(0 == subtle->gravities->ndata)
    {
      subSharedLogError("No gravities found\n");
      subSubtleFinish();

      exit(-1);
    }

  /* Get default gravity */
  if(-1 == (subtle->gravity = RubyGetGravity(subtle->gravity)))
    subtle->gravity = 0;

  subGravityPublish();

  /* Check and update screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Check if vid exists */
      if(0 > s->vid || s->vid >= subtle->views->ndata)
        s->vid = 0;

      s->flags &= ~SUB_RUBY_DATA;
    }

  /* Check and update tags */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);

      /* Update gravities */
      if(t->flags & SUB_TAG_GRAVITY)
        {
          if(-1 == (t->gravity = RubyGetGravity(t->gravity)))
            t->gravity = 0;
        }
    }

  subTagPublish();

  if(1 == subtle->tags->ndata) subSharedLogWarn("No tags found\n");

  /* Check and update views */
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
    }

  subViewPublish();
  subDisplayPublish();
} /* }}} */

/* Wrap */

/* RubyWrapLoadSubtlext {{{ */
static VALUE
RubyWrapLoadSubtlext(VALUE data)
{
  return rb_require("subtle/subtlext");
}/* }}} */

/* RubyWrapLoadPanels {{{ */
static VALUE
RubyWrapLoadPanels(VALUE data)
{
  int i;

  /* Pass 1: Load actual panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      if(!s->panels) s->panels = subArrayNew();

      RubyEvalPanel(s->top,    SUB_SCREEN_PANEL1, s);
      RubyEvalPanel(s->bottom, SUB_SCREEN_PANEL2, s);
    }

  /* Pass 2: Add remaining sublets if any */
  if(0 < subtle->sublets->ndata)
    {
      int j, k, pos;

      for(i = 0; i < subtle->screens->ndata; i++)
        {
          SubScreen *s = SCREEN(subtle->screens->data[i]);
          Window panel = s->panel1;

          for(j = 0; j < s->panels->ndata; j++)
            {
              SubPanel *p = PANEL(s->panels->data[j]);

              if(p->flags & SUB_PANEL_BOTTOM) panel = s->panel2;

              /* Find dummy panel */
              if(p->flags & SUB_PANEL_SUBLETS)
                {
                  SubPanel *sublet = NULL;

                  pos = j;

                  subArrayRemove(s->panels, (void *)p);

                  /* Find sublets not on any panel so far */
                  for(k = 0; k < subtle->sublets->ndata; k++)
                    {
                      sublet = PANEL(subtle->sublets->data[k]);

                      /* Check if screen is empty */
                      if(NULL == sublet->screen)
                        {
                          sublet->flags |= SUB_PANEL_SEPARATOR2;

                          subArrayInsert(s->panels, pos++, (void *)sublet);
                          XReparentWindow(subtle->dpy, sublet->win, panel, 0, 0);

                          /* Set borders */
                          XSetWindowBorder(subtle->dpy, sublet->win,
                            subtle->colors.bo_sublets);
                          XSetWindowBorderWidth(subtle->dpy, sublet->win,
                            subtle->pbw);
                        }
                    }

                  /* Check spacers and separators in first and last sublet */
                  if((sublet = PANEL(subArrayGet(s->panels, j))))
                    sublet->flags |= (p->flags & (SUB_PANEL_BOTTOM|
                      SUB_PANEL_SPACER1|SUB_PANEL_SEPARATOR1|SUB_PANEL_CENTER));

                  if((sublet = PANEL(subArrayGet(s->panels, pos - 1))))
                    {
                      sublet->flags |= (p->flags & SUB_PANEL_SPACER2);

                      if(!(p->flags & SUB_PANEL_SEPARATOR2))
                        sublet->flags &= ~SUB_PANEL_SEPARATOR2;
                    }

                  free(p);

                  break;
                }
            }
        }

      /* Finally sort sublets */
      subArraySort(subtle->sublets, subPanelCompare);
    }

  return Qnil;
} /* }}} */

/* RubyWrapCall {{{ */
static VALUE
RubyWrapCall(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  /* Check call type */
  switch((int)rargs[0])
    {
      case SUB_CALL_CONFIGURE: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__configure"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_RUN: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__run"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_DATA: /* {{{ */
          {
            int nlist = 0;
            char **list = NULL;
            Atom prop = subEwmhGet(SUB_EWMH_SUBTLE_DATA);
            VALUE meth = rb_intern("__data"), str = Qnil;

            /* Get data */
            if((list = subSharedPropertyGetStrings(subtle->dpy, ROOT,
                prop, &nlist)))
              {
                if(list && 0 < nlist)
                  str = rb_str_new2(list[0]);

                XFreeStringList(list);
              }

            subSharedPropertyDelete(subtle->dpy, ROOT, prop);

            rb_funcall(rargs[1], meth,
              MINMAX(rb_obj_method_arity(rargs[1], meth), 1, 2),
              rargs[1], str);
          }
        break; /* }}} */
      case SUB_CALL_WATCH: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__watch"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_DOWN: /* {{{ */
          {
            XButtonEvent *ev = (XButtonEvent *)rargs[2];
            VALUE meth = rb_intern("_down");

            rb_funcall(rargs[1], meth,
              MINMAX(rb_obj_method_arity(rargs[1], meth), 1, 4),
              rargs[1], INT2FIX(ev->x), INT2FIX(ev->y), INT2FIX(ev->button));
          }
        break; /* }}} */
      case SUB_CALL_OVER: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__over"), 1, rargs[1]);
        break; /* }}} */
      case SUB_CALL_OUT: /* {{{ */
        rb_funcall(rargs[1], rb_intern("__out"), 1, rargs[1]);
        break; /* }}} */
      default: /* {{{ */
        if(rb_obj_is_instance_of(rargs[1], rb_cMethod))
          {
            int arity = 0;
            VALUE receiver = Qnil;

            /* Get arity */
            receiver = rb_funcall(rargs[1], rb_intern("receiver"), 0, NULL);
            arity    = FIX2INT(rb_funcall(rargs[1], rb_intern("arity"),
              0, NULL));
            arity    = -1 == arity ? 2 : MINMAX(arity, 1, 2);

            rb_funcall(rargs[1], rb_intern("call"), arity, receiver,
              RubyConvert((VALUE *)rargs[2]));
          }
        else
          {
            rb_funcall(rargs[1], rb_intern("call"),
              MINMAX(rb_proc_arity(rargs[1]), 0, 1),
              RubyConvert((VALUE *)rargs[2]));
          }
        break; /* }}} */
    }

  return Qnil;
} /* }}} */

/* RubyWrapRelease {{{ */
static VALUE
RubyWrapRelease(VALUE value)
{
  /* Relase value from shelter */
  if(Qtrue == rb_ary_includes(shelter, value))
    rb_ary_delete(shelter, value);

  return Qnil;
} /* }}} */

/* RubyWrapRead {{{ */
static VALUE
RubyWrapRead(VALUE file)
{
  VALUE str = rb_funcall(rb_cFile, rb_intern("read"), 1, file);

  return str;
} /* }}} */

/* RubyWrapEval {{{ */
static VALUE
RubyWrapEval(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  rb_obj_instance_eval(2, rargs, rargs[2]);

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
  VALUE ret = Qnil;

  /* Load subtlext on demand */
  if(CHAR2SYM("Subtlext") == missing)
    {
      int state = 0;
      ID id = SYM2ID(missing);

      /* Carefully load sublext */
      rb_protect(RubyWrapLoadSubtlext, Qnil, &state);
      if(state)
        {
          subSharedLogWarn("Failed loading subtlext\n");
          RubyBacktrace();
          subSubtleFinish();

          exit(-1);
        }

      ret = rb_const_get(rb_mKernel, id);
    }

  return ret;
} /* }}} */

/* Options */

/* RubyOptionsInit {{{ */
/*
 * call-seq: init -> Subtle::Options
 *
 * Create a new Options object
 *
 *  client = Subtle::Options.new
 *  => #<Subtle::Options:xxx>
 */

static VALUE
RubyOptionsInit(VALUE self)
{
  rb_iv_set(self, "@params", rb_hash_new());

  return Qnil;
} /* }}} */

/* RubyOptionsDispatcher {{{ */
/*
 * Dispatcher for DSL proc methods - internal use only
 */

static VALUE
RubyOptionsDispatcher(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE missing = Qnil, arg = Qnil, params = rb_iv_get(self, "@params");

  rb_scan_args(argc, argv, "2", &missing, &arg);

  return rb_hash_aset(params, missing, arg); ///< Just store param
} /* }}} */

/* RubyOptionsMatch {{{ */
/*
 * call-seq: match -> nil
 *
 * Append match hashes if called multiple times
 *
 *  option.match :name => /foo/
 *  => nil
 */

static VALUE
RubyOptionsMatch(VALUE self,
  VALUE value)
{
  return RubyHashCombine(self, CHAR2SYM("match"), value);
} /* }}} */

/* RubyOptionsExclude {{{ */
/*
 * call-seq: exclude -> nil
 *
 * Append exclude hashes if called multiple times
 *
 *  option.exclude :name => /foo/
 *  => nil
 */

static VALUE
RubyOptionsExclude(VALUE self,
  VALUE value)
{
  return RubyHashCombine(self, CHAR2SYM("exclude"), value);
} /* }}} */

/* RubyOptionsGravity {{{ */
/*
 * call-seq: gravity -> nil
 *
 * Overwrite global gravity method
 *
 *  option.gravity :center
 *  => nil
 */

static VALUE
RubyOptionsGravity(VALUE self,
  VALUE gravity)
{
  VALUE params = rb_iv_get(self, "@params");


  /* Just store param */
  return rb_hash_aset(params, CHAR2SYM("gravity"), gravity);
} /* }}} */

/* Config */

/* RubyConfigSet {{{ */
/*
 * call-seq: set(option, value) -> nil
 *
 * Set options
 *
 *  set :border, 2
 */

static VALUE
RubyConfigSet(VALUE self,
  VALUE option,
  VALUE value)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(option))
    {
      switch(rb_type(value))
        {
          case T_FIXNUM: /* {{{ */
            if(CHAR2SYM("border") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->bw = FIX2INT(value);
              }
            else if(CHAR2SYM("step") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->step = FIX2INT(value);
              }
            else if(CHAR2SYM("snap") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->snap = FIX2INT(value);
              }
            else if(CHAR2SYM("outline") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    subtle->pbw = FIX2INT(value);
                  }
              }
            else if(CHAR2SYM("gap") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gap = FIX2INT(value);
              }
            else if(CHAR2SYM("gravity") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gravity = value; ///< Store for later
              }
            else subSharedLogWarn("Unknown set option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          case T_SYMBOL: /* {{{ */
            if(CHAR2SYM("gravity") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gravity = value; ///< Store for later
              }
            else subSharedLogWarn("Unknown set option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          case T_TRUE:
          case T_FALSE: /* {{{ */
            if(CHAR2SYM("urgent") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_URGENT;
              }
            else if(CHAR2SYM("resize") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_RESIZE;
              }
            else subSharedLogWarn("Unknown set option `:%s'\n",
              SYM2CHAR(option));
            break; /* }}} */
          case T_ARRAY: /* {{{ */
            if(CHAR2SYM("strut") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  RubyGetGeometry(value, &subtle->strut);
              }
            else if(CHAR2SYM("padding") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  RubyGetGeometry(value, &subtle->padding);
              }
            else subSharedLogWarn("Unknown set option `:%s'\n",
              SYM2CHAR(option));
            break; /* }}} */
          case T_STRING: /* {{{ */
            if(CHAR2SYM("font") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    if(!(subtle->font = subSharedFontNew(subtle->dpy,
                        RSTRING_PTR(value))))
                      {
                        subSubtleFinish();

                        exit(-1); ///< Should never happen
                      }

                    /* EWMH: Font */
                    subEwmhSetString(ROOT, SUB_EWMH_SUBTLE_FONT,
                      RSTRING_PTR(value));
                  }
              }
            else if(CHAR2SYM("separator") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    if(subtle->separator.string) free(subtle->separator.string);
                    subtle->separator.string = strdup(RSTRING_PTR(value));
                  }
              }
            else subSharedLogWarn("Unknown set option `:%s'\n", SYM2CHAR(option));
            break; /* }}} */
          default:
            rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
              SYM2CHAR(option));
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for set");

  return Qnil;
} /* }}} */

/* RubyConfigColor {{{ */
/*
 * call-seq: color(color, value) -> nil
 *
 * Set color
 *
 *  color :fg_panel, "#777777"
 */

static VALUE
RubyConfigColor(VALUE self,
  VALUE option,
  VALUE value)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(option) && T_STRING == rb_type(value))
    {
      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      /* Plain 'if' to save lookups */
      if(CHAR2SYM("fg_focus") == option || CHAR2SYM("focus_fg") == option)
        {
          subtle->colors.fg_focus = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_focus") == option || CHAR2SYM("focus_bg") == option)
        {
          subtle->colors.bg_focus = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("focus_border") == option)
        {
          subtle->colors.bo_focus = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_urgent") == option || CHAR2SYM("urgent_fg") == option)
        {
          subtle->colors.fg_urgent = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_urgent") == option || CHAR2SYM("urgent_bg") == option)
        {
          subtle->colors.bg_urgent = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("urgent_border") == option)
        {
          subtle->colors.bo_urgent = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_occupied") == option || CHAR2SYM("occupied_fg") == option)
        {
          subtle->colors.fg_occupied = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_occupied") == option || CHAR2SYM("occupied_bg") == option)
        {
          subtle->colors.bg_occupied = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("occupied_border") == option)
        {
          subtle->colors.bo_occupied = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_views") == option || CHAR2SYM("views_fg") == option)
        {
          subtle->colors.fg_views = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_views") == option || CHAR2SYM("views_bg") == option)
        {
          subtle->colors.bg_views = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("views_border") == option)
        {
          subtle->colors.bo_views = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_sublets") == option ||
          CHAR2SYM("sublets_fg") == option)
        {
          subtle->colors.fg_sublets = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_sublets") == option ||
          CHAR2SYM("sublets_bg") == option)
        {
          subtle->colors.bg_sublets = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("sublets_border") == option)
        {
          subtle->colors.bo_sublets = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_panel") == option || CHAR2SYM("panel_fg") == option)
        {
          subtle->colors.fg_panel = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_panel") == option || CHAR2SYM("panel_bg") == option)
        {
          subtle->colors.bg_panel = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("border_panel") == option)
        {
          subSharedLogDeprecated("Color `:border_panel` has been splitted "
            "into separate border colors\n");

          /* TODO: Update all border colors */
          subtle->colors.bo_focus    = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
          subtle->colors.bo_urgent   = subtle->colors.bo_focus;
          subtle->colors.bo_occupied = subtle->colors.bo_focus;
          subtle->colors.bo_views    = subtle->colors.bo_focus;
          subtle->colors.bo_sublets  = subtle->colors.bo_focus;
        }
      else if(CHAR2SYM("border_focus") == option ||
          CHAR2SYM("client_active") == option)
        {
          subtle->colors.bo_active = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("border_normal") == option ||
          CHAR2SYM("client_inactive") == option)
        {
          subtle->colors.bo_inactive = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("background") == option)
        {
          subtle->colors.bg = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));

          subtle->flags |= SUB_SUBTLE_BACKGROUND;
        }
      else if(CHAR2SYM("separator") == option)
        {
          subtle->colors.separator = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else subSharedLogWarn("Unknown color `:%s'\n", SYM2CHAR(option));
    }
  else rb_raise(rb_eArgError, "Unknown value type for color");

  return Qnil;
} /* }}} */

/* RubyConfigGravity {{{ */
/*
 * call-seq: gravity(name, value) -> nil
 *
 * Create gravity
 *
 *  gravity :top_left, [0, 0, 50, 50]
 */

static VALUE
RubyConfigGravity(VALUE self,
  VALUE name,
  VALUE value)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(name) && T_ARRAY == rb_type(value))
    {
      XRectangle geometry = { 0 };

      RubyGetGeometry(value, &geometry);

      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubGravity *g = NULL;

          /* Finally create new gravity */
          if((g = subGravityNew(SYM2CHAR(name), &geometry)))
            subArrayPush(subtle->gravities, (void *)g);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for gravity");

  return Qnil;
} /* }}} */

/* RubyConfigGrab {{{ */
/*
 * call-seq: grab(chain, value) -> nil
 *           grab(chain, &blk)  -> nil
 *
 * Create grabs
 *
 *  grab "A-F1", :ViewJump1
 */

static VALUE
RubyConfigGrab(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE chain = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &chain, &value);

  if(rb_block_given_p()) value = rb_block_proc(); ///< Get proc

  RubyEvalGrab(chain, value);

  return Qnil;
} /* }}} */

/* RubyConfigTag {{{ */
/*
 * call-seq: tag(name, regex) -> nil
 *           tag(name, blk)   -> nil
 *
 * Add a new tag
 *
 *  tag "foobar", "regex"
 *
 *  tag "foobar" do
 *    regex = "foobar"
 *  end
 */

static VALUE
RubyConfigTag(int argc,
  VALUE *argv,
  VALUE self)
{
  int flags = 0, type = 0;
  unsigned long gravity = 0;
  XRectangle geometry = { 0 };
  VALUE name = Qnil, match = Qnil, exclude = Qnil, params = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &name, &match);

  /* Call proc */
  if(rb_block_given_p())
    {
      VALUE klass = Qnil, options = Qnil;

      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 0, NULL);
      rb_obj_instance_eval(0, 0, options);
      params = rb_iv_get(options, "@params");

      /* Check match */
      if(T_HASH == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("match"))))
        match = value; ///< Lazy eval

      /* Check exclude */
      if(T_HASH == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("exclude"))))
        exclude = value; ///< Lazy eval

      /* Set gravity */
      if(T_SYMBOL == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("gravity"))) || T_FIXNUM == rb_type(value) ||
          T_ARRAY == rb_type(value))
        {
          flags   |= SUB_TAG_GRAVITY;
          gravity  = value; ///< Lazy eval
        }

      /* Set geometry */
      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("geometry"))) &&
          RubyGetGeometry(value, &geometry))
        flags |= SUB_TAG_GEOMETRY;

      /* Set window type */
      if(T_SYMBOL == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("type"))))
        {
          flags |= SUB_TAG_TYPE;

          /* Check type */
          if(CHAR2SYM("desktop") == value)      type = SUB_CLIENT_TYPE_DESKTOP;
          else if(CHAR2SYM("dock") == value)    type = SUB_CLIENT_TYPE_DOCK;
          else if(CHAR2SYM("toolbar") == value) type = SUB_CLIENT_TYPE_TOOLBAR;
          else if(CHAR2SYM("splash") == value)  type = SUB_CLIENT_TYPE_SPLASH;
          else if(CHAR2SYM("dialog") == value)  type = SUB_CLIENT_TYPE_DIALOG;
        }

      /* Check tri-state properties */
      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("geometry"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_CLIENT_MODE_FULL :
          SUB_CLIENT_MODE_NOFULL);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("full"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_CLIENT_MODE_FULL :
          SUB_CLIENT_MODE_NOFULL);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("float"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_CLIENT_MODE_FLOAT :
          SUB_CLIENT_MODE_NOFLOAT);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("stick"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_CLIENT_MODE_STICK :
          SUB_CLIENT_MODE_NOSTICK);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("urgent"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_CLIENT_MODE_URGENT :
          SUB_CLIENT_MODE_NOURGENT);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("resize"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_CLIENT_MODE_RESIZE :
          SUB_CLIENT_MODE_NORESIZE);
    }

  /* Check value type */
  if(T_STRING == rb_type(name))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          int duplicate = False;
          SubTag *t = NULL;

          /* Finally create new tag */
          if((t = subTagNew(RSTRING_PTR(name), &duplicate)))
            {
              VALUE rargs[2] = { 0 };

              t->flags    |= flags;
              t->gravity   = gravity;
              t->geometry  = geometry;
              t->type      = type;

              /* Add matcher */
              rargs[0] = (VALUE)t;
              if(!NIL_P(match = RubyHashConvert(match)))
                rb_hash_foreach(match, RubyHashMatch, (VALUE)&rargs);

              /* Add excludes */
              rargs[1] = SUB_TAG_MATCH_INVERT;
              if(!NIL_P(exclude = RubyHashConvert(exclude)))
                rb_hash_foreach(exclude, RubyHashMatch, (VALUE)&rargs);

              if(t->matcher) subArraySort(t->matcher, subTagCompare);

              /* Check if Duplicate */
              if(False == duplicate)
                subArrayPush(subtle->tags, (void *)t);
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for tag");

  return Qnil;
} /* }}} */

/* RubyConfigView {{{ */
/*
 * call-seq: view(name, regex) -> nil
 *
 * Add a new view
 *
 *  view "foobar", "regex"
 */

static VALUE
RubyConfigView(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE name = Qnil, match = Qnil, params = Qnil, value = Qnil, icon = value;

  rb_scan_args(argc, argv, "11", &name, &match);

  /* Call proc */
  if(rb_block_given_p())
    {
      VALUE subtlext = Qnil, klass = Qnil, options = Qnil;

      /* Collect options */
      subtlext = rb_const_get(rb_cObject, rb_intern("Subtlext"));
      klass    = rb_const_get(mod, rb_intern("Options"));
      options  = rb_funcall(klass, rb_intern("new"), 0, NULL);
      rb_obj_instance_eval(0, 0, options);
      params  = rb_iv_get(options, "@params");

      /* Check match */
      if(T_HASH == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("match"))))
        match = rb_hash_lookup(value, Qnil); ///< Lazy eval

      /* Check icon */
      switch(rb_type(value = rb_hash_lookup(params, CHAR2SYM("icon"))))
        {
          case T_DATA:
            /* Check object instance */
            if(rb_obj_is_instance_of(value,
                rb_const_get(subtlext, rb_intern("Icon"))))
              {
                icon = value; ///< Lazy eval

                rb_ary_push(shelter, icon); ///< Protect from GC
              }
            break;
          case T_STRING:
            /* Create new text icon */
            klass = rb_const_get(subtlext, rb_intern("Icon"));
            icon  = rb_funcall(klass, rb_intern("new"), 1, value);

            rb_ary_push(shelter, icon); ///< Protect from GC
            break;
          default: break;
        }
    }

  /* Check value type */
  if(T_STRING == rb_type(name))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubView *v = NULL;
          char *re = NULL;

          /* Convert regex */
          if(T_REGEXP == rb_type(match))
            match = rb_funcall(match, rb_intern("source"), 0, NULL);

          if(T_STRING == rb_type(match)) re = RSTRING_PTR(match);

          /* Finally create new view */
          if((v = subViewNew(RSTRING_PTR(name), re)))
            {
              subArrayPush(subtle->views, (void *)v);

              /* Combine icon and text */
              if(!NIL_P(icon))
                {
                  char buf[256] = { 0 };
                  VALUE iconstr = rb_funcall(icon, rb_intern("to_str"), 0, NULL);

                  v->text = subSharedTextNew();

                  snprintf(buf, sizeof(buf), "%s%s%s",
                    RSTRING_PTR(iconstr), RSTRING_PTR(name), SEPARATOR);

                  v->width = subSharedTextParse(subtle->dpy, subtle->font,
                    v->text, buf) + 2 * subtle->pbw + subtle->padding.x +
                    subtle->padding.y;
                }
            }
       }
    }
  else rb_raise(rb_eArgError, "Unknown value type for view");

  return Qnil;
} /* }}} */

/* RubyConfigOn {{{ */
/*
 * call-seq: on(event, &block) -> nil
 *
 * Event block for hooks
 *
 *  on :event do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubyConfigOn(VALUE self,
  VALUE event)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(event) && rb_block_given_p())
    {
      VALUE proc = rb_block_proc();

      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      RubyEvalHook(event, proc);
    }
  else rb_raise(rb_eArgError, "Unknown value type for on");

  return Qnil;
} /* }}} */

/* RubyConfigSublet {{{ */
/*
 * call-seq: sublet(name, blk)   -> nil
 *
 * Configure a sublet
 *
 *  sublet :jdownloader do
 *    interval 20
 *  end
 */

static VALUE
RubyConfigSublet(VALUE self,
  VALUE sublet)
{
  VALUE klass = Qnil, options = Qnil;

  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(sublet))
    {
      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 0, NULL);
      rb_obj_instance_eval(0, 0, options);

      /* Clone to get rid of object instance and store it */
      rb_hash_aset(config, sublet,
        rb_obj_clone(rb_iv_get(options, "@params")));
    }
  else rb_raise(rb_eArgError, "Unknown value type for sublet");

  return Qnil;
} /* }}} */

/* RubyConfigScreen {{{ */
/*
 * call-seq: screen(name, blk) -> nil
 *
 * Set options for screen
 *
 *  screen 1 do
 *    stipple  false
 *    top      []
 *    bottom   []
 *  end
 */

static VALUE
RubyConfigScreen(VALUE self,
  VALUE id)
{
  VALUE params = Qnil, value = Qnil, klass = Qnil, options = Qnil;
  SubScreen *s = NULL;

  /* FIXME: rb_need_block() */
  if(!rb_block_given_p()) return Qnil;

  /* Collect options */
  klass   = rb_const_get(mod, rb_intern("Options"));
  options = rb_funcall(klass, rb_intern("new"), 0, NULL);
  rb_obj_instance_eval(0, 0, options);
  params = rb_iv_get(options, "@params");

  /* Check value type */
  if(FIXNUM_P(id))
    {
      int flags = 0, vid = -1;
      VALUE top = Qnil, bottom = Qnil;

      /* Get options */
      if(T_HASH == rb_type(params))
        {
          if(Qtrue == (value = rb_hash_lookup(params, CHAR2SYM("stipple"))))
            flags |= SUB_SCREEN_STIPPLE;
          if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
              CHAR2SYM("top"))))
            {
              top = value; /// Lazy eval
              rb_ary_push(shelter, value); ///< Protect from GC
            }
          if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
              CHAR2SYM("bottom"))))
            {
              bottom = value; ///< Lazy eval
              rb_ary_push(shelter, value); ///< Protect from GC
            }
          if(T_FIXNUM == rb_type(value = rb_hash_lookup(params,
              CHAR2SYM("view"))))
            vid = FIX2INT(value);
        }

      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          if((s = subArrayGet(subtle->screens, FIX2INT(id) - 1)))
            {
              s->flags |= (flags|SUB_RUBY_DATA);
              s->top    = top;
              s->bottom = bottom;

              if(-1 != vid) s->vid = vid;
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for screen");

  return Qnil;
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
  VALUE missing = Qnil, args = Qnil, ret = Qnil;

  rb_scan_args(argc, argv, "1*", &missing, &args);

  /* Dispatch calls */
  if(rb_respond_to(self, rb_to_id(missing))) ///< Intance methods
    ret = rb_funcall2(self, rb_to_id(missing), --argc, ++argv);
  else ///< Instance variables
    {
      char buf[20] = { 0 };
      char *name = (char *)rb_id2name(SYM2ID(missing));

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

/* RubySubletConfig {{{ */
/*
 * call-seq: config -> Hash
 *
 * Get config hash from config
 *
 *  s.config
 *  => { :interval => 30 }
 */

static VALUE
RubySubletConfig(VALUE self)
{
  SubPanel *p = NULL;
  VALUE hash = Qnil;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Get config hash */
      if(NIL_P(hash = rb_hash_lookup(config, CHAR2SYM(p->sublet->name))))
        hash = rb_hash_new();
    }

  return hash;
} /* }}} */

/* RubySubletConfigure {{{ */
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
RubySubletConfigure(VALUE self,
  VALUE name)
{
  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(name))
    {
      SubPanel *p = NULL;

      Data_Get_Struct(self, SubPanel, p);
      if(p)
        {
          VALUE proc = Qnil;

          /* Assume latest sublet */
          proc    = rb_block_proc();
          p->sublet->name = strdup(SYM2CHAR(name));

          /* Define configure method */
          rb_funcall(rb_singleton_class(p->sublet->instance), rb_intern("define_method"),
            2, CHAR2SYM("__configure"), proc);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for configure");

  return Qnil;
} /* }}} */

/* RubySubletHelper {{{ */
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
RubySubletHelper(VALUE self)
{
  SubPanel *p = NULL;

  rb_need_block();

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Instance eval the block */
      rb_yield_values(1, p->sublet->instance);
      rb_obj_instance_eval(0, 0, p->sublet->instance);
    }

  return Qnil;
} /* }}} */

/* RubySubletOn {{{ */
/*
 * call-seq: on(event, &block) -> nil
 *
 * Event block for hooks
 *
 *  on :event do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubySubletOn(VALUE self,
  VALUE event)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(event) && rb_block_given_p())
    {
      SubPanel *p = NULL;

      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      Data_Get_Struct(self, SubPanel, p);
      if(p)
        {
          char buf[64] = { 0 };
          int i, arity = 0, mask = 0;
          VALUE proc = Qnil, sing = Qnil, meth = Qnil;

          RubyMethods methods[] =
          {
            { CHAR2SYM("run"),        CHAR2SYM("__run"),    SUB_PANEL_RUN,    1 },
            { CHAR2SYM("data"),       CHAR2SYM("__data"),   SUB_PANEL_DATA,   2 },
            { CHAR2SYM("watch"),      CHAR2SYM("__watch"),  SUB_PANEL_WATCH,  1 },
            { CHAR2SYM("mouse_down"), CHAR2SYM("__down"),   SUB_PANEL_DOWN,   4 },
            { CHAR2SYM("mouse_over"), CHAR2SYM("__over"),   SUB_PANEL_OVER,   1 },
            { CHAR2SYM("mouse_out"),  CHAR2SYM("__out"),    SUB_PANEL_OUT,    1 }
          };

          /* Collect stuff */
          proc  = rb_block_proc();
          arity = rb_proc_arity(proc);
          sing  = rb_singleton_class(p->sublet->instance);
          meth  = rb_intern("define_method");

          /* Special hooks */
          for(i = 0; LENGTH(methods) > i; i++)
            {
              if(methods[i].sym == event)
                {
                  /* Check proc arity */
                  if(-1 == arity || (1 <= arity && methods[i].arity >= arity))
                    {
                      p->flags |= methods[i].flags;

                      /* Create instance method from proc */
                      rb_funcall(sing, meth, 2, methods[i].real, proc);

                      /* Update window input mask */
                      if(p->flags & SUB_PANEL_DOWN) mask |= ButtonPressMask;
                      if(p->flags & SUB_PANEL_OVER) mask |= EnterWindowMask;
                      if(p->flags & SUB_PANEL_OUT)  mask |= LeaveWindowMask;

                      XSelectInput(subtle->dpy, p->win, mask);

                      return Qnil;
                    }
                  else rb_raise(rb_eArgError, "Wrong number of arguments (%d for %d)",
                    arity, methods[i].arity);
               }
            }

          /* Generic hooks */
          snprintf(buf, sizeof(buf), "__hook_%s", SYM2CHAR(event));
          rb_funcall(sing, meth, 2, CHAR2SYM(buf), proc);

          RubyEvalHook(event, rb_obj_method(p->sublet->instance, CHAR2SYM(buf)));
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for on");

  return Qnil;
} /* }}} */

/* RubySubletGrab {{{ */
/*
 * call-seq: grab(chain, &block) -> nil
 *
 * Add grabs to sublets
 *
 *  grab "A-b" do |s|
 *    puts s.name
 *  end
 */

static VALUE
RubySubletGrab(VALUE self,
  VALUE chain)
{
  rb_need_block();

  /* Check value type */
  if(T_STRING == rb_type(chain))
    {
      SubPanel *p = NULL;

      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      Data_Get_Struct(self, SubPanel, p);
      if(p)
        {
          char buf[64] = { 0 };

          snprintf(buf, sizeof(buf), "__grab_%s", RSTRING_PTR(chain));

          /* Define method */
          rb_funcall(rb_singleton_class(p->sublet->instance),
            rb_intern("define_method"), 2, CHAR2SYM(buf), rb_block_proc());

          RubyEvalGrab(chain, rb_obj_method(p->sublet->instance, CHAR2SYM(buf)));
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for grab");

  return Qnil;
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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p) subPanelRender(p);

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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);

  return p ? rb_str_new2(p->sublet->name) : Qnil;
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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);

  return p ? INT2FIX(p->sublet->interval) : Qnil;
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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(FIXNUM_P(value))
        {
          p->sublet->interval = FIX2INT(value);
          p->sublet->time     = subSubtleTime() + p->sublet->interval;

          if(0 < p->sublet->interval) p->flags |= SUB_PANEL_INTERVAL;
          else p->flags &= ~SUB_PANEL_INTERVAL;
        }
      else rb_raise(rb_eArgError, "Unknown value type `%s'", rb_obj_classname(value));
    }

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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(0 < p->sublet->text->nitems)
        {
          /* Concat string */
          for(i = 0; i < p->sublet->text->nitems; i++)
            {
              SubTextItem *item = (SubTextItem *)p->sublet->text->items[i];

              if(Qnil == string) rb_str_new2(item->data.string);
              else rb_str_cat(string, item->data.string, strlen(item->data.string));
            }
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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      /* Check value type */
      if(T_STRING == rb_type(value))
        {
          p->width = subSharedTextParse(subtle->dpy, subtle->font,
            p->sublet->text, RSTRING_PTR(value)) +
            2 * subtle->pbw + subtle->padding.x + subtle->padding.y;
        }
      else rb_raise(rb_eArgError, "Unknown value type");
    }

  return Qnil;
} /* }}} */

/* RubySubletForegroundWriter {{{ */
/*
 * call-seq: foreground=(value) -> nil
 *
 * Set the default foreground color of a Sublet
 *
 *  sublet.foreground("#ffffff")
 *  => nil
 *
 *  sublet.foreground(Sublet::Color.new("#ffffff"))
 *  => nil
 */

static VALUE
RubySubletForegroundWriter(VALUE self,
  VALUE value)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      p->sublet->fg = subtle->colors.fg_sublets; ///< Set default color

      /* Check value type */
      switch(rb_type(value))
        {
          case T_STRING:
            p->sublet->fg = subSharedParseColor(subtle->dpy, RSTRING_PTR(value));
            break;
          case T_OBJECT:
              {
                VALUE klass = Qnil;

                klass = rb_const_get(mod, rb_intern("Color"));

                if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
                  p->sublet->fg = NUM2LONG(rb_iv_get(self, "@pixel"));
              }
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }

      subPanelRender(p);
    }

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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      p->sublet->bg = subtle->colors.bg_sublets; ///< Set default color

      /* Check value type */
      switch(rb_type(value))
        {
          case T_STRING:
            p->sublet->bg = subSharedParseColor(subtle->dpy, RSTRING_PTR(value));
            break;
          case T_OBJECT:
              {
                VALUE klass = Qnil;

                klass = rb_const_get(mod, rb_intern("Color"));

                if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
                  p->sublet->bg = NUM2LONG(rb_iv_get(self, "@pixel"));
              }
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }

      subPanelRender(p);
    }

  return Qnil;
} /* }}} */

/* RubySubletGeometryReader {{{ */
/*
 * call-seq: gemetry -> Subtlext::Geometry
 *
 * Get geometry of a sublet
 *
 *  win.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
RubySubletGeometryReader(VALUE self)
{
  SubPanel *p = NULL;
  VALUE geometry = Qnil;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      int wx = 0, wy = 0, px = 0, py = 0;
      unsigned int wwidth = 0, wheight = 0, wbw = 0, wdepth = 0;
      unsigned int pwidth = 0, pheight = 0, pbw = 0, pdepth = 0, size = 0;
      Window *wins = NULL, parent = None, wroot = None, proot = None;
      VALUE subtlext = Qnil, klass = Qnil;

      /* Get parent and geometries */
      XGetGeometry(subtle->dpy, p->win, &wroot, &wx, &wy,
        &wwidth, &wheight, &wbw, &wdepth);

      XQueryTree(subtle->dpy, p->win, &wroot, &parent,
        &wins, &size);

      XGetGeometry(subtle->dpy, parent, &proot, &px, &py,
        &pwidth, &pheight, &pbw, &pdepth);

      /* Create geometry object */
      subtlext = rb_const_get(rb_mKernel, rb_intern("Subtlext"));
      klass    = rb_const_get(subtlext, rb_intern("Geometry"));
      geometry = rb_funcall(klass, rb_intern("new"), 4, INT2FIX(px + wx),
        INT2FIX(py + wy), INT2FIX(wwidth), INT2FIX(wheight));

      if(wins) XFree(wins);
    }

  return geometry;
} /* }}} */

/* RubySubletShow {{{ */
/*
 * call-seq: show -> nil
 *
 * Show sublet on panel
 *
 *  sublet.show
 *  => nil
 */

static VALUE
RubySubletShow(VALUE self)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      p->flags &= ~SUB_PANEL_HIDDEN;

      /* Update screens */
      subScreenUpdate();
      subScreenRender();
    }

  return Qnil;
} /* }}} */

/* RubySubletHide {{{ */
/*
 * call-seq: hide -> nil
 *
 * Hide sublet from panel
 *
 *  sublet.hide
 *  => nil
 */

static VALUE
RubySubletHide(VALUE self,
  VALUE value)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      p->flags |= SUB_PANEL_HIDDEN;
      XUnmapWindow(subtle->dpy, p->win);

      /* Update screens */
      subScreenUpdate();
      subScreenRender();
    }

  return Qnil;
} /* }}} */

/* RubySubletHidden {{{ */
/*
 * call-seq: hidden? -> Bool
 *
 * Whether sublet is hidden
 *
 *  puts sublet.display
 *  => true
 */

static VALUE
RubySubletHidden(VALUE self)
{
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);

  return p && p->flags & SUB_PANEL_HIDDEN ? Qtrue : Qfalse;
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
  VALUE ret = Qfalse;
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(!(p->flags & (SUB_PANEL_SOCKET|SUB_PANEL_INOTIFY)) && RTEST(value))
        {
          /* Socket file descriptor or ruby socket */
          if(FIXNUM_P(value) || rb_respond_to(value, rb_intern("fileno")))
            {
              int flags = 0;

              p->flags |= SUB_PANEL_SOCKET;

              /* Get socket file descriptor */
              if(FIXNUM_P(value)) p->sublet->watch = FIX2INT(value);
              else
                {
                  p->sublet->watch = FIX2INT(rb_funcall(value, rb_intern("fileno"),
                    0, NULL));
                }

              XSaveContext(subtle->dpy, subtle->windows.support, p->sublet->watch,
                (void *)p);
              subEventWatchAdd(p->sublet->watch);

              /* Set nonblocking */
              if(-1 == (flags = fcntl(p->sublet->watch, F_GETFL, 0))) flags = 0;
              fcntl(p->sublet->watch, F_SETFL, flags | O_NONBLOCK);

              ret = Qtrue;
            }
#ifdef HAVE_SYS_INOTIFY_H
          else if(T_STRING == rb_type(value)) /// Inotify file
            {
              char *watch = RSTRING_PTR(value);

              /* Create inotify watch */
              if(0 < (p->sublet->watch = inotify_add_watch(subtle->notify,
                  watch, IN_MODIFY)))
                {
                  p->flags |= SUB_PANEL_INOTIFY;

                  XSaveContext(subtle->dpy, subtle->windows.support, p->sublet->watch,
                    (void *)p);
                  subSharedLogDebug("Inotify: Adding watch on %s\n", watch);

                  ret = Qtrue;
                }
              else subSharedLogWarn("Failed adding watch on file `%s': %s\n",
                watch, strerror(errno));
            }
#endif /* HAVE_SYS_INOTIFY_H */
          else subSharedLogWarn("Failed handling unknown value type\n");
        }
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
  SubPanel *p = NULL;

  Data_Get_Struct(self, SubPanel, p);
  if(p)
    {
      if(p->flags & SUB_PANEL_SOCKET) ///< Probably a socket
        {
          XDeleteContext(subtle->dpy, subtle->windows.support, p->sublet->watch);
          subEventWatchDel(p->sublet->watch);

          p->flags &= ~SUB_PANEL_SOCKET;
          p->sublet->watch  = 0;

          ret = Qtrue;
        }
#ifdef HAVE_SYS_INOTIFY_H
      else if(p->flags & SUB_PANEL_INOTIFY) /// Inotify file
        {
          XDeleteContext(subtle->dpy, subtle->windows.support, p->sublet->watch);
          inotify_rm_watch(subtle->notify, p->sublet->watch);

          p->flags &= ~SUB_PANEL_INOTIFY;
          p->sublet->watch  = 0;

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
  VALUE options = Qnil, sublet = Qnil;

  void Init_prelude(void);

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

#ifdef HAVE_RB_ENC_SET_DEFAULT_INTERNAL
  {
    VALUE encoding = Qnil;

    /* FIXME: Fix for ruby 1.9.2p429 borrowed from ruby? */
    (void)rb_filesystem_encoding();

    /* Set encoding */
    encoding = rb_enc_from_encoding(rb_locale_encoding());
    rb_enc_set_default_external(encoding);
  }
#endif /* HAVE_RB_ENC_SET_DEFAULT_INTERNAL */

  /* FIXME: Fake ruby_init_gems(Qtrue) */
  rb_define_module("Gem");
  Init_prelude();

  /* FIXME: Autload seems to be broken in <1.9.2, use dispatcher */
  //rb_autoload(rb_cObject, SYM2ID(CHAR2SYM("Subtlext")), "subtle/subtlext");

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
   * Document-class: Config
   *
   * Config class for DSL evaluation
   */

  config = rb_define_class_under(mod, "Config", rb_cObject);

  /* Class methods */
  rb_define_method(config, "set",     RubyConfigSet,       2);
  rb_define_method(config, "color",   RubyConfigColor ,    2);
  rb_define_method(config, "gravity", RubyConfigGravity,   2);
  rb_define_method(config, "grab",    RubyConfigGrab,     -1);
  rb_define_method(config, "tag",     RubyConfigTag,      -1);
  rb_define_method(config, "view",    RubyConfigView,     -1);
  rb_define_method(config, "on",      RubyConfigOn,        1);
  rb_define_method(config, "sublet",  RubyConfigSublet,    1);
  rb_define_method(config, "screen",  RubyConfigScreen,    1);

  /*
   * Document-class: Options
   *
   * Options class for DSL evaluation
   */

  options = rb_define_class_under(mod, "Options", rb_cObject);

  /* Params list */
  rb_define_attr(options, "params", 1, 1);

  /* Class methods */
  rb_define_method(options, "initialize",     RubyOptionsInit,        0);
  rb_define_method(options, "match",          RubyOptionsMatch,       1);
  rb_define_method(options, "exclude",        RubyOptionsExclude,     1);
  rb_define_method(options, "gravity",        RubyOptionsGravity,     1);
  rb_define_method(options, "method_missing", RubyOptionsDispatcher, -1);

  /*
   * Document-class: Subtle::Sublet
   *
   * Sublet class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* DSL stuff */
  rb_define_method(sublet, "configure",      RubySubletConfigure,  1);
  rb_define_method(sublet, "helper",         RubySubletHelper,     0);
  rb_define_method(sublet, "on",             RubySubletOn,         1);
  rb_define_method(sublet, "grab",           RubySubletGrab,       1);

  /* Class methods */
  rb_define_method(sublet, "method_missing", RubySubletDispatcher,       -1);
  rb_define_method(sublet, "config",         RubySubletConfig,            0);
  rb_define_method(sublet, "render",         RubySubletRender,            0);
  rb_define_method(sublet, "name",           RubySubletNameReader,        0);
  rb_define_method(sublet, "interval",       RubySubletIntervalReader,    0);
  rb_define_method(sublet, "interval=",      RubySubletIntervalWriter,    1);
  rb_define_method(sublet, "data",           RubySubletDataReader,        0);
  rb_define_method(sublet, "data=",          RubySubletDataWriter,        1);
  rb_define_method(sublet, "foreground=",    RubySubletForegroundWriter,  1);
  rb_define_method(sublet, "background=",    RubySubletBackgroundWriter,  1);
  rb_define_method(sublet, "geometry",       RubySubletGeometryReader,    0);
  rb_define_method(sublet, "show",           RubySubletShow,              0);
  rb_define_method(sublet, "hide",           RubySubletHide,              0);
  rb_define_method(sublet, "hidden?",        RubySubletHidden,            0);
  rb_define_method(sublet, "watch",          RubySubletWatch,             1);
  rb_define_method(sublet, "unwatch",        RubySubletUnwatch,           0);

  /* Bypassing garbage collection */
  shelter = rb_ary_new();
  rb_gc_register_address(&shelter);

  subSharedLogDebug("init=ruby\n");
} /* }}} */

 /** subRubyLoadConfig {{{
  * @brief Load config file
  * @param[in]  path  Path to config file
  * @retval  1  Success
  * @retval  0  Failure
  **/

int
subRubyLoadConfig(void)
{
  int state = 0;
  char buf[100] = { 0 }, path[100] = { 0 };
  VALUE str = Qnil , klass = Qnil, conf = Qnil, rargs[3] = { Qnil };
  SubTag *t = NULL;

  /* Check config paths */
  if(subtle->paths.config)
    snprintf(path, sizeof(path), "%s", subtle->paths.config);
  else
    {
      char *tok = NULL, *bufp = buf, *home = getenv("XDG_CONFIG_HOME"),
        *dirs = getenv("XDG_CONFIG_DIRS");

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.config", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s:%s",
        home ? home : path, dirs ? dirs : "/etc/xdg");

      /* Search config in XDG path */
      while((tok = strsep(&bufp, ":")))
        {
          snprintf(path, sizeof(path), "%s/%s/%s", tok, PKG_NAME, PKG_CONFIG);

          if(-1 != access(path, R_OK)) ///< Check if config file exists
            break;

          subSharedLogDebug("Checked config=%s\n", path);
        }
    }

  /* Create default tag */
  if(!(subtle->flags & SUB_SUBTLE_CHECK) &&
      (t = subTagNew("default", NULL)))
    subArrayPush(subtle->tags, (void *)t);

  /* Create and register sublet config hash */
  config = rb_hash_new();
  rb_gc_register_address(&config);

  /* Carefully read config */
  printf("Using config `%s'\n", path);
  str = rb_protect(RubyWrapRead, rb_str_new2(path), &state);
  if(state)
    {
      subSharedLogWarn("Failed reading config `%s'\n", path);
      RubyBacktrace();

      /* Exit when not checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          subSubtleFinish();

          exit(-1);
        }
      else return !state;
    }

  /* Create config */
  klass = rb_const_get(mod, rb_intern("Config"));
  conf  = rb_funcall(klass, rb_intern("new"), 0, NULL);

  /* Carefully eval file */
  rargs[0] = str;
  rargs[1] = rb_str_new2(path);
  rargs[2] = conf;
  rb_protect(RubyWrapEval, (VALUE)&rargs, &state);
  if(state)
    {
      subSharedLogWarn("Failed loading config `%s'\n", path);
      RubyBacktrace();

      /* Exit when not checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          subSubtleFinish();

          exit(-1);
        }
      else return !state;
    }

  /* If not check only lazy eval config values */
  if(!(subtle->flags & SUB_SUBTLE_CHECK)) RubyEvalConfig();

  return !state;
} /* }}} */

 /** subRubyReloadConfig {{{
  * @brief Reload config file
  **/

void
subRubyReloadConfig(void)
{
  int i, *vids = NULL;

  /* Reset before reloading */
  subtle->flags &= (SUB_SUBTLE_DEBUG|SUB_SUBTLE_EWMH|SUB_SUBTLE_RUN|
    SUB_SUBTLE_XINERAMA|SUB_SUBTLE_XRANDR);

  /* Unregister current sublet config hash */
  rb_gc_unregister_address(&config);

  /* Reset sublet panel flags */
  for(i = 0; i < subtle->sublets->ndata; i++)
    {
      SubPanel *p = PANEL(subtle->sublets->data[i]);

      p->flags &= ~(SUB_PANEL_BOTTOM|SUB_PANEL_SPACER1|
        SUB_PANEL_SPACER1| SUB_PANEL_SEPARATOR1|SUB_PANEL_SEPARATOR2);
      p->screen = NULL;
    }

  /* Allocate memory to store current views per screen */
  vids = (int *)subSharedMemoryAlloc(subtle->screens->ndata, sizeof(int));

  /* Reset screen panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      vids[i]   = s->vid; ///< Store views
      s->flags &= ~(SUB_SCREEN_STIPPLE|SUB_SCREEN_PANEL1|SUB_SCREEN_PANEL2);
      subArrayClear(s->panels, True);
    }

  /* Unload fonts */
  if(subtle->font)
    {
      subSharedFontKill(subtle->dpy, subtle->font);
      subtle->font = NULL;
    }

  /* Unset escape grab */
  subtle->escape = NULL;

  /* Clear arrays */
  subArrayClear(subtle->hooks,     True); ///< Must be first
  subArrayClear(subtle->grabs,     True);
  subArrayClear(subtle->gravities, True);
  subArrayClear(subtle->sublets,   False);
  subArrayClear(subtle->tags,      True);
  subArrayClear(subtle->views,     True);

  /* Load and configure */
  subRubyLoadConfig();
  subRubyLoadSublets();
  subRubyLoadPanels();
  subDisplayConfigure();

  /* Restore current views */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      if(vids[i] < subtle->views->ndata)
        SCREEN(subtle->screens->data[i])->vid = vids[i];
    }

  /* Update client tags */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      int flags = 0;
      SubClient *c = CLIENT(subtle->clients->data[i]);

      c->gravity = -1;

      subClientSetTags(c, &flags);
      subClientToggle(c, ~c->flags & flags, True); ///< Toggle flags
    }

  printf("Reloaded config\n");

  subScreenConfigure();
  subScreenUpdate();
  subScreenRender();
  subSubtleFocus(True);

  /* Hook: Reload */
  subHookCall(SUB_HOOK_RELOAD, NULL);

  free(vids);
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
  SubPanel *p = NULL;
  VALUE str = Qnil, klass = Qnil, hash = Qnil, rargs[2] = { Qnil };

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

  /* Carefully read sublet */
  str = rb_protect(RubyWrapRead, rb_str_new2(buf), &state);
  if(state)
    {
      subSharedLogWarn("Failed loading sublet `%s'\n", file);
      RubyBacktrace();

      return;
    }

  /* Create sublet */
  p = subPanelNew(SUB_PANEL_SUBLET);
  klass               = rb_const_get(mod, rb_intern("Sublet"));
  p->sublet->instance = Data_Wrap_Struct(klass, NULL, NULL, (void *)p);

  subArrayPush(subtle->sublets, (void *)p);
  rb_ary_push(shelter, p->sublet->instance); ///< Protect from GC

  /* Carefully eval file */
  rargs[0] = str;
  rargs[1] = rb_str_new2(buf);
  rargs[2] = p->sublet->instance;
  rb_protect(RubyWrapEval, (VALUE)&rargs, &state);
  if(state)
    {
      subSharedLogWarn("Failed loading sublet `%s'\n", file);
      RubyBacktrace();

      subArrayRemove(subtle->sublets, (void *)p);
      subPanelKill(p);

      return;
    }

  /* Check for config hash */
  if(T_HASH == rb_type(hash = rb_hash_lookup(config,
      CHAR2SYM(p->sublet->name))))
    {
      VALUE value = Qnil;

      if(FIXNUM_P(rb_type(value = rb_hash_lookup(hash, CHAR2SYM("interval")))))
        p->sublet->interval = FIX2INT(value);

      if(T_STRING == rb_type(value = rb_hash_lookup(hash,
          CHAR2SYM("foreground"))))
        p->sublet->fg = subSharedParseColor(subtle->dpy, RSTRING_PTR(value));

      if(T_STRING == rb_type(value = rb_hash_lookup(hash,
          CHAR2SYM("background"))))
        p->sublet->bg = subSharedParseColor(subtle->dpy, RSTRING_PTR(value));

    }

  /* Configure sublet */
  subRubyCall(SUB_CALL_CONFIGURE, p->sublet->instance, NULL);

  /* Sanitize interval time */
  if(0 >= p->sublet->interval) p->sublet->interval = 60;

  /* First run */
  if(p->flags & SUB_PANEL_RUN)
    subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);

  printf("Loaded sublet (%s)\n", p->sublet->name);
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
      subSubtleFinish();

      exit(-1);
    }

  subPanelPublish();
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

      subArraySort(subtle->grabs, subGrabCompare);
    }
  else subSharedLogWarn("No sublets found\n");
} /* }}} */

 /** subRubyCall {{{
  * @brief Safely call ruby script
  * @param[in]  type   Script type
  * @param[in]  proc   Script receiver
  * @param[in]  data   Extra data
  * @retval   0  Called script returned false
  * @retval   1  Called script returned true
  * @retval  -1  Calling script failed
  **/

int
subRubyCall(int type,
  unsigned long proc,
  void *data)
{
  int state = 0;
  VALUE result = Qnil, rargs[3] = { Qnil };

  /* Wrap up data */
  rargs[0] = (VALUE)type;
  rargs[1] = proc;
  rargs[2] = (VALUE)data;

  /* Carefully call */
  result = rb_protect(RubyWrapCall, (VALUE)&rargs, &state);
  if(state)
    {
      subSharedLogWarn("Failed calling proc\n");
      RubyBacktrace();

      result = Qnil;
    }

#ifdef DEBUG
  subSharedLogDebug("GC RUN\n");
  rb_gc_start();
#endif /* DEBUG */

  return Qtrue == result ? 1 : (Qfalse == result ? 0 : -1);
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

 /** subRubyReceiver {{{
  * @brief Whether instance is receiver of method
  * @param[in]  instance  Object instance
  * @param[in]  meth      Method
  * @retval  1  Instance is receiver
  * @retval  0  Instance is not receiver
  **/

int
subRubyReceiver(unsigned long instance,
  unsigned long meth)
{
  VALUE receiver = Qnil;

  /* Check object instance */
  if(rb_obj_is_instance_of(meth, rb_cMethod))
    receiver = rb_funcall(meth, rb_intern("receiver"), 0, NULL);

  return receiver == instance;
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

  subSharedLogDebug("finish=ruby\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
