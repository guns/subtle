------ Clock
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the clock
-- Version: 0.1
-- Date: Mon Feb 19 00:29 CET 2007
----

clock = {}

function clock:run()
	return(os.date("%a %b %d %H:%M %Y"))
end

subtle:add_teaser("clock", 2, 22)
