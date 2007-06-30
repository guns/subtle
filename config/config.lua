-- Options config
options = {
	border	= 2													-- Border size of the windows
}

-- Font config
font = {
	face	= "lucidatypewriter",					-- Font face for the text
	style	= "medium",										-- Font style (medium|bold|italic)
	size	= 12													-- Font size
}

-- Color config
colors = {
	font				= "#ffffff",						-- Color of the font
	border			= "#ffffff",						-- Color of the border/tiles
	normal			= "#cecfce",						-- Color of the inactive windows
	focus				= "#ffcf00",						-- Color of the focussed window
	shade				= "#bac5ce",						-- Color of shaded windows
	background	= "#525152"							-- Color of the root background
}

colors1 = {
	font				= "#ffffff",						-- Color of the font
	border			= "#ffffff",						-- Color of the border/tiles
	normal			= "#CFDCE6",						-- Color of the inactive windows
	focus				= "#6096BF",						-- Color of the focussed window
	shade				= "#bac5ce",						-- Color of shaded windows
	background	= "#596F80"							-- Color of the root background
}

colors2 = {
  font        = "#ffffff",            -- Color of the font
  border      = "#ffffff",            -- Color of the border/tiles
  normal      = "#848684",            -- Color of the inactive windows
  focus	      = "#63659c",            -- Color of focussed window
	shade				= "#FFE6E6",						-- Color of shaded windows
  background  = "#adaead"  						-- Color of the root background
}

-- Key config
keys = {
	add_vtile				= "S-C-v",	-- Add a vertical tile
	add_htile				= "S-C-h",	-- Add a horizontal tile
	del_win					= "S-C-d",	-- Delete a window
	collapse_win		= "S-C-c",	-- Collapse a window
	raise_win				= "S-C-r",	-- Raise a window
	["rxvt +sb"]		= "S-C-s",		-- Exec an app
	["mutt"]				= "S-F1"
}
