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
  "gravity" => 5,               # Default gravity (0 = gravity of active window)
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
  "fg_bar"     => "#ffffff",   # Foreground color of the bar
  "bg_bar"     => "#5d5d5d",   # Background color of the bar
  "fg_focus"   => "#ffffff",   # Foreground color of focus windows
  "bg_focus"   => "#ff00a8",   # Background color of focus windows
  "normal"     => "#5d5d5d",   # Color of windows without focus
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
  # View Jumps
  "W-1"      => "ViewJump1",                   # Jump to view 1
  "W-2"      => "ViewJump2",                   # Jump to view 2
  "W-3"      => "ViewJump3",                   # Jump to view 3 
  "W-4"      => "ViewJump4",                   # Jump to view 4

  # Subtle grabs
  "W-C-r"    => "SubtleReload",                # Reload config
  "W-C-q"    => "SubtleQuit",                  # Quit subtle

  # Window grabs
  "W-B1"     => "WindowMove",                  # Move window
  "W-B3"     => "WindowResize",                # Resize window
  "W-f"      => "WindowFloat",                 # Toggle float
  "W-space"  => "WindowFull",                  # Toggle full
  "W-s"      => "WindowStick",                 # Toggle stick
  "W-r"      => "WindowRaise",                 # Raise window
  "W-l"      => "WindowLower",                 # Lower window
  "W-Left"   => "WindowLeft",                  # Select window left
  "W-Down"   => "WindowDown",                  # Select window below
  "W-Up"     => "WindowUp",                    # Select window above
  "W-Right"  => "WindowRight",                 # Select window right
  "W-S-k"    => "WindowKill",                  # Kill window

  # Vertical gravities grabs
  "W-KP_7"   => "GravityTopLeftVert",          # Set top left gravity
  "W-KP_8"   => "GravityTopVert",              # Set top gravity
  "W-KP_9"   => "GravityTopRightVert",         # Set top right gravity
  "W-KP_4"   => "GravityLeftVert",             # Set left gravity
  "W-KP_5"   => "GravityCenterVert",           # Set center gravity
  "W-KP_6"   => "GravityRightVert",            # Set right gravity
  "W-KP_1"   => "GravityBottomLeftVert",       # Set bottom left gravity
  "W-KP_2"   => "GravityBottomVert",           # Set bottom gravity
  "W-KP_3"   => "GravityBottomRightVert",      # Set bottom right gravity

  # Horizontal gravities grabs
  "W-S-KP_7" => "GravityTopLeftHorz",          # Set top left gravity
  "W-S-KP_8" => "GravityTopHorz",              # Set top gravity
  "W-S-KP_9" => "GravityTopRightHorz",         # Set top right gravity
  "W-S-KP_4" => "GravityLeftHorz",             # Set left gravity
  "W-S-KP_5" => "GravityCenterHorz",           # Set center gravity
  "W-S-KP_6" => "GravityRightHorz",            # Set right gravity
  "W-S-KP_1" => "GravityBottomLeftHorz",       # Set bottom left gravity
  "W-S-KP_2" => "GravityBottomHorz",           # Set bottom gravity
  "W-S-KP_3" => "GravityBottomRightHorz",      # Set bottom right gravity

  # Exec
  "W-Return" => "xterm",                       # Exec a term

  # Ruby lambdas
  "S-F2"     => lambda { |c| puts c.name  },   # Print client name
  "S-F3"     => lambda { puts version },       # Print subtlext version
  "S-F4"     => lambda { |c|                   # Show client on view test
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
  "browser" => "opera|firefox",
  "editor"  => "[g]?vim",
  "stick"   => "mplayer|imagemagick",
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

#
# Hooks
#
HOOKS = { }

# vim:ts=2:bs=2:sw=2:et:fdm=marker
