# coding: utf-8
#
# @package subtle
#
# @file Migrate config
# @copyright Copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

HOOKS        ||= {}
@config       = ""
@config_orig  = ""
@positions    = []

 ## on {{{
 # Collect hook DSL blocks
 # @param event  [Symbol]  Hook event
 # @param blk    [Proc]    Hook block
 ##

def on(event, &blk)
  # Just store the hooks for later
  HOOKS[event] = blk
end # }}}

  ## print_options {{{
  # Print options
  ##

  def print_options
    OPTIONS.each do |k, v|
      puts "set %-12s %s\n" % [ k.inspect + ",", v.inspect ]
    end
  end # }}}

  ## print_panel {{{
  # Print panel
  ##

  def print_panel
    PANEL.each do |k, v|
      # Rename some symbols
      k = case k
        when :border then :outline
        else k
      end

      puts "set %-12s %s\n" % [ k.inspect + ",", v.inspect ]
    end
  end # }}}

  ## print_colors {{{
  # Print colors
  ##

  def print_colors
    COLORS.each do |k, v|
      puts "color %-15s %s\n" % [ k.inspect + ",", v.inspect ]
    end
  end # }}}

  ## print_gravities {{{
  # Print gravities
  ##

  def print_gravities
    GRAVITIES.each do |k, v|
      puts "gravity %-15s %s\n" % [ k.inspect + ",", v.inspect ]
    end
  end # }}}

  ## print_grabs {{{
  # Print grabs
  ##

  def print_grabs
    GRABS.each do |k, v|
      if(v.is_a?(Symbol) or v.is_a?(Array) or v.is_a?(String))
        puts 'grab %s %s' % [ k.inspect + ",", v.inspect ]
      elsif(v.is_a?(Proc))
        # Extract block from config
        block = extract(v.source_location[1])

        puts "\ngrab \"%s\" do %s" % [ k, block[0] ]
        puts "  # Extracted from line #%d" % [ v.source_location[1] ]
        puts block[1]
        puts "end\n\n"
      end
    end
  end # }}}

  ## print_tags {{{
  # Print tags
  ##

  def print_tags
    TAGS.each do |k, v|
      if(v.is_a?(String))
        puts 'tag "%s", %s' % [ k, v.inspect ]
      elsif(v.is_a?(Hash))
        puts "tag \"%s\" do" % [ k ]

        # Print hash values
        v.each do |vk, vv|
          puts "  %-8s %s" % [ vk, vv.inspect ]
        end

        puts "end\n\n"
      end
    end
  end # }}}

  ## print_views {{{
  # Print views
  ##

  def print_views
    VIEWS.each do |k, v|
      if(v.is_a?(String))
        puts 'view %s, %s' % [ k.inspect, v.inspect ]
      elsif(v.is_a?(Hash))
        puts "view \"%s\" do" % [ k ]

        # Print hash values
        v.each do |vk, vv|
          puts "  %-8s %s" % [ vk, vv.inspect ]
        end

        puts "end\n\n"
      end
    end
  end # }}}

  ## print_hooks {{{
  # Print hooks
  ##

  def print_hooks
    HOOKS.each do |k, v|
      if(v.is_a?(Proc))
        # Extract block from config
        block = extract(v.source_location[1])

        puts 'on %s do %s' % [ k.inspect, block[0] ]
        puts "  # Extracted from line #%d" % [ v.source_location[1] ]
        puts block[1]
        puts "end\n\n"
      end
    end
  end # }}}

  ## extract {{{
  # Extract code of procs
  # @param line  Fixnum  Line number
  # @return [Array] Iterator and code of block
  ##

def extract(line)
  iter  = ""
  block = ""

  begin
    # Get indentation level
    spaces = @config_orig[line - 1].match(/^([ \t]*)/)[1].length

    # Proc on a single line
    unless((match = @config_orig[line - 1].match(/.*(lambda|proc).*(\{|do)(.*)(end|\})[,]?$/)).nil?)
      return [ iter, match[3] ]
    end

    # Iterators
    unless((match = @config_orig[line - 1].match(/(\|.*\|)$/)).nil?)
      iter = match[1]
    end

    # Append lines
    (line..@config_orig.size).each do |l|
      if(@config_orig[l].match(/^[ \t]{#{spaces}}(\}|end)[,]?$/))
        break
      end

      block << @config_orig[l]
    end
  rescue => err
    puts err
    puts err.backtrace
  end

  [ iter, block ]
end # }}}

  ## analyze {{{
  # Find positions in file
  # @param file  Config file
  ##

  def analyze(file)
    @config      = IO.readlines(file)
    @config_orig = @config.clone
    opensect     = nil

    i = 0
    @config.each do |line|
      if(opensect.nil?)
        [ "OPTIONS", "PANEL", "COLORS", "GRAVITIES", "GRABS", "TAGS", "VIEWS", "HOOKS" ].each do |section|
          if(line.match(/^#{section}/))
            @positions << [ i, method(eval(":print_" + section.downcase)) ]
            opensect   = section
            break
          end
        end
      else
        if(line.match(/}$/))
          @positions.last << i
          opensect = nil
        end
      end

      i += 1
    end

    @positions.sort! { |a, b| a.first <=> b.first }
  end # }}}

begin
  old_config = nil
  new_config = nil
  bak_config = nil

  # Check arguments
  if(0 == ARGV.size)
    # Get XDG path
    xdg_config_home = File.join((ENV["XDG_CONFIG_HOME"] ||
      File.join(ENV["HOME"], ".config")), "subtle")
    xdg_config_dir = File.join((ENV["XDG_CONFIG_DIRS"].split(":").first rescue
      File.join("/etc", "xdg")), "subtle")

    # Find files
    old_config = File.join(xdg_config_home, "subtle.rb")

    unless(File.exist?(old_config))
      old_config = File.join(xdg_config_home, "subtle.rb")

      unless(File.exist?(old_config))
        puts "Couldn't find config file"
        exit
      end
    end
  elsif([ "--help", "-h", "help" ].include?(ARGV[0]))
    puts "Usage: %s [CONFIG]" % [ $0 ]
    puts "\nMigrate either given config or found user/system config to the"
    puts "new DSL and make a backup of the old one"
    puts "\nPlease report bugs to <unexist@dorfelite.net>"
    exit
  else
    old_config = ARGV[0]
  end

  # Load config
  analyze(old_config)
  require old_config

  puts ">>> Migrating `%s`" % [ old_config ]

  # Rename old config
  new_config = old_config + ".dsl"
  old_config = old_config
  bak_config = old_config + ".old"

  puts ">>> Working on `%s`" % [ new_config ]

  # Redirecting stdout
  stdout  = $stdout
  $stdout = File.new(new_config, "w")

  # Overwrite array entries
  @positions.each do |p|
    @config[p.first] = p[1]

    (p.first + 1..p.last).each do |i|
      @config[i] = nil
    end
  end

  # Dump new config
  @config.each do |l|
    if(l.is_a?(String))
      puts l
    elsif(l.is_a?(Method))
      l.call
    end
  end

  # Restoring stdout
  $stdout.close
  $stdout = stdout

  # Renaming files
  puts ">>> Renaming `%s` -> `%s`" % [ old_config, bak_config ]
  File.rename(old_config, bak_config)

  puts ">>> Renaming `%s` -> `%s`" % [ new_config, old_config ]
  File.rename(new_config, old_config)

  puts ">>> Done"
rescue => err
  puts ">>> ERROR: #{err}"
  puts err.backtrace
end
