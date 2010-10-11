#
# @package subtle
#
# @file Rake build file
# @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require "mkmf"
require "yaml"
require "fileutils"
require "rake/clean"
require "rake/rdoctask"

#
# Settings
#
# Options / defines {{{
@options = {
  "destdir"    => "",
  "prefix"     => "/usr",
  "manprefix"  => "$(prefix)/share/man",
  "bindir"     => "$(destdir)/$(prefix)/bin",
  "sysconfdir" => "$(destdir)/etc",
  "configdir"  => "$(sysconfdir)/xdg/$(PKG_NAME)",
  "datadir"    => "$(destdir)/$(prefix)/share/$(PKG_NAME)",
  "scriptdir"  => "$(datadir)/scripts",
  "extdir"     => "$(destdir)/$(sitelibdir)/$(PKG_NAME)",
  "mandir"     => "$(destdir)/$(manprefix)/man1",
  "debug"      => "no",
  "xft"        => "yes",
  "xinerama"   => "yes",
  "xrandr"     => "yes",
  "builddir"   => "build",
  "hdrdir"     => "",
  "archdir"    => "",
  "revision"   => "0",
  "cflags"     => "-Wall -Werror -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
  "cpppath"    => "-I. -I$(builddir) -Isrc -Isrc/shared -Isrc/subtle -idirafter$(hdrdir) -idirafter$(archdir)",
  "ldflags"    => "-L$(archdir) $(rpath) -l$(RUBY_SO_NAME)",
  "extflags"   => "$(LDFLAGS) $(rpath) -l$(RUBY_SO_NAME)",
  "rpath"      => "-Wl,-rpath=$(libdir)"
}

@defines = {
  "PKG_NAME"      => "subtle",
  "PKG_VERSION"   => "0.9.$(revision)",
  "PKG_BUGREPORT" => "unexist@dorfelite.net",
  "PKG_CONFIG"    => "subtle.rb",
  "RUBY_VERSION"  => "$(MAJOR).$(MINOR).$(TEENY)"
}  # }}}

# Lists {{{
PG_SUBTLE   = "subtle"
PG_SUBTLEXT = "subtlext"
PG_SUBTLER  = "subtler"
PG_SUR      = "sur"
PG_SERVER   = "surserver"

SRC_SHARED   = FileList["src/shared/*.c"]
SRC_SUBTLE   = (SRC_SHARED | FileList["src/subtle/*.c"])
SRC_SUBTLEXT = (SRC_SHARED | FileList["src/subtlext/*.c"])

# Collect object files
OBJ_SUBTLE = SRC_SUBTLE.collect do |f|
  File.join(@options["builddir"], "subtle", File.basename(f).ext("o"))
end

OBJ_SUBTLEXT = SRC_SUBTLEXT.collect do |f|
  File.join(@options["builddir"], "subtlext", File.basename(f).ext("o"))
end

FUNCS   = [ "select" ]
HEADER  = [
  "stdio.h", "stdlib.h", "stdarg.h", "string.h", "unistd.h", "signal.h", "errno.h",
  "assert.h", "sys/time.h", "sys/types.h"
]
OPTIONAL = [ "sys/inotify.h", "execinfo.h" ]
# }}}

# Miscellaneous {{{
Logging.logfile("config.log") #< mkmf log
CLEAN.include(PG_SUBTLE, "#{PG_SUBTLEXT}.so", OBJ_SUBTLE, OBJ_SUBTLEXT)
CLOBBER.include(@options["builddir"], "config.h", "config.log", "config.yml")
# }}}

