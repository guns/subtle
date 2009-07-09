#
# @package subtle
# @file Rake build file
# @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require("rake/clean")
require("mkmf")
require("yaml")
require("fileutils")

# 
# Settings
#
# XDG paths {{{
@xdg_config = ENV["XDG_CONFIG_DIRS"] ? ENV["XDG_CONFIG_DIRS"].split(":").first : "/etc/xdg"
@xdg_data   = ENV["XDG_DATA_DIRS"]   ? ENV["XDG_DATA_DIRS"].split(":").first   : "$(prefix)/share"
# }}}

# Options / defines {{{
@options = {
  "destdir"    => "",
  "xdg_config" => @xdg_config + "/$(PKG_NAME)",
  "xdg_data"   => @xdg_data + "/$(PKG_NAME)",
  "bindir"     => "$(destdir)/usr/bin",
  "sysconfdir" => "$(destdir)/$(xdg_config)",
  "datadir"    => "$(destdir)/$(xdg_data)",
  "subletdir"  => "$(destdir)/$(xdg_data)/sublets",
  "scriptdir"  => "$(destdir)/$(xdg_data)/scripts",
  "extdir"     => "$(destdir)/$(sitelibdir)/$(PKG_NAME)",
  "debug"      => "no",
  "builddir"   => "build",
  "hdrdir"     => "",
  "archdir"    => "",
  "revision"   => "0",
  "cflags"     => "-Wall -Werror -Wtype-limits -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
  "cpppath"    => "-I. -I$(builddir) -Isrc -Isrc/shared -Isrc/subtle -idirafter$(hdrdir) -idirafter$(archdir)",
  "ldflags"    => "-L$(archdir) -l$(RUBY_SO_NAME)",
  "extflags"   => "$(LDFLAGS) $(LIBRUBYARG_SHARED)"
}

@defines = {
  "PKG_NAME"      => "subtle",
  "PKG_VERSION"   => "0.8.$(revision)",
  "PKG_BUGREPORT" => "unexist@dorfelite.net",
  "PKG_CONFIG"    => "subtle.rb",
  "RUBY_VERSION"  => "$(MAJOR).$(MINOR).$(TEENY)",
  "DIR_CONFIG"    => "$(xdg_config)",
  "DIR_SUBLET"    => "$(xdg_data)/sublets"
}  # }}}

# Lists {{{
PG_WM   = "subtle"
PG_RMT  = "subtler"
PG_RBE  = "subtlext"

SRC_WM  = FileList["src/subtle/*.c"]
SRC_RMT = FileList["src/subtler/*.c"]
SRC_RBE = FileList["src/subtlext/*.c"]
SRC_SHD = FileList["src/shared/*.c"]

