------ Clock
-- Author: Christoph Kappel
-- Contact: unexist@dorfelite.net
-- Description: Show the clock
-- Version: 0.1
-- Date: Sat Mar 03 16:58 CET 2007
-- $Id: sublets/clock.lua,v 545 2008/04/17 20:40:57 unexist $
----

clock = {}

function clock:teaser()
	return(os.date("%d%m%y%H%M"))
end

subtle:add_teaser("clock:teaser", 60)
