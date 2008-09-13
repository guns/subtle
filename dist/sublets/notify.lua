---- Notify
-- Author: Christoph Kappel
-- Contact: unexist@dorfelite.net
-- Description: Show the content of a file
-- Version: 0.1
-- Date: Wed Sept 26 13:45 CET 2007
-- $Id: sublets/notify.lua,v 504 2008/03/21 11:26:55 unexist $
----

notify = {
	file = "/tmp/watch.txt"
}

function notify:watch()
	local f = io.input(notify.file)
	data = f:read("*l")
	f:close()

	return(data)
end

subtle:add_watch("notify:watch", notify.file)
