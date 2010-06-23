
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

static VALUE shelter = Qnil, mod = Qnil, subtlext = Qnil; ///< Globals
static VALUE top = Qnil, bottom = Qnil;

/* Typedef {{{ */
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
  VALUE lasterr = Qnil, message = Qnil, klass = Qnil;
  VALUE backtrace = Qnil, entry = Qnil;

  /* Fetching backtrace data */
  lasterr   = rb_gv_get("$!");
  message   = rb_obj_as_string(lasterr);
  klass     = rb_class_path(CLASS_OF(lasterr));
  backtrace = rb_funcall(lasterr, rb_intern("backtrace"), 0, NULL);

  /* Print error and backtrace */
  subSharedLogWarn("%s: %s\n", RSTRING_PTR(klass), RSTRING_PTR(message));
  for(i = 0; Qnil != (entry = rb_ary_entry(backtrace, i)); ++i)
    printf("\tfrom %s\n", RSTRING_PTR(entry));

  rb_backtrace();

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

/* RubyHook {{{ */
static void
RubyHook(VALUE event,
  VALUE p,
  void *data)
{
  int i;
  SubHook *h = NULL;

  RubySymbols hooks[] =
  {
    { CHAR2SYM("start"),            SUB_HOOK_START            },
    { CHAR2SYM("exit"),             SUB_HOOK_EXIT             },
    { CHAR2SYM("tile"),             SUB_HOOK_TILE             },
    { CHAR2SYM("reload"),           SUB_HOOK_RELOAD           },
    { CHAR2SYM("client_create"),    SUB_HOOK_CLIENT_CREATE    },
    { CHAR2SYM("client_configure"), SUB_HOOK_CLIENT_CONFIGURE },
    { CHAR2SYM("client_focus"),     SUB_HOOK_CLIENT_FOCUS     },
    { CHAR2SYM("client_gravity"),   SUB_HOOK_CLIENT_GRAVITY   },
    { CHAR2SYM("client_kill"),      SUB_HOOK_CLIENT_KILL      },
    { CHAR2SYM("tag_create"),       SUB_HOOK_TAG_CREATE       },
    { CHAR2SYM("tag_kill"),         SUB_HOOK_TAG_KILL         },
    { CHAR2SYM("view_create"),      SUB_HOOK_VIEW_CREATE      },
    { CHAR2SYM("view_configure"),   SUB_HOOK_VIEW_CONFIGURE   },
    { CHAR2SYM("view_jump"),        SUB_HOOK_VIEW_JUMP        },
    { CHAR2SYM("view_kill"),        SUB_HOOK_VIEW_KILL        }
  };

  if(subtle->flags & SUB_SUBTLE_CHECK) return; ///< Skip on check

  /* Generic hooks */
  for(i = 0; LENGTH(hooks) > i; i++)
    {
      if(hooks[i].sym == event)
        {
          /* Create new hook */
          if((h = subHookNew(hooks[i].flags|SUB_CALL_HOOKS, p, data)))
            {
              subArrayPush(subtle->hooks, (void *)h);
              rb_ary_push(shelter, p); ///< Protect from GC
            }
        }
    }
} /* }}} */

/* RubyLoadPanel {{{ */
void
RubyLoadPanel(VALUE ary,
  int position,
  RubyPanels *panels)
{
  /* Check value type */
  if(T_ARRAY == rb_type(ary))
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
      if(SUB_SUBTLE_PANEL2 == position)
        {
          flags |= SUB_PANEL_BOTTOM;
          panel  = subtle->windows.panel2;
        }

      /* Parse array */
      for(i = 0; Qnil != (entry = rb_ary_entry(ary, i)); ++i)
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
              p              = PANEL(item);
              p->flags      |= flags;
              flags          = 0;
              subtle->flags |= position; ///< Enable this panel

              subArrayPush(subtle->panels, item);
              XReparentWindow(subtle->dpy, p->win, panel, 0, 0);
            }
        }

      /* Add stuff to last item */
      if(p && flags & SUB_PANEL_SPACER1)    p->flags |= SUB_PANEL_SPACER2;
      if(p && flags & SUB_PANEL_SEPARATOR1) p->flags |= SUB_PANEL_SEPARATOR2;
    }
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

