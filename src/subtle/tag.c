
 /**
  * @package subtle
  *
  * @file Tag functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

/* Typedef {{{ */
typedef struct tagmatcher_t
{
  FLAGS   flags;
  regex_t *regex;
} TagMatcher;
/* }}} */

/* TagClear {{{ */
static void
TagClear(SubTag *t)
{
  int i;

  assert(t);

  /* Clear matcher */
  for(i = 0; t->matcher &&  i < t->matcher->ndata; i++)
    {
      TagMatcher *m = (TagMatcher *)t->matcher->data[i];

      if(m->regex) subSharedRegexKill(m->regex);

      free(m);
    }

  subArrayClear(t->matcher, False);
} /* }}} */

/* TagFind {{{ */
SubTag *
TagFind(char *name)
{
  int i;
  SubTag *t = NULL;

  assert(name);

  /* Linear search */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      t = TAG(subtle->tags->data[i]);

      if(0 == strcmp(t->name, name)) return t;
    }

  return NULL;
} /* }}} */

 /** subTagNew {{{
  * @brief Create new tag
  * @param[in]   name       Name of the tag
  * @param[out]  duplicate  Added twice
  * @return Returns a #SubTag or \p NULL
  **/

SubTag *
subTagNew(char *name,
  int *duplicate)
{
  SubTag *t = NULL;

  assert(name);

  /* Check if tag already exists */
  if((t = TagFind(name)))
    {
      TagClear(t);
      if(duplicate) *duplicate = True;
    }
  else
    {
      /* Create new tag */
      t = TAG(subSharedMemoryAlloc(1, sizeof(SubTag)));
      t->name  = strdup(name);
      if(duplicate) *duplicate = False;
    }

  t->flags = SUB_TYPE_TAG;

  subSharedLogDebug("new=tag, name=%s\n", name);

  return t;
} /* }}} */

 /** subTagRegex {{{
  * @brief Add regex to tag
  * @param[in]  t      A #SubTag
  * @param[in]  type   Matcher type
  * @param[in]  regex  Regex string
  **/

void
subTagRegex(SubTag *t,
  int type,
  char *regex)
{
  TagMatcher *m = NULL;

  assert(t);

  /* Create new matcher */
  m = (TagMatcher *)subSharedMemoryAlloc(1, sizeof(TagMatcher));
  m->flags |= (SUB_TYPE_UNKNOWN|type);

  /* Prevent emtpy regex */
  if(regex && 0 != strncmp("", regex, 1))
    m->regex  = subSharedRegexNew(regex);

  /* Create on demand */
  if(NULL == t->matcher) t->matcher = subArrayNew();

  subArrayPush(t->matcher, (void *)m);
}
 /** subTagMatch {{{
  * @brief Check whether client matches tag
  * @param[in]  t  A #SubTag
  * @param[in]  c  A #SubClient
  * @retval  True  Client matches
  * @retval  False Client doesn't match
  **/

int
subTagMatch(SubTag *t,
  SubClient *c)
{
  int i;

  assert(t && c);

  /* Check if client matches */
  for(i = 0; t->matcher && i < t->matcher->ndata; i++)
    {
      TagMatcher *m = (TagMatcher *)t->matcher->data[i];

      /* Complex matching */
      if((m->flags & SUB_TAG_MATCH_NAME && c->name &&
            subSharedRegexMatch(m->regex, c->name)) ||
          (m->flags & SUB_TAG_MATCH_INSTANCE && c->instance &&
            subSharedRegexMatch(m->regex, c->instance)) ||
          (m->flags & SUB_TAG_MATCH_CLASS && c->klass &&
            subSharedRegexMatch(m->regex, c->klass)) ||
          (m->flags & SUB_TAG_MATCH_ROLE && c->role &&
            subSharedRegexMatch(m->regex, c->role)) ||
          (m->flags & SUB_TAG_MATCH_TYPE &&
            c->flags & (m->flags & TYPES_ALL)))
        return True;
    }

  return False;
} /* }}} */

 /** subTagPublish {{{
  * @brief Publish tags
  **/

void
subTagPublish(void)
{
  int i;
  char **names = NULL;

  assert(0 < subtle->tags->ndata);

  names = (char **)subSharedMemoryAlloc(subtle->tags->ndata, sizeof(char *));

  for(i = 0; i < subtle->tags->ndata; i++)
    names[i] = TAG(subtle->tags->data[i])->name;

  /* EWMH: Tag list */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_TAG_LIST, names, i);

  subSharedLogDebug("publish=tags, n=%d\n", i);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(names);
} /* }}} */

 /** subTagKill {{{
  * @brief Delete tag
  * @param[in]  t  A #SubTag
  **/

void
subTagKill(SubTag *t)
{
  assert(t);

  /* Hook: Kill */
  subHookCall(SUB_HOOK_TAG_KILL, (void *)t);

  /* Remove matcher */
  if(t->matcher)
    {
      TagClear(t);
      subArrayKill(t->matcher, False);
    }

  free(t->name);
  free(t);

  subSharedLogDebug("kill=tag\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
