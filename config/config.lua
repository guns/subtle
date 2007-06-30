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
	AddVertTile			= "S-C-v",					-- Add a vertical tile
	AddHorzTile			= "S-C-h",					-- Add a horizontal tile
	DeleteWindow		= "S-C-d",					-- Delete a window
	ToggleCollapse	= "S-C-c",					-- Collapse a window
	ToggleRaise			= "S-C-r",					-- Raise a window
	["rxvt +sb"]		= "S-C-s",					-- Exec an app
	["mutt"]				= "S-F1"
}
