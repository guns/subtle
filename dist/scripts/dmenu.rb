#
# dmenu based tagging script
# Copyright (c) 2005-2010 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

require "subtle/subtlext"
require "getoptlong"

# Load subtle's config for colors and font
xdgconfig = ENV["XDG_CONFIG_HOME"] || File.join(ENV["HOME"], ".config")

require File.join(xdgconfig, "subtle", "subtle.rb")

# Variables
action = :tag
group  = :client
klass  = Subtlext::Client
dmenu  = "dmenu -fn '%s' -nb '%s' -nf '%s' -sb '%s' -sf '%s'" % [
  OPTIONS[:font], COLORS[:bg_panel], COLORS[:fg_panel], 
  COLORS[:fg_focus], COLORS[:fg_panel]
]

# Getopt options
opts = GetoptLong.new(
  [ "--clients", "-c", GetoptLong::NO_ARGUMENT ],
  [ "--views",   "-v", GetoptLong::NO_ARGUMENT ],
  [ "--tag",     "-t", GetoptLong::NO_ARGUMENT ],
  [ "--untag",   "-u", GetoptLong::NO_ARGUMENT ],
  [ "--help",    "-h", GetoptLong::NO_ARGUMENT ]
)

# Parse options
opts.each do |opt, arg|
  case opt
    when "--clients"
      group = :client
      klass = Subtlext::Client
    when "--views"
      group = :view
      klass = Subtlext::View
    when "--tag"   then action = :tag
    when "--untag" then action = :untag
    else
      puts <<EOF
Usage: dmenu.rb [OPTIONS]

  -c, --clients  Select clients
  -v, --views    Select views
  -t, --tag      Set tags
  -u, --untag    Unset tags
  -h, --help     Show this help and exit
EOF
      exit
  end
end

# Select target
list   = klass[:all].map { |o| o.instance rescue o.name }.join("\n")
target = `echo '#{list}' | #{dmenu} -p 'Select #{group.to_s}: '`

exit unless(0 == $?) #< Check exit code of dmenu

# Select tag
tags = Subtlext::Tag[:all].map { |t| t.name }.join("\n")
tag  = `echo '#{tags}' | #{dmenu} -p 'Select tag: '`

exit unless(0 == $?) #< Check exit code of dmenu

# Send action
klass[target].send(action, tag)
