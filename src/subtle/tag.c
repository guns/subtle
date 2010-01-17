
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

/* TagFind {{{ */
SubTag *
TagFind(char *name)
{
  int i;
  SubTag *t = NULL;

  assert(name);

  /* @todo Linear search.. */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      t = TAG(subtle->tags->data[i]);

      if(!strncmp(t->name, name, strlen(t->name)))
        return t;
    }
  
  return NULL;
} /* }}} */

 /** subTagNew {{{
  * @brief Create new tag
  * @param[in]  name     Name of the tag
  * @param[in]  regex    Regex
  * @return Returns a #SubTag or \p NULL
  **/

SubTag *
subTagNew(char *name,
  char *regex)
{
  SubTag *t = NULL;

  assert(name);

  /* Check if tag already exists */
  if((t = TagFind(name))) return NULL;

  t = TAG(subSharedMemoryAlloc(1, sizeof(SubTag)));
  t->name  = strdup(name);
  t->flags = SUB_TYPE_TAG;

  if(regex && strncmp("", regex, 1)) t->preg = subSharedRegexNew(regex);

  subSharedLogDebug("new=tag, name=%s, regex=%s\n", name, regex);

  return t;
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

  if(t->preg) subSharedRegexKill(t->preg);
  free(t->name);
  free(t);

  subSharedLogDebug("kill=tag\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
