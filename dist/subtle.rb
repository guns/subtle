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
  "border"  => 2,               # Border size of the windows
  "step"    => 5,               # Window move/resize key step
  "bar"     => 0,               # Bar position (0 = top, 1 = bottom)
  "padding" => [ 0, 0, 0, 0 ]   # Screen padding (left, right, top, bottom)
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
  "W-1"      => "ViewJump1",                  # Jump to view 1
  "W-2"      => "ViewJump2",                  # Jump to view 2
  "W-3"      => "ViewJump3",                  # Jump to view 3 
  "W-4"      => "ViewJump4",                  # Jump to view 4
  "W-B1"     => "WindowMove",                 # Move window
  "W-B3"     => "WindowResize",               # Resize window
  "W-f"      => "WindowFloat",                # Toggle float
  "W-space"  => "WindowFull",                 # Toggle full
  "W-s"      => "WindowStick",                # Toggle stick
  "W-S-r"    => "WindowRaise",                # Raise window
  "W-S-l"    => "WindowLower",                # Lower window
  "W-h"      => "WindowLeft",                 # Select window left
  "W-j"      => "WindowDown",                 # Select window below
  "W-k"      => "WindowUp",                   # Select window above
  "W-l"      => "WindowRight",                # Select window right
  "W-S-k"    => "WindowKill",                 # Kill window
  "S-KP_7"   => "GravityTopLeft",             # Set top left gravity
  "S-KP_8"   => "GravityTop",                 # Set top gravity
  "S-KP_9"   => "GravityTopRight",            # Set top right gravity
  "S-KP_4"   => "GravityLeft",                # Set left gravity
  "S-KP_5"   => "GravityCenter",              # Set center gravity
  "S-KP_6"   => "GravityRight",               # Set right gravity
  "S-KP_1"   => "GravityBottomLeft",          # Set bottom left gravity
  "S-KP_2"   => "GravityBottom",              # Set bottom gravity
  "S-KP_3"   => "GravityBottomRight",         # Set bottom right gravity

  "A-Return" => "xterm",                      # Exec a term

  "S-F2"     => lambda { |c| puts c.name  },  # Print client name
  "S-F3"     => lambda { puts version },      # Print subtlext version
  "S-F4"     => lambda { |c|                  # Show client on view test
    v = add_view("test")
    t = add_tag("test")

    c + t
    v + t

    v.jump
  }  
}

#
# Tags
#
TAGS = {
  "terms"   => "xterm|[u]?rxvt",
  "browser" => "opera",
  "editor"  => "[g]?vim",
  "stick"   => "mplayer",
  "float"   => "mplayer|imagemagick|gimp"
}  

#
# Views
#
VIEWS = {
  "work" => "terms",
  "dev"  => "editor|terms",
  "web"  => "browser"
}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