OBJ_WM  = SRC_WM.collect  { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_RMT = SRC_RMT.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_RBE = SRC_RBE.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_SHD = SRC_SHD.collect { |f| File.join(@options["builddir"], File.basename(f).ext("o")) }
OBJ_SHW = SRC_SHD.collect { |f| File.join(@options["builddir"], File.basename(f).ext("wm.o")) }

FUNCS   = ["select"]
HEADER  = [
  "stdio.h", "stdlib.h", "stdarg.h", "string.h", "unistd.h", "signal.h", "errno.h", 
  "assert.h", "regex.h", "sys/time.h", "sys/types.h", "sys/inotify.h", "execinfo.h"
]
# }}}

# Miscellaneous {{{
Logging.logfile("config.log") #< mkmf log
CLEAN.include(PG_WM, PG_RMT, "#{PG_RBE}.so", OBJ_WM, OBJ_SHD, OBJ_SHW, OBJ_RMT, OBJ_RBE)
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

# Func: compile {{{
def compile(src, out = nil, options = "")
  out = File.join(@options["builddir"], File.basename(src).ext("o")) unless(!out.nil?)
  opt = ["shared.c", "shared.wm.c", "subtlext.c"].include?(File.basename(src)) ? " -fPIC " : ""
  opt << options

  silent_sh("gcc -o #{out} -c #{@options["cflags"]} #{opt} #{@options["cpppath"]} #{src}",
    "CC #{out}") do |ok, status|
      ok or fail("Compiler failed with status #{status.exitstatus}")
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
      @options["cflags"] << " -g -DDEBUG"
    else
      @options["cflags"] << " -g -DNDEBUG"
    end
    
    # Get revision
    if(File.exists?(".hg") && (hg = find_executable0("hg")))
      match = `#{hg} tip`.match(/changeset:\s*(\d+).*/)

      if(!match.nil? && 2 == match.size )
        @options["revision"] = match[1]
      else
        @options["revision"] = "UNKNOWN"
      end
    end  

    # Get header dir
    if(CONFIG["rubyhdrdir"].nil?)
      @options["hdrdir"]  = Config.expand(CONFIG["archdir"]) 
    else
      @options["hdrdir"]  = Config.expand(CONFIG["rubyhdrdir"]) 
    end

    @options["archdir"] = File.join(
      @options["hdrdir"], 
      Config.expand(CONFIG["arch"])
    )

    # Expand options and defines
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

    # Xinerama
    if(have_header("X11/extensions/Xinerama.h"))
      @options["ldflags"] << " -lXinerama"
    end
    
    # Update flags
    @options["cflags"] << " %s" % [cflags]
    @options["ldflags"] << " %s" % [libs]
    @options["extflags"] << " %s" % [libs]

    # Defines
    @defines.each do |k, v|
      $defs.push(format('-D%s="%s"', k, v))
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
Binaries............: #{@options["bindir"]}
Configuration.......: #{@options["sysconfdir"]}
Sublets.............: #{@options["datadir"]}
Extension...........: #{@options["extdir"]}

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
task(PG_WM => [:config, OBJ_SHW])

desc("Build subtler")
task(PG_RMT => [:config, OBJ_SHD])

desc("Build subtlext")
task(PG_RBE => [:config, OBJ_SHD])
# }}}

# Task: install {{{
desc("Install subtle")
task(:install => [:config, :build]) do
  FileUtils.mkdir_p( 
    [
      @options["bindir"],
      @options["sysconfdir"],
      @options["datadir"],
      @options["subletdir"],
      @options["scriptdir"],
      @options["extdir"]
    ]
  )

  message("INSTALL %s\n" % [PG_WM])
  FileUtils.install(PG_WM, @options["bindir"], :mode => 0755, :verbose => false)

  message("INSTALL %s\n" % [PG_RMT])
  FileUtils.install(PG_RMT, @options["bindir"], :mode => 0755, :verbose => false)

  message("INSTALL %s\n" % [PG_RBE])
  FileUtils.install(PG_RBE + ".so", @options["extdir"], :mode => 0644, :verbose => false)

  FileList["dist/scripts/*.*"].collect do |f|
    message("INSTALL %s\n" % [File.basename(f)])
    FileUtils.install(f, @options["scriptdir"], :mode => 0644, :verbose => false)
  end

  FileList["dist/sublets/*.rb"].collect do |f|
    message("INSTALL %s\n" % [File.basename(f)])
    FileUtils.install(f, @options["subletdir"], :mode => 0644, :verbose => false)
  end

  message("INSTALL %s\n" % [@defines["PKG_CONFIG"]])
  FileUtils.install("dist/" + @defines["PKG_CONFIG"], @options["sysconfdir"], :mode => 0644, :verbose => false)
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

# Task: test {{{
desc("Run tests")
task(:test => [:build]) do
  if(!ENV["sublet"].nil?)
    require("test/unit/sublet_test")
    require(ENV["sublet"])
  else
    require("test/subtlext_suite")
  end
end # }}}

#
# File tasks
#
# Task: compile {{{
(SRC_SHD | SRC_WM).each do |src|
  ext = SRC_SHD.include?(src) ? "wm.o" : "o"
  out = File.join(@options["builddir"], File.basename(src).ext(ext))

  file(out => src) do
    compile(src, out, "-DWM")
  end
end 

(SRC_SHD | SRC_RMT | SRC_RBE).each do |src|
  out = File.join(@options["builddir"], File.basename(src).ext("o"))

  file(out => src) do
    compile(src, out)
  end
end 
# }}}

# Task: link {{{
file(PG_WM => OBJ_WM) do
  silent_sh("gcc -o #{PG_WM} #{OBJ_SHW} #{OBJ_WM} #{@options["ldflags"]}", 
    "LD #{PG_WM}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end

file(PG_RMT => OBJ_RMT) do
  silent_sh("gcc -o #{PG_RMT} #{OBJ_SHD} #{OBJ_RMT} #{@options["ldflags"]}", 
    "LD #{PG_RMT}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end

file(PG_RBE => OBJ_RBE) do
  silent_sh("gcc -o #{PG_RBE}.so #{OBJ_SHD} #{OBJ_RBE} -shared #{@options["extflags"]}", 
    "LD #{PG_RBE}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
