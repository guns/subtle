---- Loadavg
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the average load
-- Version: 0.1
-- Date: Mon Feb 19 00:41 CET 2007
----

loadavg = {}

function loadavg:run()
	io.input("/proc/loadavg")
	load1, load2, load3 = io.read("*number", "*number", "*numer")
	return(string.format("%.2f %.2f %.2f", load1, load2, load3))
end

subtle:add_teaser("loadavg", 20, 15)
