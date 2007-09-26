------ Clock
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the clock
-- Version: 0.1
-- Date: Sat Mar 03 16:58 CET 2007
----

clock = {}

function clock:teaser()
	return(os.date("%H:%M:%S/%a-%d-%b-%y"))
end

subtle:add_teaser("clock:teaser", 1)
