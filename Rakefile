#
# @package subtle
# @file Rake build file
# @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require("rake/clean")
require("mkmf")
require("yaml")

# 
# Settings
#

# Options / defines {{{
@options = {
  "destdir"    => "/",
  "prefix"     => "/usr/local",
  "bindir"     => "$(prefix)/bin",
  "sysconfdir" => "$(prefix)/etc",
  "datadir"    => "$(prefix)/share",
  "debug"      => "no",
  "builddir"   => "build",
  "ccflags"    => "-Wall -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
  "cpppath"    => "-I. -I$(builddir) -Isrc -idirafter$(archdir)",
  "ldflags"    => "-L$(archdir) -l$(RUBY_SO_NAME)"
}

@defines = {
  "PACKAGE_NAME"      => "subtle",
  "PACKAGE_VERSION"   => "0.7",
  "PACKAGE_BUGREPORT" => "unexist@dorfelite.net",
  "DIR_CONFIG"        => "$(sysconfdir)/subtle",
  "DIR_SUBLET"        => "$(datadir)/subtle"
}  
# }}}

# Lists {{{
PG_WM    = "subtle"
PG_RMT   = "subtler"

SRC_WM   = FileList["src/*.c"]
SRC_RMT  = ["src/subtler.c"]

OBJ_WM   = SRC_WM.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_RMT  = SRC_RMT.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }

SUBLETS  = FileList["sublets/*.rb"]
CONF     = ["data/config.rb"]

FUNCS    = ["select"]
HEADER   = [
  "stdio.h", "stdlib.h", "stdarg.h", "string.h", "unistd.h",
  "signal.h", "errno.h", "assert.h", "regex.h", "sys/time.h",
  "sys/types.h", "sys/inotify.h", "execinfo.h"
]
# }}}

# Miscellaneous {{{
Logging.logfile("config.log") #< mkmf log
CLEAN.include(PG_WM, PG_RMT, OBJ_WM, OBJ_RMT, "config.h", "config.log", "config.yml")
CLOBBER.include(@options["builddir"])
# }}}

#
# Funcs
#

# Func: silent_sh {{{
def silent_sh(cmd, msg, &block)
  options = {}
  rake_check_options(options, :noop, :verbose)

  # Hide raw messages?
  if(!options[:verbose]) then
    rake_output_message(Array == cmd.class ? cmd.join(" ") : cmd) #< Check type
  else
    rake_output_message(msg)
  end

  unless options[:noop]
    res = system(cmd)
    block.call(res, $?)
  end
end # }}} 

# Func: make_header {{{
# FIXME: Shellwords strips quotes!
def make_header(header = "config.h")
  message "creating %s\n", header
  sym = header.tr("a-z./\055", "A-Z___")
  hdr = ["#ifndef #{sym}\n#define #{sym}\n"]
  for line in $defs
    case line
    when /^-D([^=]+)(?:=(.*))?/
      hdr << "#define #$1 #{$2 ? $2 : 1}\n"
    when /^-U(.*)/
      hdr << "#undef #$1\n"
    end
  end
  hdr << "#endif\n"
  hdr = hdr.join
  unless (IO.read(header) == hdr rescue false)
    open(header, "w") do |hfile|
      hfile.write(hdr)
    end
  end
end # }}}

# 
# Tasks
#

# Task: default {{{
desc("Configure and build subtle")
task(:default => [:config, :build])
# }}}

