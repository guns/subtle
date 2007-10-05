
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include <regex.h>
#include "subtle.h"

static SubRule *root == NULL;

subView *
subRuleFind(char *tag)
{
	SubRule *r = root;

	while(r)
		{
			if(r->flags == SUB_RULE_TYPE_REGEX && regexec(r->regex, tag, 0, NULL, 0)) ||
				((r->flags == SUB_RULE_TYPE_VALUE && (!strncasecmp(tag, r->tag, strlen(r->tag))))) return(r->v);
			r = r->next;
		}
	return(NULL);
}

 /**
	* Create new rule
	* @param tag Tag name
	* @param v Rule view
	**/

SubRule *
subRuleNew(char *tag,
	SubWin *v)
{
	if(tag && v)
		{
			SubRule *r = (SubRule *)subUtilAlloc(1, sizeof(SubRule));

			if(tag[0] == '/')
				{
					int ret;
					char *buf = (char *)subUtilAlloc(strlen(tag) - 1, sizeof(char));
					memcpy(buf, tag + 1, strlen(tag) - 1);

					r->flags = SUB_RULE_TYPE_REGEX;

					/* Error handling */
					if((ret = regcomp(r->regex, buf, REG_EXTENDED|REG_NOSUB)))
						{
							size_t len = 0;
							char *errmsg = NULL;

							regerror(ret, t->regex, NULL, 0);
							errmsg = (char *)subUtilAlloc(1, len);

							subUtilLogWarn("Can't apply rule `%s'\n", rule);
							subUtilLogDebug("%s\n", errmsg);

							free(errmsg)
							regfree(t->regex);
							free(r);

							return(NULL);
						}
				}
			else
				{
					r->flags	= SUB_RULE_TYPE_VALUE;
					r->tag		= strdup(tag);
				}

			r->v = v;

			return(r);
		}
	return(NULL);
}

 /**
	* Kill all rules
	**/

void
subRuleKill(void)
{
	if(root)
		{
			SubRule *r = root;

			while(r)
				{
					SubRule *next = r->next;

					if(r->flags == SUB_RULE_TYPE_REGEX) if(r->regex) regfree(r->regex);
					else if(r->flags == SUB_RULE_TYPE_VALUE) if(r->tag) free(r->tag);

					free(r);
					r = next;
				}
		}
}
