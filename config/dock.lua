-- Dock
function clock()
	return(os.date("%c"))
end

function loadavg()
	io.input("/proc/loadavg")
	load1, load2, load3 = io.read("*number", "*number", "*numer")
	return(string.format("%.2f %.2f %.2f", load1, load2, load3))
end

add("clock", 2, 25)
add("loadavg", 20, 15)
