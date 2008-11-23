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
  "family" => "Bitstream Vera Sans Mono",   # Font family for the text
  "style"  => "medium",                     # Font style (medium|bold|italic)
  "size"   => 7                             # Font size
}

#
# Colors
# 
COLORS = { 
  "font"       => "#ffffff",   # Color of the font
  "border"     => "#ffffff",   # Color of the border
  "normal"     => "#9AA38A",   # Color of the inactive windows
  "focus"      => "#A7E737",   # Color of the focus window
  "background" => "#9A9C95"    # Color of root background
}

#
# Grabs
#
# Modifier keys:
# A  = Alt key       S = Shift key
# C  = Control key   W = Super (Windows key)
# M  = Meta key

# Mouse buttons:
# B1 = Button1       B2 = Button2
# B3 = Button3       B4 = Button4
# B5 = Button5
#
GRABS = {
  "ViewJump1"       => "A-1",       # Jump to view 1
  "ViewJump2"       => "A-2",       # Jump to view 2
  "ViewJump3"       => "A-3",       # Jump to view 3
  "ViewJump4"       => "A-4",       # Jump to view 4
  "ViewJump5"       => "A-5",       # Jump to view 5
  "WindowMove"      => "S-B1",      # Move window
  "WindowResize"    => "S-B3",      # Resize window
  "WindowFloat"     => "A-f",       # Toggle float
  "WindowFull"      => "A-space",   # Toggle full
  "WindowKill"      => "A-k",       # Kill window
  "subtag.sh -c -t" => "A-S-c",     # Exec tag menu (client/tag)
  "subtag.sh -c -u" => "A-C-c",     # Exec tag menu (client/untag)
  "subtag.sh -v -t" => "A-S-v",     # Exec tag menu (view/tag)
  "subtag.sh -v -u" => "A-C-v",     # Exec tag menu (view/untag)
  "xterm +sb"       => "S-F1"       # Exec a term
}

#
# Tags
#
TAGS = {
  "terms"   => "urxvt|xterm",
  "browser" => "gran paradiso",
  "editor"  => "gvim",
  "video"   => "mplayer",
  "float"   => "xnest|urxvt",
  "full"    => "gvim",
  "urgent"  => "xlogo"
}  

#
# Views
#
VIEWS = {
  "work" => "browser|terms",
  "dev"  => "editor|terms",
  "web"  => "browser|video"
}
