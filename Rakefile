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
require("ftools")

# 
# Settings
#
# Options / defines {{{
@options = {
  "destdir"    => "",
  "prefix"     => "$(destdir)/usr/local",
  "bindir"     => "$(prefix)/bin",
  "sysconfdir" => "$(prefix)/etc",
  "datadir"    => "$(prefix)/share",
  "debug"      => "no",
  "builddir"   => "build",
  "archdir"    => "",
  "revision"   => "0",
  "cflags"     => "-Wall -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
  "cpppath"    => "-I. -I$(builddir) -Isrc -Isrc/shared -idirafter$(archdir)",
  "ldflags"    => "-L$(archdir) -l$(RUBY_SO_NAME)",
  "extflags"   => "$(LDFLAGS) $(LIBRUBYARG_SHARED)"
}

@defines = {
  "PKG_NAME"      => "subtle",
  "PKG_VERSION"   => "0.8.$(revision)",
  "PKG_BUGREPORT" => "unexist@dorfelite.net",
  "PKG_CONFIG"    => "subtle.rb",
  "RUBY_VERSION"  => "$(MAJOR).$(MINOR).$(TEENY)",
  "DIR_CONFIG"    => "$(sysconfdir)/$(PKG_NAME)",
  "DIR_SUBLET"    => "$(datadir)/$(PKG_NAME)",
  "DIR_EXT"       => "$(sitelibdir)/$(PKG_NAME)"
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

FUNCS   = ["select"]
HEADER  = [
  "stdio.h", "stdlib.h", "stdarg.h", "string.h", "unistd.h",
  "signal.h", "errno.h", "assert.h", "regex.h", "sys/time.h",
  "sys/types.h", "sys/inotify.h", "execinfo.h"
]
# }}}

# Miscellaneous {{{
Logging.logfile("config.log") #< mkmf log
CLEAN.include(PG_WM, PG_RMT, "#{PG_RBE}.so", OBJ_WM, OBJ_SHD, OBJ_RMT, OBJ_RBE)
CLOBBER.include(@options["builddir"], "config.h", "config.log", "config.yml")
# }}}

#
# Funcs
#
# Func: silent_sh {{{
def silent_sh(cmd, msg, &block)
  # Hide raw messages?

  if(:default == RakeFileUtils.verbose) then
    rake_output_message(msg)
  else
    rake_output_message(Array == cmd.class ? cmd.join(" ") : cmd) #< Check type
  end

  unless RakeFileUtils.nowrite
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

  # Check if options.yaml exists or config is run per target
  if((!ARGV.nil? && !ARGV.include?("config")) && File.exist?("config.yml"))
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
    
    # Get revision
    if((@hg = find_executable0("hg")))
      @options["revision"] = `#{@hg} tip`.match(/changeset:\s*(\d+).*/)[1]
    end  

    # Expand options and defines
    @options["archdir"]    = Config.expand(CONFIG["archdir"]) 
    @options["sitelibdir"] = Config.expand(CONFIG["sitelibdir"])
    [@options, @defines].each do |hash|
      hash.each do |k, v|
        hash[k] = Config.expand(v, CONFIG.merge(@options.merge(@defines)))
      end
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

    # Check pkg-config for X11
    cflags, ldflags, libs = pkg_config("x11")
    if(libs.nil?) then
      fail("X11 was not found")
    end
    
    # Update flags
    @options["cflags"] << " %s" % [cflags]
    @options["ldflags"] << " %s" % [libs]
    @options["extflags"] << " %s" % [libs]

    # Check pkg-config for Xft
    cflags, ldflags, libs = pkg_config("xft")
    if(libs.nil?) then
      fail("Xft was not found")
    end

    # Update flags
    @options["cpppath"] << " %s" % [cflags]
    @options["ldflags"] << " %s" % [libs]

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
Install prefix......: #{@options["prefix"]}
Binaries............: #{@options["bindir"]}
Configuration.......: #{@defines["DIR_CONFIG"]}
Sublets.............: #{@defines["DIR_SUBLET"]}
Extension...........: #{@defines["DIR_EXT"]}

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
  File.makedirs(
    @options["bindir"],
    @options["sysconfdir"],
    @defines["DIR_CONFIG"],
    @defines["DIR_SUBLET"],
    @defines["DIR_EXT"]
  )

  message("INSTALL %s\n" % [PG_WM])
  File.install(PG_WM, @options["bindir"], 0755, false)

  message("INSTALL %s\n" % [PG_RMT])
  File.install(PG_RMT, @options["bindir"], 0755, false)

  message("INSTALL %s\n" % [PG_RBE])
  File.install(PG_RBE + ".so", @defines["DIR_EXT"], 0644, false)

  FileList["dist/sublets/*.rb"].collect do |f|
    message("INSTALL %s\n" % [File.basename(f)])
    File.install(f, @defines["DIR_SUBLET"], 0644, false)
  end

  message("INSTALL %s\n" % [@defines["PKG_CONFIG"]])
  File.install("dist/" + @defines["PKG_CONFIG"], @defines["DIR_CONFIG"], 0644, false)

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

puts @options["cflags"]
  out  = File.join(@options["builddir"], File.basename(src).ext("o"))

  file(out => src) do
    fpic = "shared.c" == File.basename(src) ? "-fPIC" : ""
    silent_sh("gcc -o #{out} -c #{@options["cflags"]} #{fpic} #{@options["cpppath"]} #{src}", "CC #{out}") do |ok, status|
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
