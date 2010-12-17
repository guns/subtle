#!/usr/bin/ruby
#
# @package subtler
#
# @file Subtle remote
# @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require "getoptlong"
require "subtle/subtlext"

module Subtle # {{{
  module Subtler # {{{
    class Runner # {{{

    # Signals
    trap "INT" do
      exit
    end

      ## initialize {{{
      # Intialize class
      ##

      def initialize
        @mod    = nil
        @group  = nil
        @action = nil

        # Getopt options
        @opts = GetoptLong.new(
          # Groups
          [ "--Client",  "-c", GetoptLong::NO_ARGUMENT ],
          [ "--Gravity", "-g", GetoptLong::NO_ARGUMENT ],
          [ "--Screen",  "-e", GetoptLong::NO_ARGUMENT ],
          [ "--Sublet",  "-s", GetoptLong::NO_ARGUMENT ],
          [ "--Tag",     "-t", GetoptLong::NO_ARGUMENT ],
          [ "--Tray",    "-y", GetoptLong::NO_ARGUMENT ],
          [ "--View",    "-v", GetoptLong::NO_ARGUMENT ],

          # Actions
          [ "--add",     "-a", GetoptLong::NO_ARGUMENT ],
          [ "--kill",    "-k", GetoptLong::NO_ARGUMENT ],
          [ "--find",    "-f", GetoptLong::NO_ARGUMENT ],
          [ "--focus",   "-o", GetoptLong::NO_ARGUMENT ],
          [ "--full",    "-F", GetoptLong::NO_ARGUMENT ],
          [ "--float",   "-O", GetoptLong::NO_ARGUMENT ],
          [ "--stick",   "-S", GetoptLong::NO_ARGUMENT ],
          [ "--jump",    "-j", GetoptLong::NO_ARGUMENT ],
          [ "--list",    "-l", GetoptLong::NO_ARGUMENT ],
          [ "--tag",     "-T", GetoptLong::NO_ARGUMENT ],
          [ "--untag",   "-U", GetoptLong::NO_ARGUMENT ],
          [ "--tags",    "-G", GetoptLong::NO_ARGUMENT ],
          [ "--retag",   "-A", GetoptLong::NO_ARGUMENT ],
          [ "--clients", "-I", GetoptLong::NO_ARGUMENT ],
          [ "--views",   "-W", GetoptLong::NO_ARGUMENT ],
          [ "--update",  "-u", GetoptLong::NO_ARGUMENT ],
          [ "--data",    "-D", GetoptLong::NO_ARGUMENT ],
          [ "--gravity", "-Y", GetoptLong::NO_ARGUMENT ],
          [ "--screen",  "-n", GetoptLong::NO_ARGUMENT ],
          [ "--raise",   "-E", GetoptLong::NO_ARGUMENT ],
          [ "--lower",   "-L", GetoptLong::NO_ARGUMENT ],

          # Modifiers
          [ "--reload",  "-r", GetoptLong::NO_ARGUMENT ],
          [ "--restart", "-R", GetoptLong::NO_ARGUMENT ],
          [ "--quit",    "-q", GetoptLong::NO_ARGUMENT ],
          [ "--current", "-C", GetoptLong::NO_ARGUMENT ],
          [ "--select",  "-X", GetoptLong::NO_ARGUMENT ],

          # Other
          [ "--help",    "-h", GetoptLong::NO_ARGUMENT ],
          [ "--version", "-V", GetoptLong::NO_ARGUMENT ]
        )
      end # }}}

      ## run {{{
      # Run subtler
      ##

      def run
        # Parse arguments
        @opts.each do |opt, arg|
          case opt
            # Groups
            when "--Client"  then @group  = Subtlext::Client
            when "--Gravity" then @group  = Subtlext::Gravity
            when "--Screen"  then @group  = Subtlext::Screen
            when "--Sublet"  then @group  = Subtlext::Sublet
            when "--Tag"     then @group  = Subtlext::Tag
            when "--Tray"    then @group  = Subtlext::Tray
            when "--View"    then @group  = Subtlext::View

            # Actions
            when "--add"     then @action = :new
            when "--kill"    then @action = :kill
            when "--find"    then @action = :find
            when "--focus"   then @action = :focus
            when "--full"    then @action = :toggle_full
            when "--float"   then @action = :toggle_float
            when "--stick"   then @action = :toggle_stick
            when "--jump"    then @action = :jump
            when "--list"    then @action = :all
            when "--tag"     then @action = :tag
            when "--untag"   then @action = :untag
            when "--tags"    then @action = :tags
            when "--retag"   then @action = :retag
            when "--clients" then @action = :clients
            when "--views"   then @action = :views
            when "--update"  then @action = :update
            when "--data"    then @action = :data=
            when "--gravity" then @action = :gravity=
            when "--raise"   then @action = :raise
            when "--lower"   then @action = :lower

            # Modifiers
            when "--reload"  then @mod = :reload
            when "--restart" then @mod = :restart
            when "--quit"    then @mod = :quit
            when "--current" then @mod = :current
            when "--select"  then @mod = :select

            # Other
            when "--help"
              usage(@group)
              exit
            when "--version"
              version
              exit
            else
              usage
              exit
          end
        end

        # Get arguments
        arg1 = ARGV.shift
        arg2 = ARGV.shift

        # Pipes?
        arg1 = ARGF.read.chop if("-" == arg1)
        arg2 = ARGF.read.chop if("-" == arg2)

        # Modifiers
        case @mod
          when :reload  then  Subtlext::Subtle.reload
          when :restart then  Subtlext::Subtle.restart
          when :quit    then  Subtlext::Subtle.quit
          when :current
            arg2 = arg1
            arg1 = :current
          when :select
            arg2 = arg1
            arg1 = Subtlext::Subtle.select_window
        end

        # Call method
        if(!@group.nil? and !@action.nil?)
          # Check singleton and instance methods
          if((@group.singleton_methods << :new).include?(@action))
            obj   = @group
            arg   = arg1
            arity = obj.method(@action).arity
          elsif(@group.instance_methods.include?(@action))
            obj   = @group.send(:find, arg1)
            arg   = arg2
            arity = @group.instance_method(@action).arity
          end

          # Check arity
          case arity
            when 1
              ret = obj.send(@action, arg)
            when -1
              if([ Subtlext::View, Subtlext::Tag ].include?(@group))
                # Create new object
                ret = obj.send(@action, arg)
                ret.save
              end

              exit
            when nil
              usage(@group)
              exit
            else
              ret = obj.send(@action)
          end

          # Handle result
          case ret
            when Array
              ret.each do |r|
                printer(r)
              end
            else
              printer(ret)
          end
        elsif(:reload != @mod and :restart != @mod and :quit != @mod)
          usage(@group)
          exit
        end
      end # }}}

      private

      # Client modes
      MODE_FULL  = (1 << 1)
      MODE_FLOAT = (1 << 2)
      MODE_STICK = (1 << 3)

      def printer(value) # {{{
        case value
          when Subtlext::Client # {{{
            puts "%#10x %s %d %4d x %4d + %4d + %-4d %12.12s %s%s%s %s (%s)" % [
              value.win,
              value.views.map { |v| v.name }.include?(Subtlext::View.current.name) ? "*" : "-",
              Subtlext::View.current.id,
              value.geometry.x,
              value.geometry.y,
              value.geometry.width,
              value.geometry.height,
              value.gravity.name,
              (value.flags & MODE_FULL).zero?  ? "-" : "F",
              (value.flags & MODE_FLOAT).zero? ? "-" : "O",
              (value.flags & MODE_STICK).zero? ? "-" : "S",
              value.instance,
              value.klass
            ] # }}}
          when Subtlext::Gravity # {{{
            puts "%15s %3d x %-3d %3d + %-3d" % [
              value.name,
              value.geometry.x,
              value.geometry.y,
              value.geometry.width,
              value.geometry.height,
            ] # }}}
          when Subtlext::Screen # {{{
            puts "%d %4d x %-4d %4d + %-4d" % [
              value.id,
              value.geometry.x,
              value.geometry.y,
              value.geometry.width,
              value.geometry.height,
            ] # }}}
          when Subtlext::Tag # {{{
            puts value.name # }}}
          when Subtlext::Tray # {{{
            puts "%#10lx %s (%s)" % [
              value.win,
              value.instance,
              value.klass
            ] # }}}
          when Subtlext::Sublet
            puts "%s" % [ value.name ]
          when Subtlext::View # {{{
            puts "%#10x %s %2d %s" % [
              value.win,
              value.current? ? "*" : "-",
              value.id,
              value.name
            ] # }}}
        end
      end # }}}

      def usage(group) # {{{
        puts "Usage: subtler [GENERIC|MODIFIER] [GROUP] [ACTION]\n\n"

        if(group.nil?)
          puts <<EOF
  Generic:
    -d, --display=DISPLAY   Connect to DISPLAY (default: #{ENV["DISPLAY"]})
    -D, --debug             Print debugging messages
    -h, --help              Show this help and exit
    -V, --version           Show version info and exit

  Modifier:
    -r, --reload            Reload config and sublets
    -R, --restart           Restart subtle
    -q, --quit              Quit %s
    -C, --current           Select current active window/view
    -X, --select            Select a window via pointer

  Groups:
    -c, --client            Use client group
    -g, --gravity           Use gravity group
    -e, --screen            Use screen group
    -s, --sublet            Use sublet group
    -t, --tag               Use tag group
    -y, --tray              Use tray group
    -v, --view              Use views group

EOF
        end

        if(group.nil? or Subtlext::Client == group)
          puts <<EOF
  Actions for clients (-c, --client):
    -f, --find=PATTERN      Find client
    -o, --focus=PATTERN     Set focus to client
    -F, --full              Toggle full
    -O, --float             Toggle float
    -S, --stick             Toggle stick
    -l, --list              List all clients
    -T, --tag=PATTERN       Add tag to client
    -U, --untag=PATTERN     Remove tag from client
    -G, --tags              Show client tags
    -Y, --gravity           Set client gravity
    -E, --raise             Raise client window
    -L, --lower             Lower client window
    -k, --kill=PATTERN      Kill client

EOF
        end

        if(group.nil? or Subtlext::Gravity == group)
          puts <<EOF
  Actions for gravities (-g, --gravity):
    -a, --add=NAME          Create new gravity
    -l, --list              List all gravities
    -f, --find=PATTERN      Find a gravity
    -k, --kill=PATTERN      Kill gravity mode

EOF
        end

        if(group.nil? or Subtlext::Screen == group)
          puts <<EOF
  Actions for screens (-e, --screen):
    -l, --list              List all screens
    -f, --find=ID           Find a screen

EOF
        end

        if(group.nil? or Subtlext::Sublet == group)
          puts <<EOF
  Actions for sublets (-s, --sublet):
    -a, --add=FILE          Create new sublet
    -l, --list              List all sublets
    -u, --update            Updates value of sublet
    -D, --data              Set data of sublet
    -k, --kill=PATTERN      Kill sublet

EOF
        end

        if(group.nil? or Subtlext::Tag == group)
          puts <<EOF
  Actions for tags (-t, --tag):
    -a, --add=NAME          Create new tag
    -f, --find              Find all clients/views by tag
    -l, --list              List all tags
    -I, --clients           Show clients with tag
    -k, --kill=PATTERN      Kill tag

EOF
        end

        if(group.nil? or Subtlext::Tray == group)
          puts <<EOF

  Actions for tray (-y, --tray):
    -f, --find              Find all tray icons
    -l, --list              List all tray icons
    -k, --kill=PATTERN      Kill tray icon

EOF
        end

        if(group.nil? or Subtlext::View == group)
          puts <<EOF
  Actions for views (-v, --view):
    -a, --add=NAME          Create new view
    -f, --find=PATTERN      Find a view
    -l, --list              List all views
    -T, --tag=PATTERN       Add tag to view
    -U, --untag=PATTERN     Remove tag from view
    -G, --tags              Show view tags
    -I, --clients           Show clients on view
    -k, --kill=VIEW         Kill view

EOF
        end

        puts <<EOF
  Formats:
    DISPLAY:  :<display number>
    ID:       <number>
    GEOMETRY: <x>x<y>+<width>+<height>
    PATTERN:
      Matching works either via plaintext, regex (see regex(7)), id or window id
      if applicable. If a pattern matches more than once ONLY the first match will
      be used.
      If the PATTERN is '-' subtler will read from stdin.

  Listings:
    Client listing:  <window id> [-*] <view id> <geometry> <gravity> <flags> <name> (<class>)
    Gravity listing: <gravity id> <geometry>
    Screen listing:  <screen id> <geometry>
    Tag listing:     <name>
    View listing:    <window id> [-*] <view id> <name>

  Examples:
    subtler -c -l                List all clients
    subtler -t -a subtle         Add new tag 'subtle'
    subtler -v subtle -T rocks   Tag view 'subtle' with tag 'rocks'
    subtler -c xterm -G          Show tags of client 'xterm'
    subtler -c -x -f             Select client and show info
    subtler -c -C -y 5           Set gravity 5 to current active client
    subtler -t -f term           Show every client/view tagged with 'term'

  Please report bugs at http://subforge.org/projects/subtle/issues

EOF
      end # }}}

      def version # {{{
        puts <<EOF
  subtler #{Subtlext::VERSION} - Copyright (c) 2005-2010 Christoph Kappel
  Released under the GNU General Public License
  Using Ruby #{RUBY_VERSION}
EOF
      end # }}}
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
