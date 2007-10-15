
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

 /**
	* Find a rule
	* @param r A #SubRule
	* @param tag Tag name
	* @return Return either nonzero on match or zero
	**/

int
subRuleMatch(SubRule *r,
	char *tag)
{
	assert(r && tag);
	return(regexec(r->regex, tag, 0, NULL, 0) ? 1 : 0);
}

 /**
	* Create new rule
	* @param tag Tag name
	**/

SubRule *
subRuleNew(char *tags)
{
	int errcode;
	SubRule *r = NULL;

	assert(tags);

	r	= (SubRule *)subUtilAlloc(1, sizeof(SubRule));
	r->regex = (regex_t *)subUtilAlloc(1, sizeof(regex_t));

	/* Stupid error handling */
	if((errcode = regcomp(r->regex, tags, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
		{
			size_t errsize = regerror(errcode, r->regex, NULL, 0);
			char *errbuf = (char *)subUtilAlloc(1, errsize);
			regerror(errcode, r->regex, errbuf, errsize);

			subUtilLogWarn("Can't compile regex `%s'\n", tags);
			subUtilLogDebug("%s\n", errbuf);

			free(errbuf);
			regfree(r->regex);
			free(r->regex);
			free(r);
			return(NULL);
		}

	subUtilLogDebug("Rule: tag=%s\n", tags);
	return(r);
}

 /**
	* Delete rule
	* @param r A #SubRule
	**/

void
subRuleDelete(SubRule *r)
{
	assert(r);
	regfree(r->regex);
	free(r->regex);
	free(r);
}