#
# Funcs
#
# Func: silent_sh {{{
def silent_sh(cmd, msg, &block)
  # Hide raw messages?

  if(:default == RakeFileUtils.verbose)
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
  opt = ["shared.c", "subtlext.c"].include?(File.basename(src)) ? " -fPIC " : ""
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

  # Create more build dirs
  FileUtils.mkdir_p(
    [
      File.join(@options["builddir"], "subtle"),
      File.join(@options["builddir"], "subtlext")
    ]
  )

  # Check if options.yaml exists or config is run per target
  if((!ARGV.nil? && !ARGV.include?("config")) && File.exist?("config.yml"))
    yaml = YAML::load(File.open("config.yml"))
    @options, @defines = YAML::load(yaml)
  else
    # Check version
    if(1 != CONFIG["MAJOR"].to_i or 9 != CONFIG["MINOR"].to_i)
      fail("Ruby 1.9.0 or higher required")
    end

    # Update rpath
    @options["rpath"] = Config.expand(@options["rpath"])

    # Get options
    @options.each_key do |k|
      if(!ENV[k].nil?)
        @options[k] = ENV[k]
      end
    end

    # Debug
    if("yes" == @options["debug"])
      @options["cflags"] << " -g -DDEBUG"
    else
      @options["cflags"] << " -g -DNDEBUG"
    end

    # Get revision
    if(File.exists?(".hg") && (hg = find_executable0("hg")))
      match = `#{hg} tip`.match(/changeset:\s*(\d+).*/)

      if(!match.nil? && 2 == match.size)
        @options["revision"] = match[1]
      else
        @options["revision"] = "9999"
      end
    end

    # Get ruby header dir
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
      if(!have_header(h))
        fail("Header #{h} was not found")
      end
    end

    # Check optional headers
    OPTIONAL.each do |h|
      have_header(h)
    end

    # Check pkg-config for X11
    checking_for("X11/Xlib.h") do
      cflags, ldflags, libs = pkg_config("x11")
      if(libs.nil?)
        fail("X11 was not found")
      end

      # Update flags
      @options["cflags"]   << " %s" % [ cflags ]
      @options["ldflags"]  << " %s" % [ libs ]
      @options["extflags"] << " %s" % [ libs ]

      true
    end

    # Check pkg-config for Xft
    if("yes" == @options["xft"])
      checking_for("X11/Xft/Xft.h") do
        ret = false

        cflags, ldflags, libs = pkg_config("xft")
        unless(libs.nil?)
          # Update flags
          @options["cpppath"] << " %s" % [ cflags ]
          @options["ldflags"] << " %s" % [ libs ]
          @options["extflags"] << " %s" % [ libs ]

          $defs.push("-DHAVE_X11_XFT_XFT_H")
          ret = true
        else
          @options["xft"] = "no"
        end

        ret
      end
    end

    # Xinerama
    if("yes" == @options["xinerama"])
      if(have_header("X11/extensions/Xinerama.h"))
        @options["ldflags"]  << " -lXinerama"
        @options["extflags"] << " -lXinerama"
      end
    end

    # Check Xrandr
    if("yes" == @options["xrandr"])
      ret = false

      # Pkg-config
      checking_for("X11/extensions/Xrandr.h") do
        cflags, ldflags, libs = pkg_config("xrandr")
        unless(libs.nil?)
          # Version check (xrandr >= 1.3)
          if(try_func("XRRGetScreenResourcesCurrent", libs))
            # Update flags
            @options["cflags"]  << " %s" % [ cflags ]
            @options["ldflags"] << " %s" % [ libs ]

            $defs.push("-DHAVE_X11_EXTENSIONS_XRANDR_H")

            ret = true
          else
            puts "XRRGetScreenResourcesCurrent couldn't be found"
          end
        end

        @options["xrandr"] = "no" unless(ret)

        ret
      end
    end

    # Check functions
    FUNCS.each do |f|
      if(!have_func(f))
        fail("Func #{f} was not found")
      end
    end

    # Encoding
    have_func("rb_enc_set_default_internal")

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
Configuration.......: #{@options["configdir"]}
Scripts.............: #{@options["scriptdir"]}
Extension...........: #{@options["extdir"]}

Xft support.........: #{@options["xft"]}
Xinerama support....: #{@options["xinerama"]}
XRandR support......: #{@options["xrandr"]}
Debugging messages..: #{@options["debug"]}

EOF
  end
end # }}}

# Task: build {{{
desc("Build all")
task(:build => [:config, PG_SUBTLE, PG_SUBTLEXT])
# }}}

# Task: subtle/subtlext {{{
desc("Build subtle")
task(PG_SUBTLE => [:config])

desc("Build subtlext")
task(PG_SUBTLEXT => [:config])
# }}}