/* Wrap */

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
  Window panel = subtle->windows.panel1;
  RubyPanels panels[] =
  {
    { CHAR2SYM("views"), &subtle->windows.views },
    { CHAR2SYM("title"), &subtle->windows.title },
    { CHAR2SYM("tray"),  &subtle->windows.tray  }
  };

  /* Load actual panels */
  RubyLoadPanel(top,    SUB_SUBTLE_PANEL1, panels);
  RubyLoadPanel(bottom, SUB_SUBTLE_PANEL2, panels);

  /* Update sizes */
  if(0 < subtle->pbw)
    {
      subtle->th = subtle->font->height + 2 * subtle->pbw;
      subDisplayConfigure();

      subViewUpdate();
      subSubletUpdate();
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
      if(rargs[2]) ///< Sublet
        {
          SubSublet *s = SUBLET(rargs[2]);

          rb_funcall(rargs[1], rb_intern("call"),
            MINMAX(rb_proc_arity(rargs[1]), 1, 2), s->instance,
            RubyConvert((VALUE *)rargs[3]));
        }
      else
        rb_funcall(rargs[1], rb_intern("call"),
          MINMAX(rb_proc_arity(rargs[1]), 0, 1), 
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
          case SUB_CALL_SUBLET_UNLOAD: /* {{{ */
            rb_funcall(rargs[1], rb_intern("__unload"), 1, rargs[1]);
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

/* RubyWrapSandboxEval {{{ */
static VALUE
RubyWrapSandboxEval(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  rb_obj_instance_eval(1, rargs, rargs[1]);

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
  ID id = SYM2ID(missing);
  VALUE ret = Qnil;

  subRubyLoadSubtlext(); ///< Load subtlext on demand

  /* Check if subtlext has this symbol */
  if(rb_const_defined(rb_mKernel, id))
    ret = rb_const_get(rb_mKernel, id);
  else
    {
      char *name = (char *)rb_id2name(id);

      rb_raise(rb_eStandardError, "Failed finding constant `%s'", name);
    }

  return ret;
} /* }}} */

/* Sandbox */

/* RubySandboxConfigure {{{ */
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
RubySandboxConfigure(VALUE self,
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

/* RubySandboxHelper {{{ */
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
RubySandboxHelper(VALUE self)
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

/* RubySandboxOn {{{ */
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
RubySandboxOn(VALUE self,
  VALUE event)
{
  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(event))
    {
      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      /* Sublet hooks */
      if(0 < subtle->sublets->ndata)
        {
          int i, arity = 0, mask = 0;
          VALUE p = Qnil, sing = Qnil, meth = Qnil;
          SubSublet *s = NULL;

          RubyMethods methods[] =
          {
            { CHAR2SYM("run"),        CHAR2SYM("__run"),    SUB_SUBLET_RUN,    1 },
            { CHAR2SYM("unload"),     CHAR2SYM("__unload"), SUB_SUBLET_UNLOAD, 1 },
            { CHAR2SYM("watch"),      CHAR2SYM("__watch"),  SUB_SUBLET_WATCH,  1 },
            { CHAR2SYM("mouse_down"), CHAR2SYM("__down"),   SUB_SUBLET_DOWN,   4 },
            { CHAR2SYM("mouse_over"), CHAR2SYM("__over"),   SUB_SUBLET_OVER,   1 },
            { CHAR2SYM("mouse_out"),  CHAR2SYM("__out"),    SUB_SUBLET_OUT,    1 }
          };

          /* Since loading is linear we use the last sublet */
          p     = rb_block_proc();
          arity = rb_proc_arity(p);
          s     = SUBLET(subtle->sublets->data[subtle->sublets->ndata - 1]);
          sing  = rb_singleton_class(s->instance);
          meth  = rb_intern("define_method");

          /* Generic hooks */
          RubyHook(event, p, (void *)s);

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

          /* Hook specific stuff */
          if(s->flags & SUB_SUBLET_DOWN) mask |= ButtonPressMask;
          if(s->flags & SUB_SUBLET_OVER) mask |= EnterWindowMask;
          if(s->flags & SUB_SUBLET_OUT)  mask |= LeaveWindowMask;

          XSelectInput(subtle->dpy, s->win, mask);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for on");

  return Qnil;
} /* }}} */

/* Kernel */

/* RubyKernelSet {{{ */
/*
 * call-seq: set(option, value) -> nil
 *
 * Set options
 *
 *  set :border, 2
 */

static VALUE
RubyKernelSet(VALUE self,
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
            else if(CHAR2SYM("limit") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->limit = FIX2INT(value);
              }
            else if(CHAR2SYM("outline") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->pbw = FIX2INT(value);
              }
            else if(CHAR2SYM("gravity") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gravity = value; ///< Store for later
              }
            else rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
              SYM2CHAR(option));
            break; /* }}} */
          case T_SYMBOL: /* {{{ */
            if(CHAR2SYM("gravity") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  subtle->gravity = value; ///< Store for later
              }
            else rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
              SYM2CHAR(option));
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
            else if(CHAR2SYM("stipple") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK) && Qtrue == value)
                  subtle->flags |= SUB_SUBTLE_STIPPLE;
              }
            else rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
              SYM2CHAR(option));
            break; /* }}} */
          case T_ARRAY: /* {{{ */
            if(CHAR2SYM("top") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    if(RTEST(top)) subRubyRelease(top);
                    top = value;
                    rb_ary_push(shelter, top); ///< Protect from GC
                  }
              }
            else if(CHAR2SYM("bottom") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    if(RTEST(bottom)) subRubyRelease(bottom);
                    bottom = value;
                    rb_ary_push(shelter, bottom); ///< Protect from GC
                  }
              }
            else if(CHAR2SYM("padding") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  RubyGetGeometry(value, &subtle->strut);
              }
            else rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
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

                    subtle->th = subtle->font->height;
                  }
              }
            else if(CHAR2SYM("separator") == option)
              {
                if(!(subtle->flags & SUB_SUBTLE_CHECK))
                  {
                    if(subtle->separator.string) free(subtle->separator.string);
                    subtle->separator.string = strdup(RSTRING_PTR(value));
                    subtle->separator.width  = subSharedTextWidth(subtle->dpy,
                      subtle->font, subtle->separator.string,
                      strlen(subtle->separator.string),
                      NULL, NULL, True);

                    if(0 < subtle->separator.width)
                      subtle->separator.width += 6; ///< Add spacings
                  }
              }
            else rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
              SYM2CHAR(option));
            break; /* }}} */
          default:
            rb_raise(rb_eArgError, "Unknown value type for option `:%s'",
              SYM2CHAR(option));
        }
    }
  else rb_raise(rb_eArgError, "Unknown option name `:%s'", SYM2CHAR(option));

  return Qnil;
} /* }}} */

