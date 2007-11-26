# SConscript file
import os.path

BuildDir("build", "src", duplicate = 0)

# Options
opts = Options()
opts.AddOptions(
	("prefix", "prefix", "/usr/local"),
	("bindir", "binary directory", "${prefix}/bin"),
	("datadir", "data directory", "${prefix}/share"),
	("sysconfdir", "config directory", "${prefix}/etc"),
	PackageOption("debug", "enable debugging messages", "no"))

# Environment
env	= Environment(
	options = opts,
	CPPPATH = "..", 
	CCFLAGS = " -Wall -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
	CCCOMSTR = "CC $TARGET",
	LINKCOMSTR = "LD $TARGET",
	INSTALLSTR = "INSTALL $SOURCE"
)

if env.GetOption("clean") and os.path.isfile("config.h"):
	os.remove("config.h")

# Generate help
env.Help(opts.GenerateHelpText(env))

defines = {
	"PACKAGE_NAME" : "subtle",
	"PACKAGE_VERSION" : "0.7",
	"PACKAGE_BUGREPORT" : "unexist@dorfelite.net"
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
	print("Binary..............: %s" % env.subst("$prefix" + "/bin"))
	print("Configuration.......: %s" % defines["CONFIG_DIR"])
	print("Sublets.............: %s" % defines["SUBLET_DIR"])
	print
	print("Debugging messages..: %s" % debug)
	print

env.ParseConfig("pkg-config --cflags --libs lua")
env.ParseConfig("pkg-config --cflags --libs x11")

# SConscripts
SConscript("config/SConscript", "env", duplicate = 0)
SConscript("sublets/SConscript", "env", duplicate = 0)
SConscript("src/SConscript", "env", build_dir = "build", duplicate = 0)
