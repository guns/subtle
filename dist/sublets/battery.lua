------ Battery
-- Author: Christoph Kappel
-- Contact: unexist@dorfelite.net
-- Description: Show the battery state
-- Version: 0.1
-- Date: Sat Mar 03 16:58 CET 2007
-- $Id: sublets/battery.lua,v 504 2008/03/21 11:26:55 unexist $
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
function battery:teaser()
	local f = io.open("/proc/acpi/battery/BAT" .. battery.slot .. "/state", "r")
	local info = f:read("*a")
	f:close()

  _, _, battery.remaining = string.find(info, "remaining capacity:%s*(%d+).*")
  _, _, battery.rate      = string.find(info, "present rate:%s*(%d+).*")
	_, _, battery.state			= string.find(info, "charging state:%s*(%a+).*")

	-- Get remaining battery time
  local remain = battery.remaining / battery.rate * 60
	local ac = ""
	if(battery.state == "charged") then
		ac = "AC: on"
	elseif(battery.state == "discharging") then
    ac = string.format("%dh %dm", remain / 60, math.floor(remain % 60))
  elseif(battery.state == "charging") then
		ac = "AC: chg"
  end

	return("Bat: " .. math.floor(battery.remaining * 100 / battery.capacity) .. "% " .. ac)
end

battery:first_run()
subtle:add_teaser("battery:teaser", 60)