/* RubyKernelColor {{{ */
/*
 * call-seq: color(color, value) -> nil
 *
 * Set color
 *
 *  color :fg_panel, "#777777"
 */

static VALUE
RubyKernelColor(VALUE self,
  VALUE option,
  VALUE value)
{
  /* Check value type */
  if(T_SYMBOL == rb_type(option) && T_STRING == rb_type(value))
    {
      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      /* Plain if to save register */
      if(CHAR2SYM("fg_panel") == option)
        {
          subtle->colors.fg_panel = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_views") == option)
        {
          subtle->colors.fg_views = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_sublets") == option)
        {
          subtle->colors.fg_sublets = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_focus") == option)
        {
          subtle->colors.fg_focus = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("fg_urgent") == option)
        {
          subtle->colors.fg_urgent = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_panel") == option)
        {
          subtle->colors.bg_panel = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_views") == option)
        {
          subtle->colors.bg_views = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_sublets") == option)
        {
          subtle->colors.bg_sublets = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_focus") == option)
        {
          subtle->colors.bg_focus = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("bg_urgent") == option)
        {
          subtle->colors.bg_urgent = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("border_focus") == option)
        {
          subtle->colors.bo_focus = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("border_normal") == option)
        {
          subtle->colors.bo_normal = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("border_panel") == option)
        {
          subtle->colors.bo_panel = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));
        }
      else if(CHAR2SYM("background") == option)
        {
          subtle->colors.bg = subSharedParseColor(subtle->dpy,
            RSTRING_PTR(value));

          subtle->flags |= SUB_SUBTLE_BACKGROUND;
        }
      else rb_raise(rb_eArgError, "Unkown color name");
    }
  else rb_raise(rb_eArgError, "Unknown value type for color");

  return Qnil;
} /* }}} */

/* RubyKernelGravity {{{ */
/*
 * call-seq: gravity(name, value) -> nil
 *
 * Create gravity
 *
 *  gravity :top_left, [0, 0, 50, 50]
 */

static VALUE
RubyKernelGravity(VALUE self,
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

/* RubyKernelGrab {{{ */
/*
 * call-seq: grab(chain, value) -> nil
 *           grab(chain, &blk)  -> nil
 *
 * Create grabs
 *
 *  grab "A-F1", :ViewJump1
 */

static VALUE
RubyKernelGrab(int argc,
  VALUE *argv,
  VALUE self)
{
  int type = -1;
  SubData data = { None };
  VALUE chain = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &chain, &value);

  if(rb_block_given_p()) value = rb_block_proc(); ///< Get proc

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
            else if(CHAR2SYM("SubletsReload") == value)
              {
                type = SUB_GRAB_SUBLETS_RELOAD;
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
                data = DATA((unsigned long)SUB_MODE_FLOAT);
              }
            else if(CHAR2SYM("WindowFull") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_MODE_FULL);
              }
            else if(CHAR2SYM("WindowStick") == value)
              {
                type = SUB_GRAB_WINDOW_TOGGLE;
                data = DATA((unsigned long)SUB_MODE_STICK);
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
                else if(!strncmp(name, "ScreenJump", 10))
                  {
                    if((name = (char *)name + 10))
                      {
                        type = SUB_GRAB_SCREEN_JUMP;
                        data = DATA((unsigned long)(atol(name) - 1));
                      }
                  }
                else if(!strncmp(name, "WindowScreen", 12))
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
        type = SUB_GRAB_WINDOW_GRAVITY;
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
    }

  /* Check value type */
  if(T_STRING == rb_type(chain) && -1 != type)
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubGrab *g = NULL;

          /* Finally create new grab */
          if((g = subGrabNew(RSTRING_PTR(chain), type, data)))
            subArrayPush(subtle->grabs, (void *)g);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for grab");

  return Qnil;
} /* }}} */

/* RubyKernelTag {{{ */
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
RubyKernelTag(int argc,
  VALUE *argv,
  VALUE self)
{
  int flags = 0, screen = 0;
  unsigned long gravity = 0;
  XRectangle geometry = { 0 };
  VALUE name = Qnil, regex = Qnil, params = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &name, &regex);

  /* Call proc */
  if(rb_block_given_p())
    {
      VALUE klass = Qnil, options = Qnil;

      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 0, NULL);
      rb_obj_instance_eval(0, 0, options);
      params = rb_iv_get(options, "@params");
      regex  = rb_hash_lookup(params, CHAR2SYM("regex"));
    }

  /* Get options */
  if(T_HASH == rb_type(params))
    {
      if(T_SYMBOL == rb_type(value = rb_hash_lookup(params,
        CHAR2SYM("gravity"))) || T_FIXNUM == rb_type(value))
        {
          flags   |= SUB_TAG_GRAVITY;
          gravity  = value;
        }

      if(T_FIXNUM == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("screen"))) && 0 <= FIX2INT(value) &&
          FIX2INT(value) <= subtle->screens->ndata - 1)
        {
          flags  |= SUB_TAG_SCREEN;
          screen  = FIX2INT(value);
        }

      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("geometry"))) &&
          RubyGetGeometry(value, &geometry))
        flags |= SUB_TAG_GEOMETRY;

      /* Check tri-state properties */
      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("geometry"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_MODE_FULL : SUB_MODE_NONFULL);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("float"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_MODE_FLOAT : SUB_MODE_NONFLOAT);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("stick"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_MODE_STICK : SUB_MODE_NONSTICK);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("urgent"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_MODE_URGENT : SUB_MODE_NONURGENT);

      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("resize"))) || Qfalse == value)
        flags |= (Qtrue == value ? SUB_MODE_RESIZE : SUB_MODE_NONRESIZE);

      /* Check matching options */
      if(T_ARRAY == rb_type(value = rb_hash_lookup(params,
          CHAR2SYM("match"))))
        {
          if(Qtrue == rb_ary_includes(value, CHAR2SYM("name")))
            flags |= SUB_TAG_MATCH_NAME;

          if(Qtrue == rb_ary_includes(value, CHAR2SYM("instance")))
            flags |= SUB_TAG_MATCH_INSTANCE;

          if(Qtrue == rb_ary_includes(value, CHAR2SYM("class")))
            flags |= SUB_TAG_MATCH_CLASS;

          if(Qtrue == rb_ary_includes(value, CHAR2SYM("role")))
            flags |= SUB_TAG_MATCH_ROLE;
        }
    }

  /* Enable default if no matcher is present */
  if(!(flags & (SUB_TAG_MATCH_NAME|SUB_TAG_MATCH_INSTANCE| \
      SUB_TAG_MATCH_CLASS|SUB_TAG_MATCH_ROLE)))
    flags |= (SUB_TAG_MATCH_INSTANCE|SUB_TAG_MATCH_CLASS);

  /* Check value type */
  if(T_STRING == rb_type(name) && T_STRING == rb_type(regex))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubTag *t = NULL;

          /* Finally create new tag */
          if((t = subTagNew(RSTRING_PTR(name), RSTRING_PTR(regex))))
            {
              t->flags   |= flags;
              t->gravity  = gravity;
              t->screen   = screen;
              t->geometry = geometry;

              subArrayPush(subtle->tags, (void *)t);
            }
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type for tag");

  return Qnil;
} /* }}} */

