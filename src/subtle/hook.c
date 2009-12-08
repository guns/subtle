 /**
  *
  * @package subtle
  *
  * @file Hook functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subHookNew {{{
  * @brief Create new hook
  * @param[in]  type  Type of hook
  * @param[in]  proc  Hook proc
  * @param[in]  data  Extra data
  * @return Returns a new #SubHook or \p NULL
  **/

SubHook *
subHookNew(int type,
  unsigned long proc,
  void *data)
{
  SubHook *h = NULL;

  assert(proc);

  /* Create new hook */
  h = HOOK(subSharedMemoryAlloc(1, sizeof(SubHook)));
  h->flags = (SUB_TYPE_HOOK|type);
  h->proc  = proc;
  h->data  = data;

  subSharedLogDebug("new=hook, proc=%ld, data=%p\n", proc, data);

  return h;
} /* }}} */

 /** subHookCall {{{
  * @brief Emit a hook
  * @param[in]  type  Type of hook
  * @param[in]  data  Hook data
  **/


void
subHookCall(int type,
  void *data)
{
  int i;

  /* Call matching hooks */
  for(i = 0; i < subtle->hooks->ndata; i++)
    {
      SubHook *h = HOOK(subtle->hooks->data[i]);

      if(h->flags & type) 
        {
          subRubyCall(h->flags & SUB_CALL_PROC ? SUB_CALL_PROC : type, 
            h->proc, h->data, data);

          subSharedLogDebug("call=hook, type=%d, proc=%ld, data=%p\n", type, h->proc, data);
        }
    }
} /* }}} */

 /** subHookKill {{{
  * @brief Kill a hook
  * @param[in]  h  A #SubHook
  **/

void
subHookKill(SubHook *h)
{
  assert(h);

  free(h);

  subSharedLogDebug("kill=hook\n");
} /* }}} */

