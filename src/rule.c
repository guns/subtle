
 /**
	* @package subtle
	*
	* @file Rule functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

 /** subRuleNew {{{
	* @brief Create new rule
	* @params[in] size Rule tile size
	* @params[in] tags Tag list
	* @return A #SubRule or \p NULL
	**/

SubRule *
subRuleNew(
	char *tags,
	int size)
{
	int errcode;
	SubRule *r = NULL;

	assert(tags && (size >= 0 && size <= 100));

	r = (SubRule *)subUtilAlloc(1, sizeof(SubRule));
	r->flags	|= SUB_TYPE_RULE;
	r->regex	= (regex_t *)subUtilAlloc(1, sizeof(regex_t));
	r->size		= size;

	/* Thread safe error handling */
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

	printf("Adding rule (%s)\n", tags);

	return(r);
} /* }}} */

 /** subRuleKill {{{
	* @brief Delete rule
	* @params[in] r A #SubRule
	* @param[in] clean Free elements or not
	**/

void
subRuleKill(SubRule *r,
	int clean)
{
	assert(r);

	regfree(r->regex);
	free(r->regex);
	if(r->tile) subTileKill(r->tile, clean);
	free(r);
} /* }}} */
