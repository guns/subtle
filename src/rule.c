
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int nrules = 0;
static SubRule *rules = NULL;

 /**
	* Find a rule
	* @param tag Tag name
	* @return Return the view associated with the rule or NULL
	**/

SubView *
subRuleFind(char *tag)
{
	int i;

	for(i = 0; i < nrules; i++)
		if((rules[i]->flags == SUB_RULE_TYPE_REGEX && regexec(rules[i]->regex, tag, 0, NULL, 0)) ||
			(rules[i]->flags == SUB_RULE_TYPE_VALUE && (!strncasecmp(tag, rules[i]->tag, strlen(rules[i]->tag)))))
				return(rules[i]->v);
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
	SubRuke *r = NULL;

	assert(tag && v);

	r			= (SubRule *)subUtilAlloc(1, sizeof(SubRule));
	r->v	= v;

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

	if(!rules) 
		{
			rules = (SubRule **)realloc(rules, sizeof(SubRule *) * (nrules + 2));
			if(!rules) subUtilLogError("Can't alloc memory. Exhausted?\n");
		}
	rules[++nrules] = r;

	return(r);
}

 /**
	* Kill all rules
	**/

void
subRuleKill(void)
{
	if(root)
		{
			int i;

			for(i = 0; i < nrules; i++)
				{
					if(rules[i]->flags == SUB_RULE_TYPE_REGEX) if(rules[i]->regex) regfree(rules[i]->regex);
					else if(rules[i]->flags == SUB_RULE_TYPE_VALUE) if(rules[i]->tag) free(rules[i]->tag);

					free(rules[i]);
				}
			free(rules);
		}
}
