--
-- subtle - window manager
-- Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
--
-- This program can be distributed under the terms of the GNU GPL.
-- See the file COPYING.
--
-- $Id$
--

-- Options config {{{
Options = {
	Border	= 2													-- Border size of the windows
} -- }}} 

-- Font config {{{
Font = {
	Face	= "fixed",										-- Font face for the text
	Style	= "medium",										-- Font style (medium|bold|italic)
	Size	= 12													-- Font size
} --- }}}

-- Color config {{{
Colors = {
	Font				= "#ffffff",						-- Color of the font
	Border			= "#ffffff",						-- Color of the border/tiles
	Normal			= "#9AA38A",						-- Color of the inactive windows
	Focus				= "#A7E737",						-- Color of the focus window
	Shade				= "#A0B67A",						-- Color of shaded windows
	Background	= "#9A9C95"							-- Color of root background
} -- }}}

-- Key config {{{
-- Modifier keys:
-- A = Alt key
-- S = Shift key
-- C = Control key
-- W = Super (Windows key)
-- M = Meta key
Keys = {
	ViewJump1							= "A-1", 							-- Jump to view 1
	ViewJump2							= "A-2",  						-- Jump to view 2
	ViewJump3							= "A-3", 							-- Jump to view 3
	ViewJump4							= "A-4", 							-- Jump to view 4
	ViewJump5							= "A-5",							-- Jump to view 5
	ViewMnemonic					= "A-j",							-- Jump to view
	["tagmenu.sh -c -t"]	= "A-S-c",						-- Exec tag menu (client/tag)
	["tagmenu.sh -c -u"]	= "A-C-c",						-- Exec tag menu (client/untag)
	["tagmenu.sh -v -t"]	= "A-S-v",						-- Exec tag menu (view/tag)
	["tagmenu.sh -v -u"]	= "A-C-v",						-- Exec tag menu (view/untag)
	["xterm +sb"]					= "S-F1"							-- Exec a term
} -- }}}

-- Tags {{{
Tags = {
	terms		= "[ur]+xvt|xterm",
	browser	= "firefox|opera",
	editor	= "gvim|scite"
} -- }}}

-- Views {{{
Views = {
	work	= "browser|terms",
	dev 	= "editor|terms",
	web		= "browser",
} -- }}}
