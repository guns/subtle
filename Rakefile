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
  "cflags"     => "-Wall -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
  "cpppath"    => "-I. -I$(builddir) -Isrc -Isrc/shared -idirafter$(archdir)",
  "ldflags"    => "-L$(archdir) -l$(RUBY_SO_NAME)",
  "extflags"   => "$(LDFLAGS) $(LIBRUBYARG_SHARED)"
}

@defines = {
  "PKG_NAME"      => "subtle",
  "PKG_VERSION"   => "0.8.%s" % ["$Rev$".delete("/[a-zA-Z$: ]/")], #< Get revision
  "PKG_BUGREPORT" => "unexist@dorfelite.net",
  "RUBY_VERSION"  => "$(MAJOR).$(MINOR).$(TEENY)",
  "DIR_CONFIG"    => "$(sysconfdir)/subtle",
  "DIR_SUBLET"    => "$(datadir)/subtle"
}  
# }}}

# Lists {{{
PG_WM   = "subtle"
PG_RMT  = "subtler"
PG_RBE  = "subtlext"

SRC_WM  = FileList["src/subtle/*.c"]
SRC_SHD = FileList["src/shared/*.c"]
SRC_RMT = FileList["src/subtler/*.c"]
SRC_RBE = FileList["src/subtlext/*.c"]

OBJ_WM  = SRC_WM.collect  { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_SHD = SRC_SHD.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_RMT = SRC_RMT.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_RBE = SRC_RBE.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }

SUBLETS = FileList["sublets/*.rb"]
CONF    = ["dist/config.yml"]

FUNCS   = ["select"]
HEADER  = [
  "stdio.h", "stdlib.h", "stdarg.h", "string.h", "unistd.h",
  "signal.h", "errno.h", "assert.h", "regex.h", "sys/time.h",
  "sys/types.h", "sys/inotify.h", "execinfo.h"
]
# }}}

# Miscellaneous {{{
Logging.logfile("config.log") #< mkmf log
CLEAN.include(PG_WM, PG_RMT, "#{PG_RBE}.so", OBJ_WM, OBJ_SHD, OBJ_RMT, OBJ_RBE, "config.h", "config.log", "config.yml")
CLOBBER.include(@options["builddir"])
# }}}

#
# Funcs
#

# Func: silent_sh {{{
def silent_sh(cmd, msg, &block)
  # Hide raw messages?
  if(true === RakeFileUtils.verbose_flag) then
    rake_output_message(Array == cmd.class ? cmd.join(" ") : cmd) #< Check type
  else
    rake_output_message(msg)
  end

  unless RakeFileUtils.nowrite_flag
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
      @options["cflags"] << " -g"
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
      @defines[k] = Config.expand(v, @options.merge(CONFIG))
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

    # Update flags
    @options["cflags"] << " %s" % [cflags]
    @options["ldflags"] << " %s" % [libs]
    @options["extflags"] << " %s" % [libs]

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

#{@defines["PKG_NAME"]} #{@defines["PKG_VERSION"]}
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
desc("Build all")
task(:build => [:config, PG_WM, PG_RMT, PG_RBE])
# }}}

# Task: subtle/subtler/subtlext {{{
desc("Build subtle")
task(PG_WM => [:config])

desc("Build subtler")
task(PG_RMT => [:config, OBJ_SHD])

desc("Build subtlext")
task(PG_RBE => [:config, OBJ_SHD])
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
(SRC_WM | SRC_SHD | SRC_RMT | SRC_RBE).each do |src|
  out = File.join(@options["builddir"], File.basename(src).ext("o"))
  file(out => src) do
    silent_sh("gcc -o #{out} -c #{@options["cflags"]} #{@options["cpppath"]} #{src}", "CC #{out}") do |ok, status|
      ok or fail("Compiler failed with status #{status.exitstatus}")
    end
  end
end # }}}

# Task: link {{{
file(PG_WM => OBJ_WM) do
  silent_sh("gcc -o #{PG_WM} #{@options["ldflags"]} #{OBJ_WM}", "LD #{PG_WM}") do |ok, status|
    ok or fail("Linker failed with status #{status.exitstatus}")
  end
end

file(PG_RMT => OBJ_RMT) do
  silent_sh("gcc -o #{PG_RMT} #{OBJ_SHD} #{OBJ_RMT} #{@options["ldflags"]}", "LD #{PG_RMT}") do |ok, status|
    ok or fail("Linker failed with status #{status.exitstatus}")
  end

end

file(PG_RBE => OBJ_RBE) do
  silent_sh("gcc -o #{PG_RBE}.so #{OBJ_SHD} #{OBJ_RBE} -shared #{@options["extflags"]}", "LD #{PG_RBE}") do |ok, status|
    ok or fail("Linker failed with status #{status.exitstatus}")
  end
end # }}}
