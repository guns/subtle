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
	Normal			= "#cecfce",						-- Color of the inactive windows
	Focus				= "#ffcf00",						-- Color of the focussed window
	Collapse		= "#bac5ce",						-- Color of collapsed windows
	Background	= "#525152"							-- Color of the root background
}

-- Key config
Keys = {
	AddVertTile			= "A-v",						-- Add a vertical tile
	AddHorzTile			= "A-h",						-- Add a horizontal tile
	DeleteWindow		= "A-d",						-- Delete a window
	ToggleCollapse	= "A-c",						-- Collapse a window
	ToggleRaise			= "A-r",						-- Raise a window
	NextDesktop			= "S-Page_Up",			-- Switch to next desktop
	PreviousDesktop	= "S-Page_Down",		-- Switch to previous desktop
	MoveToDesktop1	= "S-A-1",					-- Move window to desktop 1
	MoveToDesktop2	= "S-A-2",					-- Move window to desktop 2
	MoveToDesktop3	= "S-A-3",					-- Move window to desktop 3
	MoveToDesktop4	= "S-A-4",					-- Move window to desktop 4
	["rxvt +sb"]		= "S-F1",						-- Exec an app
	["mutt"]				= "S-F2"
}
