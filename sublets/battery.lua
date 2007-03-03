------ Battery
-- Author: Christoph Kappel
-- Contact: unexist@hilflos.org
-- Description: Show the battery state
-- Version: 0.1
-- Date: Sat Mar 03 16:58 CET 2007
----

battery = {
  slot      = 0,
  capacity  = 0,
  remaining = 0,
  rate      = 0,
	state			= ""
}

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
function battery:meter()
	local f = io.open("/proc/acpi/battery/BAT" .. battery.slot .. "/state", "r")
	local info = f:read("*a")
	f:close()

  _, _, battery.remaining = string.find(info, "remaining capacity:%s*(%d+).*")
  _, _, battery.rate      = string.find(info, "present rate:%s*(%d+).*")
	_, _, battery.state			= string.find(info, "charging state:%s*(%a+).*")

	return(math.floor(battery.remaining * 100 / battery.capacity))
end

-- Get remaining battery time
function battery:teaser()
  local remain  = battery.remaining / battery.rate * 60
  if(tonumber(battery.rate) > 0) then
    return(string.format("(%dh %dmins)", remain / 60, math.floor(remain % 60)))
  elseif(state == "charging") then
		return("(charging)")
	else
    return("(on-line)")
  end
end

battery:first_run()
subtle:add_meter("battery:meter", 60, 4)
subtle:add_teaser("battery:teaser", 60, 12)
