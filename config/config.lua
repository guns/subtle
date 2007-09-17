-- Options config
Options = {
	Border	= 2													-- Border size of the windows
}

-- Font config
Font = {
	Face	= "lucidatypewriter",					-- Font face for the text
	Style	= "medium",										-- Font style (medium|bold|italic)
	Size	= 12													-- Font size
}

-- Color config
Colors = {
	Font				= "#ffffff",						-- Color of the font
	Border			= "#ffffff",						-- Color of the border/tiles
	Normal			= "#9AA38A",						-- Color of the inactive windows
	Focus				= "#A7E737",						-- Color of the focussed window
	Collapse		= "#A0B67A",						-- Color of collapsed windows
	Background	= "#9A9C95"							-- Color of the root background
}

-- Key config
-- Modifier keys:
-- A = Alt key
-- S = Shift key
-- C = Control key
-- W = Super (Windows key)
-- M = Meta key
Keys = {
	AddVertTile			= "A-v",						-- Add a vertical tile
	AddHorzTile			= "A-h",						-- Add a horizontal tile
	DeleteWindow		= "A-d",						-- Delete a window
	ToggleCollapse	= "A-c",						-- Toggle collapse
	ToggleRaise			= "A-r",						-- Toggle raise
	ToggleFull			= "A-f",						-- Toggle fullscreen
	ToggleWeight		= "A-w",						-- Toggle weight
	TogglePile			= "S-A-p",					-- Toggle pile
	ToggleLayout		= "S-A-l",					-- Toggle tile layout
	NextDesktop			= "A-Right",				-- Switch to next desktop
	PreviousDesktop	= "A-Left",					-- Switch to previous desktop
	MoveToDesktop1	= "S-A-1",					-- Move window to desktop 1
	MoveToDesktop2	= "S-A-2",					-- Move window to desktop 2
	MoveToDesktop3	= "S-A-3",					-- Move window to desktop 3
	MoveToDesktop4	= "S-A-4",					-- Move window to desktop 4
	["rxvt +sb"]					= "S-F1",			-- Exec a term
	["xterm +sb"]					= "S-F2",			-- Exec a term
	["mplayer -nosound ~/foo.avi &>/dev/null"] = "S-F3",
	["firefox"] = "S-F4"
}
