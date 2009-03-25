# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

#
# Options
#
OPTIONS = {
  "border" => 2,   # Border size of the windows
  "step"   => 5    # Window move/resize key step
}

#
# Font
#
FONT = { 
  "family" => "lucidatypewriter",   # Font family for the text
  "style"  => "medium",             # Font style (medium|bold|italic)
  "size"   => 11                    # Font size
}

#
# Colors
# 
COLORS = { 
  "font"       => "#ffffff",   # Color of font
  "border"     => "#e1e1e1",   # Color of border
  "normal"     => "#5d5d5d",   # Color of inactive windows
  "focus"      => "#ff00a8",   # Color of focus window
  "background" => "#3d3d3d"    # Color of root background
}

#
# Grabs
#
# Modifier keys:
# A  = Alt key       S = Shift key
# C  = Control key   W = Super (Windows key)
# M  = Meta key
#
# Mouse buttons:
# B1 = Button1       B2 = Button2
# B3 = Button3       B4 = Button4
# B5 = Button5
#
GRABS = {
  "ViewJump1"          => "A-1",       # Jump to view 1
  "ViewJump2"          => "A-2",       # Jump to view 2
  "ViewJump3"          => "A-3",       # Jump to view 3
  "ViewJump4"          => "A-4",       # Jump to view 4
  "ViewJump5"          => "A-5",       # Jump to view 5
  "WindowMove"         => "S-B1",      # Move window
  "WindowResize"       => "S-B3",      # Resize window
  "WindowFloat"        => "A-f",       # Toggle float
  "WindowFull"         => "A-space",   # Toggle full,
  "WindowStick"        => "A-s",       # Toggle stick
  "WindowKill"         => "A-k",       # Kill window
  "GravityTopLeft"     => "S-KP_7",    # Set left gravity
  "GravityTop"         => "S-KP_8",    # Set top gravity
  "GravityTopRight"    => "S-KP_9",    # Set right gravity
  "GravityLeft"        => "S-KP_4",    # Set left gravity
  "GravityCenter"      => "S-KP_5",    # Set center gravity
  "GravityRight"       => "S-KP_6",    # Set right gravity
  "GravityBottomLeft"  => "S-KP_1",    # Set bottom-left gravity
  "GravityBottom"      => "S-KP_2",    # Set bottom gravity
  "GravityBottomRight" => "S-KP_3",    # Set bottom-right gravity
  "xterm +sb"          => "A-Return"   # Exec a term
}

#
# Tags
#
TAGS = {
  "terms"   => "xterm|urxvt",
  "browser" => "opera",
  "editor"  => "gvim",
  "video"   => "mplayer",
  "float"   => "mplayer",
  "full"    => "gvim",
  "stick"   => "xlogo",
  "left"    => "xterm",
  "right"   => "urxvt"
}  

#
# Views
#
VIEWS = {
  "work" => "terms",
  "dev"  => "editor",
  "web"  => "browser|video|terms"
}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
