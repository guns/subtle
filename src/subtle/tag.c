
 /**
  * @package subtle
  *
  * @file Tag functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
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

 /** subTagInit {{{
  * @brief Init default tags
  **/
void
subTagInit(void)
{
  int i;
  char *tags[] = { 
    "default", "float", "full", "stick",        ///< Properties
    "top", "bottom", "left", "right", "center"  ///< Gravities
  };

  for(i = 0; LENGTH(tags) > i; i++)
    {
      SubTag *t = subTagNew(tags[i], NULL);
      subArrayPush(subtle->tags, (void *)t);
    }

  subTagPublish();
} /* }}} */

 /** subTagNew {{{
  * @brief Create new tag
  * @param[in]  name   Name of the tag
  * @param[in]  regex  Regex
  * @return Returns a #SubTag or \p NULL
  **/

SubTag *
subTagNew(char *name,
  char *regex)
{
  SubTag *t = NULL;

  assert(name);

  /* Check if tag already exists */
  if((t = TagFind(name)))
    {
      if(regex)
        {
          /* Update regex */
          if(t->preg) subSharedRegexKill(t->preg);
          t->preg = subSharedRegexNew(regex);
        }
      return NULL;
    }

  t = TAG(subSharedMemoryAlloc(1, sizeof(SubTag)));
  t->name  = strdup(name);
  t->flags = SUB_TYPE_TAG;
  if(regex && strncmp("", regex, 1)) t->preg = subSharedRegexNew(regex);

  subSharedLogDebug("new=tag, name=%s\n", name);

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