# Task: install {{{
desc("Install subtle")
task(:install => [:config, :build]) do
  # Make install dirs
  FileUtils.mkdir_p(
    [
      @options["bindir"],
      @options["configdir"],
      @options["scriptdir"],
      @options["extdir"],
      @options["mandir"],
      File.join(@options["extdir"], "subtler"),
      File.join(@options["extdir"], "sur")
    ]
  )

  # Install config
  message("INSTALL config\n" % [ @defines["PKG_CONFIG"] ])
  FileUtils.install(
    File.join("contrib", @defines["PKG_CONFIG"]),
    @options["configdir"],
    :mode => 0644, :verbose => false
  )

  # Install subtle
  message("INSTALL %s\n" % [PG_SUBTLE])
  FileUtils.install(
    PG_SUBTLE, @options["bindir"],
    :mode => 0755, :verbose => false
  )

  # Install subtlext
  message("INSTALL %s\n" % [PG_SUBTLEXT])
  FileUtils.install(
    PG_SUBTLEXT + ".so", @options["extdir"],
    :mode => 0644, :verbose => false
  )

  # Get path of sed and ruby interpreter
  sed         = find_executable0("sed")
  interpreter = File.join(Config.expand(CONFIG["bindir"]), 
    CONFIG["ruby_install_name"]
  )

  # Install subtler
  message("INSTALL subtler\n")
  FileList["contrib/subtler/*.rb"].collect do |f|
    FileUtils.install(f,
    File.join(@options["extdir"], "subtler"),
      :mode => 0644, :verbose => false
    )
  end

  # Install sur
  message("INSTALL sur\n")
  FileList["contrib/sur/*.rb"].collect do |f|
    FileUtils.install(f,
      File.join(@options["extdir"], "sur"),
      :mode => 0644, :verbose => false
    )
  end

  # Install tools
  message("INSTALL tools\n")
  FileList["contrib/bin/*"].collect do |f|
    FileUtils.install(f, @options["bindir"],
      :mode => 0755, :verbose => false
    )

    # Update interpreter name
    `#{sed} -i -e 's#/usr/bin/ruby.*##{interpreter}#' \
      #{File.join(@options["bindir"], File.basename(f))}`
  end

  # Install scripts
  message("INSTALL scripts\n")
  FileList["contrib/scripts/*.*"].collect do |f|
    FileUtils.install(f, @options["scriptdir"],
      :mode => 0644, :verbose => false
    )

    # Update interpreter name
    `#{sed} -i -e 's#/usr/bin/ruby.*##{interpreter}#' \
      #{File.join(@options["scriptdir"], File.basename(f))}`
  end

  # Install manpages
  message("INSTALL manpages\n")
  FileList["contrib/man/*.*"].collect do |f|
    FileUtils.install(f, @options["mandir"],
    :mode => 0644, :verbose => false)
  end
end # }}}

# Task: help {{{
desc("Show help")
task(:help => [:config]) do
  puts <<EOF
destdir=PATH       Set intermediate install prefix (current: #{@options["destdir"]})
prefix=PATH        Set install prefix (current: #{@options["prefix"]})
manprefix=PATH     Set install prefix for manpages (current: #{@options["manprefix"]})
bindir=PATH        Set binary directory (current: #{@options["bindir"]})
sysconfdir=PATH    Set config directory (current: #{@options["sysconfdir"]})
datadir=PATH       Set data directory (current: #{@options["datadir"]})
mandir=PATH        Set man directory (current: #{@options["mandir"]})
debug=[yes|no]     Whether to build with debugging messages (current: #{@options["debug"]})
xft=[yes|no]       Whether to build with Xft support (current: #{@options["xft"]})
xinerama=[yes|no]  Whether to build with Xinerama support (current: #{@options["xinerama"]})
randr=[yes|no]     Whether to build with XRandR support (current: #{@options["xrandr"]})
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

# Task: rdoc {{{
Rake::RDocTask.new(:rdoc) do |rdoc|
  rdoc.rdoc_files.include(
    "contrib/subtle.rb",
    "src/subtle/ruby.c",
    "src/subtlext/subtlext.c"
  )
  rdoc.options << "-o doc"
  rdoc.title    = "Subtle RDoc Documentation"
end # }}}

#
# File tasks
#
# Task: compile {{{
SRC_SUBTLE.each do |src|
  out = File.join(@options["builddir"], PG_SUBTLE, File.basename(src).ext("o"))

  file(out => src) do
    compile(src, out, "-D#{PG_SUBTLE.upcase}")
  end
end

SRC_SUBTLEXT.each do |src|
  out = File.join(@options["builddir"], PG_SUBTLEXT, File.basename(src).ext("o"))

  file(out => src) do
    compile(src, out, "-D#{PG_SUBTLEXT.upcase} -fPIC")
  end
end
# }}}

# Task: link {{{
file(PG_SUBTLE => OBJ_SUBTLE) do
  silent_sh("gcc -o #{PG_SUBTLE} #{OBJ_SUBTLE} #{@options["ldflags"]}",
    "LD #{PG_SUBTLE}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end

file(PG_SUBTLEXT => OBJ_SUBTLEXT) do
  silent_sh("gcc -o #{PG_SUBTLEXT}.so #{OBJ_SUBTLEXT} -shared #{@options["extflags"]}",
    "LD #{PG_SUBTLEXT}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
