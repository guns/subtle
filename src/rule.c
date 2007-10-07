
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static SubRule *root = NULL;

 /**
	* Find a rule
	* @param tag Tag name
	* @return Return the view associated with the rule or NULL
	**/

SubView *
subRuleFind(char *tag)
{
	SubRule *r = root;

	while(r)
		{
			if((r->flags == SUB_RULE_TYPE_REGEX && regexec(r->regex, tag, 0, NULL, 0)) ||
				(r->flags == SUB_RULE_TYPE_VALUE && (!strncasecmp(tag, r->tag, strlen(r->tag))))) return(r->v);
			r = r->next;
		}
	return(NULL);
}

 /**
	* Create new rule
	* @param tag Tag name
	* @param v A #SubView
	**/

SubRule *
subRuleNew(char *tag,
	SubView *v)
{
	if(tag && v)
		{
			SubRule *tail = NULL, *r = (SubRule *)subUtilAlloc(1, sizeof(SubRule));
			r->v = v;

			if(tag[0] == '/')
				{
					int errcode;
					char *buf = (char *)subUtilAlloc(strlen(tag) - 1, sizeof(char));
					strncpy(buf, tag + 1, strlen(tag) - 2);

					r->flags = SUB_RULE_TYPE_REGEX;
					r->regex = (regex_t *)subUtilAlloc(1, sizeof(regex_t));

					/* Stupid error handling */
					if((errcode = regcomp(r->regex, buf, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
						{
							size_t errsize = regerror(errcode, r->regex, NULL, 0);
							char *errbuf = (char *)subUtilAlloc(1, errsize);
							regerror(errcode, r->regex, errbuf, errsize);

							subUtilLogWarn("Can't compile regex `%s'\n", tag);
							subUtilLogDebug("%s\n", errbuf);

							free(buf);
							free(errbuf);
							regfree(r->regex);
							free(r);

							return(NULL);
						}
					free(buf);
				}
			else
				{
					r->flags	= SUB_RULE_TYPE_VALUE;
					r->tag		= strdup(tag);
				}

			subUtilLogDebug("Rule: view=%s, tag=%s, type=%s\n", v->name, tag,
				r->flags & SUB_RULE_TYPE_REGEX ? "regex" : "value");

			if(!root) root = r;
			else
				{
					tail = root;
					while(tail->next) tail = tail->next;
					tail->next = r;
				}

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
