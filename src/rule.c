
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Header$
	**/

#include "subtle.h"

SubRule *
subRuleNew(int size,
	char *tags)
{
	int errcode;
	SubRule *r = NULL;

	assert(tags);

	r = (SubRule *)subUtilAlloc(1, sizeof(SubRule));
	r->size		= size;
	r->regex	= (regex_t *)subUtilAlloc(1, sizeof(regex_t));

	/* Thread safe error handling.. */
	if((errcode = regcomp(v->regex, tags, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
		{
			size_t errsize = regerror(errcode, v->regex, NULL, 0);
			char *errbuf = (char *)subUtilAlloc(1, errsize);

			regerror(errcode, v->regex, errbuf, errsize);

			subUtilLogWarn("Can't compile regex `%s'\n", tags);
			subUtilLogDebug("%s\n", errbuf);

			free(errbuf);
			regfree(v->regex);
			free(v->regex);
			free(v);

			return(NULL);
		}

}
