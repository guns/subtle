---- Loadavg
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the average load
-- Version: 0.1
-- Date: Sat Mar 03 16:58 CET 2007
----

loadavg = {}

function loadavg:teaser()
	f = io.input("/proc/loadavg")
	load1, load2, load3 = io.read("*number", "*number", "*number")
	f:close()

	return(string.format("%.2f %.2f %.2f", load1, load2, load3))
end

subtle:add_teaser("loadavg:teaser", 20)