/* RubyKernelView {{{ */
/*
 * call-seq: view(name, regex) -> nil
 *           view(name, blk)   -> nil
 *
 * Add a new view
 *
 *  view "foobar", "regex"
 *
 *  tag "foobar" do
 *    regex  "foobar"
 *    dynamic true
 *  end
 */

static VALUE
RubyKernelView(int argc,
  VALUE *argv,
  VALUE self)
{
  int flags = 0;
  VALUE name = Qnil, regex = Qnil, params = Qnil, value = Qnil;

  rb_scan_args(argc, argv, "11", &name, &regex);

  /* Call proc */
  if(rb_block_given_p())
    {
      VALUE klass = Qnil, options = Qnil;

      /* Collect options */
      klass   = rb_const_get(mod, rb_intern("Options"));
      options = rb_funcall(klass, rb_intern("new"), 0, NULL);
      rb_obj_instance_eval(0, 0, options);
      params = rb_iv_get(options, "@params");
      regex  = rb_hash_lookup(params, CHAR2SYM("regex"));
    }

  /* Get options */
  if(T_HASH == rb_type(params))
    {
      if(Qtrue == (value = rb_hash_lookup(params,
          CHAR2SYM("dynamic"))))
        flags |= SUB_VIEW_DYNAMIC;
    }

  /* Check value type */
  if(T_STRING == rb_type(name) && T_STRING == rb_type(regex))
    {
      /* Skip on checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          SubView *v = NULL;

          /* Finally create new view */
          if((v = subViewNew(RSTRING_PTR(name), RSTRING_PTR(regex))))
            {
              v->flags |= flags;

              subArrayPush(subtle->views, (void *)v);
          }
       }
    }
  else rb_raise(rb_eArgError, "Unknown value type for view");

  return Qnil;
} /* }}} */

