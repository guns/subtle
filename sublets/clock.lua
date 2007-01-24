------ Clock
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Shows the clock
-- Version: 0.1
-- Date: Wed Jan 24 19:42 CET 2007
----
--
function clock()
	return(os.date("%c"))
end

add("clock", 2, 25)
