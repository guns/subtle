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
  * @param[in]  recv  Receiver
  * @return Returns a new #SubHook or \p NULL
  **/

SubHook *
subHookNew(int type,
  unsigned long recv)
{
  SubHook *h = NULL;

  assert(recv);

  /* Create new hook */
  h = HOOK(subSharedMemoryAlloc(1, sizeof(SubHook)));
  h->flags = (SUB_TYPE_HOOK|type);
  h->recv  = recv;

  subSharedLogDebug("new=hook\n");

  return h;
} /* }}} */

 /** subHookCall {{{
  * @brief Emit a hook
  * @param[in]  type  Type of hook
  **/


void
subHookCall(int type,
  void *extra)
{
  int i;

  /* Call matching hooks */
  for(i = 0; i < subtle->hooks->ndata; i++)
    {
      SubHook *h = HOOK(subtle->hooks->data[i]);

      if(h->flags & type) 
        {
          subRubyCall(h->flags & SUB_CALL_PROC ? SUB_CALL_PROC : type, 
            h->recv, extra);
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