/* RubyKernelOn {{{ */
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
RubyKernelOn(VALUE self,
  VALUE event)
{
  rb_need_block();

  /* Check value type */
  if(T_SYMBOL == rb_type(event))
    {
      if(subtle->flags & SUB_SUBTLE_CHECK) return Qnil; ///< Skip on check

      RubyHook(event, rb_block_proc(), NULL);
    }
  else rb_raise(rb_eArgError, "Unknown value type for on");

  return Qnil;
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
 * Dispatcher for Sublet instance variables - internal use only
 */

static VALUE
RubyOptionsDispatcher(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE missing = Qnil, arg = Qnil, params = Qnil;

  rb_scan_args(argc, argv, "2", &missing, &arg);

  params = rb_iv_get(self, "@params");

  /* Just store param */
  return rb_hash_aset(params, missing, arg);
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
                VALUE klass = Qnil;

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
  SubSublet *s = NULL;
  VALUE geometry = Qnil;

  Data_Get_Struct(self, SubSublet, s);
  if(s)
    {
      XRectangle geom = { 0 };
      VALUE klass = Qnil;

      subRubyLoadSubtlext(); ///< Load subtlext on demand

      /* Get window geometry */
      subSharedPropertyGeometry(subtle->dpy, s->win, &geom);

      /* Create geometry object */
      klass    = rb_const_get(subtlext, rb_intern("Geometry"));
      geometry = rb_funcall(klass, rb_intern("new"), 4,
        INT2FIX(geom.x), INT2FIX(geom.y),
        INT2FIX(geom.width), INT2FIX(geom.height));
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
 * Hide sublet from panel
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
 * Whether sublet is hidden
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
  VALUE options = Qnil, sandbox = Qnil, sublet = Qnil;

  void Init_prelude(void);

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  /* FIXME: Fake ruby_init_gems(Qtrue) */
  rb_define_module("Gem");
  Init_prelude();

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
   * Document-class: Subtle::Sandbox
   *
   * Sandbox is a module to safely load sublets
   */

  sandbox = rb_define_class_under(mod, "Sandbox", rb_cObject);

  /* Class methods */
  rb_define_method(sandbox, "configure",     RubySandboxConfigure, 1);
  rb_define_method(sandbox, "helper",        RubySandboxHelper,    0);
  rb_define_method(sandbox, "on",            RubySandboxOn,        1);
  rb_define_method(sandbox, "const_missing", RubyObjectDispatcher, 1);

  /*
   * Document-class: Kernel
   *
   * Ruby Kernel class for Sublet DSL
   */

  /* Class methods */
  rb_define_method(rb_mKernel, "set",     RubyKernelSet,       2);
  rb_define_method(rb_mKernel, "color",   RubyKernelColor ,    2);
  rb_define_method(rb_mKernel, "gravity", RubyKernelGravity,   2);
  rb_define_method(rb_mKernel, "grab",    RubyKernelGrab,     -1);
  rb_define_method(rb_mKernel, "tag",     RubyKernelTag,      -1);
  rb_define_method(rb_mKernel, "view",    RubyKernelView,     -1);
  rb_define_method(rb_mKernel, "on",      RubyKernelOn,        1);

  /*
   * Document-class: Options
   *
   * Options class for evaluating DSL procs
   */

  options = rb_define_class_under(mod, "Options", rb_cObject);

  /* Params list */
  rb_define_attr(options, "params", 1, 1);

  /* Class methods */
  rb_define_method(options, "initialize",     RubyOptionsInit,        0);
  rb_define_method(options, "gravity",        RubyOptionsGravity,     1);
  rb_define_method(options, "method_missing", RubyOptionsDispatcher, -1);

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
  rb_define_method(sublet, "geometry",       RubySubletGeometryReader,    0);
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
  * @retval  1  Success
  * @retval  0  Failure
  **/

int
subRubyLoadConfig(void)
{
  int i, state = 0, def = 0;
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

      /* Exit when not checking only */
      if(!(subtle->flags & SUB_SUBTLE_CHECK))
        {
          subSubtleFinish();

          exit(-1);
        }
    }

  if(subtle->flags & SUB_SUBTLE_CHECK) return !state; ///< Exit after config check

  /* Check gravities */
  if(1 == subtle->gravities->ndata)
    {
      subSharedLogError("No gravities found\n");
      subSubtleFinish();

      exit(-1);
    }

  /* Get default gravity */
  if(-1 == (subtle->gravity = RubyGetGravity(subtle->gravity)))
    subtle->gravity = 0;

  subGravityPublish();

  /* Update gravites */
  for(i = 0; i < subtle->grabs->ndata; i++)
    {
      SubGrab *g = GRAB(subtle->grabs->data[i]);

      if(g->flags & SUB_GRAB_WINDOW_GRAVITY)
        {
          int j = 0, k = 0, id = -1, size = 0;
          VALUE entry = Qnil, value = Qnil;

          /* Create gravity string */
          value          = g->data.num;
          size           = FIX2INT(rb_funcall(value, rb_intern("size"), 0, NULL));
          g->data.string = (char *)subSharedMemoryAlloc(size + 1, sizeof(char));

          /* Add gravities */
          for(j = 0; Qnil != (entry = rb_ary_entry(value, j)); j++)
            {
              /* We store ids in a string to ease the whole thing */
              if(-1 != (id = RubyGetGravity(entry)))
                g->data.string[k++] = id + 65; /// Use letters only
              else subSharedLogWarn("Failed finding gravity `%s'\n", SYM2CHAR(entry));
            }

          subRubyRelease(value);
        }
    }

  /* Check grabs */
  if(0 == subtle->grabs->ndata)
    {
      subSharedLogWarn("No grabs found\n");
    }
  else subArraySort(subtle->grabs, subGrabCompare);

  /* Check and update tags */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);

      /* Default tag must be first */
      if(0 == strncmp("default", t->name, 7))
        {
          void *swap = subtle->tags->data[0];

          subtle->tags->data[0] = (void *)t;
          subtle->tags->data[i] = swap;

          def++;
        }

      /* Update gravities */
      if(t->flags & SUB_TAG_GRAVITY)
        if(-1 == (t->gravity = RubyGetGravity(t->gravity)))
          t->gravity = 0;
    }

  /* Create default tag */
  if(0 == def)
    {
      SubTag *t = subTagNew("default", NULL);

      subArrayPush(subtle->tags, (void *)t);
    }

  subTagPublish();

  if(1 == subtle->tags->ndata) subSharedLogWarn("No tags found\n");

  /* Check views */
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

  return 1;
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

  /* Load and configure */
  subRubyLoadConfig();
  subRubyLoadPanels();
  subDisplayConfigure();

  /* Update client tags */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      int flags = 0;
      SubClient *c = CLIENT(subtle->clients->data[i]);

      subClientSetTags(c, &flags);
      subClientToggle(c, ~c->flags & flags); ///< Toggle flags
    }

  /* Reload sublets */
  subRubyReloadSublets();

  printf("Reloaded config\n");

  subViewJump(subtle->views->data[0], True);
  subViewConfigure(subtle->views->data[0], True);

  /* Hook: Reload */
  subHookCall(SUB_HOOK_RELOAD, NULL);
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
  VALUE str = Qnil, rargs[2] = { Qnil }, klass = Qnil, sandbox = Qnil;

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

      return;
    }

  /* Create sublet */
  s = subSubletNew();
  klass       = rb_const_get(mod, rb_intern("Sublet"));
  s->instance = Data_Wrap_Struct(klass, NULL, NULL, (void *)s);

  /* Create sandbox */
  klass   = rb_const_get(mod, rb_intern("Sandbox"));
  sandbox = rb_funcall(klass, rb_intern("new"), 0, NULL);

  subArrayPush(subtle->sublets, s);
  rb_ary_push(shelter, s->instance); ///< Protect from GC

  /* Carefully eval file */
  rargs[0] = str;
  rargs[1] = sandbox;
  rb_protect(RubyWrapSandboxEval, (VALUE)&rargs, &state);
  if(state)
    {
      subSharedLogWarn("Failed loading sublet `%s'\n", file);
      RubyBacktrace();

      subArrayRemove(subtle->sublets, (void *)s);
      subSubletKill(s);

      return;
    }

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
      subSubtleFinish();

      exit(-1);
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
      subSubtleFinish();

      exit(-1);
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
  VALUE result = Qnil, rargs[4] = { Qnil };

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
