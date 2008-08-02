
 /**
  * @package subtle
  *
  * @file Tag functions
  * @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subTagNew {{{
  * @brief Create new tag
  * @param[in] name   Name of the tag
  * @param[in] regex  Regex
  * @return Returns a #SubTag or \p NULL
  **/

SubTag *
subTagNew(char *name,
  char *regex)
{
  SubTag *t = NULL;

  assert(name);

  t = TAG(subUtilAlloc(1, sizeof(SubTag)));
  t->name    = strdup(name);
  t->flags  = SUB_TYPE_TAG;
  if(regex) t->preg = subUtilRegexNew(regex);

  printf("Adding tag (%s)\n", name);
  subUtilLogDebug("new=tag, name=%s\n", name);

  return(t);
} /* }}} */

 /** subTagFind {{{
  * @brief Find tag
  * @param[in]   name  Name of tag
  * @param[out]  id    Tag id
  * @return Returns a #SubTag or \p NULL
  **/

SubTag *
subTagFind(char *name,
  int *id)
{
  int i;
  SubTag *t = NULL;

  /* @todo Linear search.. */
  for(i = 0; i < subtle->tags->ndata; i++)
    {
      t = TAG(subtle->tags->data[i]);

      if(!strncmp(t->name, name, strlen(t->name))) 
        {
          if(id) *id = i;
          return(t);
        }
    }
  
  return(NULL);
} /* }}} */

 /** subTagMatch {{{
  * @brief Get matching tags
  * @param[in]  string  String to match
  * @return Returns a bitfield with matching tags
  **/

int
subTagMatch(char *string)
{
  int i, tags = 0;

  assert(string);

  for(i = 0; i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);
      if(t->preg && subUtilRegexMatch(t->preg, string)) tags |= (1L << (i + 1));
    }

  /* Add default tag if no tag matches */
  if(!tags) 
    {
      int id;
      subTagFind("default", &id);
      tags |= (1L << (id + 1));
    }
  
  return(tags);
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

  names = (char **)subUtilAlloc(subtle->tags->ndata, sizeof(char *));
  for(i = 0; i < subtle->tags->ndata; i++) names[i] = TAG(subtle->tags->data[i])->name;

  /* EWMH: Tag list */
  subEwmhSetStrings(DefaultRootWindow(subtle->disp), SUB_EWMH_SUBTLE_TAG_LIST, names, i);

  subUtilLogDebug("publish=tags, n=%d\n", i);

  free(names);
} /* }}} */

 /** subTagKill {{{
  * @brief Delete tag
  * @param[in] t  A #SubTag
  **/

void
subTagKill(SubTag *t)
{
  assert(t);

  printf("Killing tag (%s)\n", t->name);

  if(t->preg) subUtilRegexKill(t->preg);
  free(t->name);
  free(t);

  subUtilLogDebug("kill=tag\n");
} /* }}} */
