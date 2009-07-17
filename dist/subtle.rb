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
  :border  => 2,               # Border size of the windows
  :step    => 5,               # Window move/resize key step
  :gravity => 5,               # Default gravity (0 = copy gravity of focus window)
  :swap    => false,           # Bar at bottom?
  :panel   => false,           # Show sublets in an own panel?
  :stipple => true,            # Draw stipple on bar  
  :padding => [ 0, 0, 0, 0 ]   # Screen padding (left, right, top, bottom)
}

#
# Font
#
FONT = { 
  :family => "lucidatypewriter",   # Font family for the text
  :style  => "medium",             # Font style (medium|bold|italic)
  :size   => 11                    # Font size
}

#
# Colors
# 
COLORS = { 
  :fg_bar        => "#e2e2e5",  # Foreground color of bar
  :fg_views      => "#CF6171",  # Foreground color of view button
  :fg_focus      => "#000000",  # Foreground color of focus window title and view

  :bg_bar        => "#444444",  # Background color of bar
  :bg_views      => "#3d3d3d",  # Background color of view button
  :bg_focus      => "#CF6171",  # Background color of focus window title and view

  :border_focus  => "#CF6171",  # Border color of focus windows
  :border_normal => "#5d5d5d",  # Border color of normal windows
  
  :background    => "#3d3d3d"   # Background color of root background
}

#
# Dmenu settings
#
@dmenu = "dmenu_run -fn '%s-%d' -nb '%s' -nf '%s' -sb '%s' -sf '%s' -p 'Select:'" % [
  FONT[:family], FONT[:size],
  COLORS[:bg_bar], COLORS[:fg_bar], 
  COLORS[:bg_focus], COLORS[:fg_focus]
]

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
  "W-1"      => :ViewJump1,                   # Jump to view 1
  "W-2"      => :ViewJump2,                   # Jump to view 2
  "W-3"      => :ViewJump3,                   # Jump to view 3 
  "W-4"      => :ViewJump4,                   # Jump to view 4

  # View Jumps
  "W-A-1"    => :ScreenJump1,                 # Jump to screen 1
  "W-A-2"    => :ScreenJump2,                 # Jump to screen 2
  "W-A-3"    => :ScreenJump3,                 # Jump to screen 3 
  "W-A-4"    => :ScreenJump4,                 # Jump to screen 4

  # Subtle grabs
  "W-C-r"    => :SubtleReload,                # Reload config
  "W-C-q"    => :SubtleQuit,                  # Quit subtle

  # Window grabs
  "W-B1"     => :WindowMove,                  # Move window
  "W-B3"     => :WindowResize,                # Resize window
  "W-f"      => :WindowFloat,                 # Toggle float
  "W-space"  => :WindowFull,                  # Toggle full
  "W-s"      => :WindowStick,                 # Toggle stick
  "W-r"      => :WindowRaise,                 # Raise window
  "W-l"      => :WindowLower,                 # Lower window
  "W-Left"   => :WindowLeft,                  # Select window left
  "W-Down"   => :WindowDown,                  # Select window below
  "W-Up"     => :WindowUp,                    # Select window above
  "W-Right"  => :WindowRight,                 # Select window right
  "W-S-k"    => :WindowKill,                  # Kill window

  # Gravities grabs
  "W-KP_7"   => :GravityTopLeft,              # Set top left gravity
  "W-KP_8"   => :GravityTop,                  # Set top gravity
  "W-KP_9"   => :GravityTopRight,             # Set top right gravity
  "W-KP_4"   => :GravityLeft,                 # Set left gravity
  "W-KP_5"   => :GravityCenter,               # Set center gravity
  "W-KP_6"   => :GravityRight,                # Set right gravity
  "W-KP_1"   => :GravityBottomLeft,           # Set bottom left gravity
  "W-KP_2"   => :GravityBottom,               # Set bottom gravity
  "W-KP_3"   => :GravityBottomRight,          # Set bottom right gravity

  # Exec
  "W-Return" => "xterm",                      # Exec a term
  "W-x"      => @dmenu,                       # Exec dmenu

  # Ruby lambdas
  "S-F2"     => lambda { |c| puts c.name  },  # Print client name
  "S-F3"     => lambda { puts version },      # Print subtlext version
}

#
# Tags
#
TAGS = {
  "terms"   => "xterm|[u]?rxvt",
  "browser" => "uzbl|opera|firefox",
  "editor"  => "[g]?vim",
  "stick"   => { :regex => "mplayer|imagemagick", :float => true, :stick => true },
  "float"   => { :regex => "gimp", :float => true }
}  

#
# Views
#
VIEWS = [
  { "work" => "terms" },
  { "dev"  => "editor|terms" },
  { "web"  => "browser" }
]

#
# Hooks
#
HOOKS = { }

# vim:ts=2:bs=2:sw=2:et:fdm=marker