# Task: config {{{
desc("Configure subtle")
task(:config) do
  #Check if build dir exists
  if(!File.exists?(@options["builddir"]))
    Dir.mkdir(@options["builddir"])
  end

  # Check if options.yaml exists
  if(File.exist?("config.yml"))
    yaml = YAML::load(File.open("config.yml"))
    @options, @defines = YAML::load(yaml)
  else
    # Get options
    @options.each_key do |k|
      if(!ENV[k].nil?) then
        @options[k] = ENV[k]
      end
    end

    # Debug
    if("yes" == @options["debug"]) then
      @options["ccflags"] << " -g"
    end

    # Destdir
    if(ENV["destdir"]) then
      @options["prefix"] = "$(destdir)/$(prefix)"
    end

    # Expand options
    @options.each do |k, v|
      @options[k] = Config.expand(v, @options.merge(CONFIG))
    end

    # Expand defines
    @defines.each do |k, v|
      @defines[k] = Config.expand(v, @options)
    end
   
    # Check header
    HEADER.each do |h|
      if(!have_header(h)) then
        fail("Header #{h} was not found")
      end
    end

    # Check functions
    FUNCS.each do |f|
      if(!have_func(f)) then
        fail("Func #{f} was not found")
      end
    end

    # Check pkg-config of x11
    cflags, ldflags, libs = pkg_config("x11")
    if(libs.nil?) then
      fail("x11 was not found")
    end
    @options["ldflags"] << " #{libs}" #< We only need the lib part

    # Defines
    @defines.each do |k, v|
      $defs.push(format('-D%s="%s"', k, v))
    end

    # Debug
    if("yes" == @options["debug"]) then
      $defs.push("-DDEBUG")
    end

    # Dump options to file
    message("creating config.yml\n")
    yaml = [@options, @defines].to_yaml
    File.open("config.yml", "w+") do |out|
      YAML::dump(yaml, out)
    end  

    # Write header
    make_header("config.h")

    # Dump info
    puts <<EOF

#{@defines["PACKAGE_NAME"]} #{@defines["PACKAGE_VERSION"]}
-----------------
Install path........: #{@options["prefix"]}
Binary..............: #{@options["bindir"]}
Configuration.......: #{@defines["DIR_CONFIG"]}
Sublets.............: #{@defines["DIR_SUBLET"]}

Debugging messages..: #{@options["debug"]}

EOF
  end
end # }}}

# Task: build {{{
desc("Build subtle")
task(:build => [:config, PG_WM, PG_RMT])
# }}}

# Task: subtle/subtler {{{
task(PG_WM => [:config])
task(PG_RMT => [:config])
# }}}

# Task: install {{{
desc("Install subtle")
task(:install => [:config, :build]) do
  message("Not implemented yet!\n")
end # }}}

# Task: help {{{
desc("Show help")
task(:help => [:config]) do
    puts <<EOF
destdir=PATH       Set intermediate install prefix (current: #{@options["destdir"]})
prefix=PATH        Set install prefix (current: #{@options["prefix"]})
bindir=PATH        Set binary directory (current: #{@options["bindir"]})
sysconfdir=PATH    Set config directory (current: #{@options["sysconfdir"]})
datadir=PATH       Set data directory (current: #{@options["datadir"]})
debug=yes          Build with debugging messages (current: #{@options["debug"]})
EOF
end # }}}

#
# File tasks
#

# Task: compile {{{
(SRC_WM | SRC_RMT).each do |src|
  out = File.join(@options["builddir"], File.basename(src).ext("o"))
  file(out => src) do
    silent_sh("gcc -o #{out} -c #{@options["ccflags"]} #{@options["cpppath"]} #{src}", "CC #{out}") do |ok, status|
      ok or fail("Compiler failed with status #{status.exitstatus}")
    end
  end
end # }}}

# Task: link {{{
file(PG_WM => OBJ_WM) do
  out = File.join(@options["builddir"], PG_WM)
  silent_sh("gcc -o #{out} #{@options["ldflags"]} #{OBJ_WM}", "LD #{out}") do |ok, status|
    ok or fail("Linker failed with status #{status.exitstatus}")
  end
end

file(PG_RMT => OBJ_RMT) do
  out = File.join(@options["builddir"], PG_RMT)
  silent_sh("gcc -o #{out} #{OBJ_RMT} #{@options["ldflags"]}", "LD #{out}") do |ok, status|
    ok or fail("Linker failed with status #{status.exitstatus}")
  end
end # }}}
