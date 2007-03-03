------ Clock
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the clock
-- Version: 0.1
-- Date: Sat Mar 03 16:58 CET 2007
----

clock = {}

function clock:teaser()
	return(os.date("%a %b %d %H:%M %Y"))
end

subtle:add_teaser("clock:teaser", 60, 22)
