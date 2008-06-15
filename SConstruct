#
#	@package subtle - window manager
#
# @file SConstruct file
#	@copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
#	@version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

import os.path

# Options
opts = Options("config.cache")
opts.AddOptions(
	PathOption("prefix", "Set install prefix", "/usr/local", PathOption.PathAccept),
	PathOption("bindir", "Set binary directory", "${prefix}/bin", PathOption.PathAccept),
	PathOption("datadir", "Set data directory", "${prefix}/share", PathOption.PathAccept),
	PathOption("sysconfdir", "Set config directory", "${prefix}/etc", PathOption.PathAccept),
	PathOption("destdir", "Set intermediate install prefix", "/", PathOption.PathAccept),
	BoolOption("debug", "Build with debugging messages", 0),
)

# Environment
env	= Environment(
	options = opts,
	CPPPATH = "..", 
	CCFLAGS = " -Wall -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
	CCCOMSTR = "CC $TARGET",
	LINKCOMSTR = "LD $TARGET",
	INSTALLSTR = "INSTALL $SOURCE"
)

env.SConsignFile()
env.Help(opts.GenerateHelpText(env))
env.Clean("dest", ["config.h", "config.cache", "config.log"])
opts.Save("config.cache", env)

if env.GetOption("clean"):
	if os.path.isfile("config.h"):
		os.remove("config.h")
	if os.path.isfile("config.cache"):
		os.remove("config.cache")
	if os.path.isfile("config.log"):
		os.remove("config.log")

# Destdir for various packages systems
if env["destdir"]:
	env["prefix"] = env.subst("$destdir/$prefix")
	env["bindir"] = env.subst("$destdir/$bindir")
	env["datadir"] = env.subst("$destdir/$datadir")
	env["sysconfdir"] = env.subst("$destdir/$sysconfdir")

defines = {
	"PACKAGE_NAME": "subtle",
	"PACKAGE_VERSION": "0.7",
	"PACKAGE_BUGREPORT": "unexist@dorfelite.net"
}

defines["CONFIG_DIR"] = env.subst("$sysconfdir/" + defines["PACKAGE_NAME"])
defines["SUBLET_DIR"] = env.subst("$datadir/" + defines["PACKAGE_NAME"])

# Debugging messages
debug = "no"
if env["debug"]:
	env.Append(CCFLAGS = " -g -DDEBUG")
	debug = "yes"
else:
	env.Append(CCFLAGS = " -DNDEBUG")

# Custom checks
def CheckPKGConfig(context, version):
	context.Message("Checking for pkg-config... ")
	ret = context.TryAction("pkg-config --atleast-pkgconfig-version=%s" % version)[0]
	context.Result(ret)

	if not ret:
		print("pkg-config >= %s not found." % version)
		Exit(1)

def CheckPKG(context, name, version):
	context.Message("Checking for %s... " % name)
	ret = context.TryAction("pkg-config --atleast-version=%s %s" % (version, name))[0]
	context.Result(ret)

	if not ret:
		print("%s >= %s not found." % (name, version))
		Exit(1)

def CheckCHeaders(context, headers):
	for h in headers:
		if not context.CheckCHeader(h):
			print("You need '%s' to compile this program" % h)
			Exit(1)

def CheckFunctions(context, funcs):
	for f in funcs:
		if not context.CheckFunc(f):
			print("You need '%s()' to compile this program" % f)
			Exit(1)

# Configuration
if not os.path.isfile("config.h") and not env.GetOption("clean"):
	conf = Configure(env,
		custom_tests = {
			"CheckPKGConfig": CheckPKGConfig,
			"CheckPKG": CheckPKG,
			"CheckCHeaders": CheckCHeaders,
			"CheckFunctions":	CheckFunctions
	})

	# Check various header and function
	CheckCHeaders(conf, Split("""
		stdio.h
		stdlib.h
		stdarg.h
		string.h
		unistd.h
		signal.h
		errno.h
		assert.h
		regex.h
		sys/time.h
		sys/types.h
	"""))

	if conf.CheckCHeader("sys/inotify.h"):
		defines["HAVE_SYS_INOTIFY_H"] = "true"
	
	if conf.CheckCHeader("execinfo.h"):
		defines["HAVE_EXECINFO_H"] = "true"

	CheckFunctions(conf, Split("""
		select
		strdup
		strcpy
		strerror
	"""))

	conf.CheckPKGConfig("0.15")
	conf.CheckPKG("lua", "5.1")
	conf.CheckPKG("x11", "1.0")

	env = conf.Finish()

	# Dump config
	conf_h = file("config.h", "w")
	for key, value in defines.items():
		conf_h.write("#define %s \"%s\"\n" % (key, value))
	conf_h.close()

	# Dump info
	print
	print("%s %s " % (defines["PACKAGE_NAME"], defines["PACKAGE_VERSION"]))
	print("-----------------")
	print("Install path........: %s" % env.subst("$prefix"))
	print("Binary..............: %s" % env.subst("$bindir"))
	print("Configuration.......: %s" % defines["CONFIG_DIR"])
	print("Sublets.............: %s" % defines["SUBLET_DIR"])
	print
	print("Debugging messages..: %s" % debug)
	print

env.ParseConfig("pkg-config --cflags --libs lua")
env.ParseConfig("pkg-config --cflags --libs x11")

# SConscripts
SConscript("data/SConscript", "env", duplicate = 0)
SConscript("sublets/SConscript", "env", duplicate = 0)
SConscript("src/SConscript", "env", build_dir = "build", duplicate = 0)
