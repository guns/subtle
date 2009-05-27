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
  "W-1"      => "ViewJump1",                         # Jump to view 1
  "W-2"      => "ViewJump2",                         # Jump to view 2
  "W-3"      => "ViewJump3",                         # Jump to view 3 
  "W-4"      => "ViewJump4",                         # Jump to view 4
  "W-B1"     => "WindowMove",                        # Move window
  "W-B3"     => "WindowResize",                      # Resize window
  "W-f"      => "WindowFloat",                       # Toggle float
  "W-space"  => "WindowFull",                        # Toggle full
  "W-s"      => "WindowStick",                       # Toggle stick
  "W-S-r"    => "WindowRaise",                       # Raise window
  "W-S-l"    => "WindowLower",                       # Lower window
  "W-Left"   => "WindowLeft",                        # Select window left
  "W-Down"   => "WindowDown",                        # Select window below
  "W-Up"      => "WindowUp",                         # Select window above
  "W-Right"  => "WindowRight",                       # Select window right
  "W-S-k"    => "WindowKill",                        # Kill window
  "S-KP_7"   => "GravityTopLeft",                    # Set top left gravity
  "S-KP_8"   => "GravityTop",                        # Set top gravity
  "S-KP_9"   => "GravityTopRight",                   # Set top right gravity
  "S-KP_4"   => "GravityLeft",                       # Set left gravity
  "S-KP_5"   => "GravityCenter",                     # Set center gravity
  "S-KP_6"   => "GravityRight",                      # Set right gravity
  "S-KP_1"   => "GravityBottomLeft",                 # Set bottom left gravity
  "S-KP_2"   => "GravityBottom",                     # Set bottom gravity
  "S-KP_3"   => "GravityBottomRight",                # Set bottom right gravity

  "A-Return" => "xterm",                             # Exec a term

  "C-KP_7"   => lambda { |c| c.gravity = horz(7) },  # Set top left gravity (horizontally)
  "C-KP_8"   => lambda { |c| c.gravity = horz(8) },  # Set top gravity (horizontally)
  "C-KP_9"   => lambda { |c| c.gravity = horz(9) },  # Set top right gravity (horizontally)
  "C-KP_4"   => lambda { |c| c.gravity = horz(4) },  # Set left gravity (horizontally)
  "C-KP_5"   => lambda { |c| c.gravity = horz(5) },  # Set center gravity (horizontally)
  "C-KP_6"   => lambda { |c| c.gravity = horz(6) },  # Set right gravity (horizontally)
  "C-KP_1"   => lambda { |c| c.gravity = horz(1) },  # Set bottom left gravity (horizontally)
  "C-KP_2"   => lambda { |c| c.gravity = horz(2) },  # Set bottom gravity (horizontally)
  "C-KP_3"   => lambda { |c| c.gravity = horz(3) },  # Set bottom right gravity (horizontally)

  "S-F2"     => lambda { |c| puts c.name  },         # Print client name
  "S-F3"     => lambda { puts version },             # Print subtlext version
  "S-F4"     => lambda { |c|                         # Show client on view test
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
