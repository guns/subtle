---- Loadavg
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Shows the average load
-- Version: 0.1
-- Date: Wed Jan 24 19:42 CET 2007
----

function loadavg()
	io.input("/proc/loadavg")
	load1, load2, load3 = io.read("*number", "*number", "*numer")
	return(string.format("%.2f %.2f %.2f", load1, load2, load3))
end

add("loadavg", 20, 15)
