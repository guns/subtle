
 /**
  * @package subtle
  *
  * @file Tag functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
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

/* Public */

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
  if(duplicate && (t = TagFind(name)))
    {
      *duplicate = True;
    }
  else
    {
      /* Create new tag */
      t = TAG(subSharedMemoryAlloc(1, sizeof(SubTag)));
      t->flags = SUB_TYPE_TAG;
      t->name  = strdup(name);
      if(duplicate) *duplicate = False;
    }

  subSharedLogDebug("new=tag, name=%s\n", name);

  return t;
} /* }}} */

 /** subTagCompare {{{
  * @brief Compare tags based on inversion
  * @param[in]  a  A #SubTag
  * @param[in]  b  A #SubTag
  * @return Returns the result of the comparison of both tags
  * @retval  -1  a is smaller
  * @retval  0   a and b are equal
  * @retval  1   a is greater
  **/

int
subTagCompare(const void *a,
  const void *b)
{
  int ret = 0;
  SubTag *t1 = *(SubTag **)a, *t2 = *(SubTag **)b;

  assert(a && b);

  /* Check flags */
  if(t1->flags & SUB_TAG_MATCH_INVERT) ret = -1;
  if(t2->flags & SUB_TAG_MATCH_INVERT) ret = 1;

  return ret;
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
  m->flags = type;

  /* Prevent emtpy regex */
  if(regex && 0 != strncmp("", regex, 1))
    m->regex  = subSharedRegexNew(regex);

  /* Create on demand to safe memory */
  if(NULL == t->matcher) t->matcher = subArrayNew();

  subArrayPush(t->matcher, (void *)m);
} /* }}} */

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

  /* Check if a matcher and client fit together */
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
          (m->flags & SUB_TAG_MATCH_TYPE && c->flags & (m->flags & TYPES_ALL)))
        return m->flags & SUB_TAG_MATCH_INVERT ? False : True;
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
  subSharedPropertySetStrings(subtle->dpy, ROOT,
    subEwmhGet(SUB_EWMH_SUBTLE_TAG_LIST), names, i);

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
  subHookCall((SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_KILL),
    (void *)t);

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
