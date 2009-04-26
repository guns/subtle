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
  "W-1"     => Subtle::Grab::Jump1,                 # Jump to view 1
  "W-2"     => Subtle::Grab::Jump2,                 # Jump to view 2
  "W-3"     => Subtle::Grab::Jump3,                 # Jump to view 3 
  "W-4"     => Subtle::Grab::Jump4,                 # Jump to view 4
  "S-B1"    => Subtle::Grab::WindowMove,            # Move window
  "S-B3"    => Subtle::Grab::WindowResize,          # Resize window
  "A-f"     => Subtle::Grab::WindowFloat,           # Toggle float
  "A-space" => Subtle::Grab::WindowFull,            # Toggle full
  "A-s"     => Subtle::Grab::WindowStick,           # Toggle stic
  "A-k"     => Subtle::Grab::WindowKill,            # Kill window
  "S-KP_7"  => Subtle::Grab::GravityTopLeft,        # Set top left gravity
  "S-KP_8"  => Subtle::Grab::GravityTop,            # Set top gravity
  "S-KP_9"  => Subtle::Grab::GravityTopRight,       # Set top right gravity
  "S-KP_4"  => Subtle::Grab::GravityLeft,           # Set left gravity
  "S-KP_5"  => Subtle::Grab::GravityCenter,         # Set center gravity
  "S-KP_6"  => Subtle::Grab::GravityRight,          # Set right gravity
  "S-KP_1"  => Subtle::Grab::GravityBottomLeft,     # Set bottom left gravity
  "S-KP_2"  => Subtle::Grab::GravityBottom,         # Set bottom gravity
  "S-KP_3"  => Subtle::Grab::GravityBottomRight,    # Set bottom right gravity

  "S-F1"    => "xterm +sb",                         # Exec a term

  "S-F2"    => lambda { |c| puts c.name  },         # Print client name
  "S-F3"    => lambda { |c| puts version },         # Print subtle version
  "S-F4"    => lambda { |c|                         # Show client on view test
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
