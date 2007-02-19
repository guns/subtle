------ Battery
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the battery state
-- Version: 0.1
-- Date: Mon Feb 19 00:28 CET 2007
----

battery = {
  slot      = 0,
  capacity  = 0,
  remaining = 0,
  rate      = 0
}

teaser = {}

-- Get battery slot and capacity
function battery:first_run()
	local f = io.popen("ls /proc/acpi/battery/")
	battery.slot = string.find(f:read("*l"), "%a+(%d)")
	f:close()

	f = io.open("/proc/acpi/battery/BAT" .. battery.slot .. "/info", "r")
	local info = f:read("*a")
	f:close()

	_, _, battery.capacity = string.find(info, "last full capacity:%s*(%d+).*")
end

-- Get remaining battery in percent
function battery:run()
	local remaining = 0
	local f = io.open("/proc/acpi/battery/BAT" .. battery.slot .. "/state", "r")
	local info = f:read("*a")
	f:close()

  _, _, battery.remaining = string.find(info, "remaining capacity:%s*(%d+).*")
  _, _, battery.rate      = string.find(info, "present rate:%s*(%d+).*")

	return(math.floor(battery.remaining * 100 / battery.capacity))
end

-- Get remaining battery time
function teaser:run()
  local remain  = battery.remaining / battery.rate * 60
  if(tonumber(battery.rate) > 0) then
    return(string.format("(%dh %dmins)", remain / 60, math.floor(remain % 60)))
  else
    return("(line)")
  end
end

battery:first_run()
subtle:add_meter("battery", 10, 4)
subtle:add_teaser("teaser", 10, 12)
