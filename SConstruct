# SConscript file
package = 'subtle'
version = '0.7'
report	= 'unexist@hilflos.org'

# Options
opts = Options('config.py')
opts.AddOptions(
	('prefix', 'prefix', '/usr/local'),
	('bindir', 'binary directory', '${prefix}/bin'),
	('datadir', 'data directory', '${prefix}/share'),
	('sysconfdir', 'config directory', '${prefix}/etc'),
	PackageOption('with_debug', 'enable debugging messages', 'no'))

BuildDir('build', 'src', duplicate = 0)

# Environment
env	= Environment(
	options = opts,
	CPPPATH = '.', 
	CCFLAGS = ' -Wall -W -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99',
	CPPDEFINES = {
		'PACKAGE_NAME' : '"' + package + '"',
		'PACKAGE_VERSION' : '"' + version + '"',
		'PACKAGE_STRING' : '"' + package + '-' + version + '"',
		'PACKAGE_BUGREPORT' : '"' + report + '"'
	}
)

debug = 'no'
if env['with_debug']:
	env.Append(CCFLAGS = ' -g -DDEBUG')
	debug = 'yes'
else
	env.Append(CCFLAGS = ' -DNDEBUG')

# Help
env.Help(opts.GenerateHelpText(env))

# Checks
def checkCHeaders(conf, headers):
	for h in headers:
		if not conf.CheckCHeader(h):
			print "You need '" + h + "' to compile this program"
			Exit(1)

def checkFunctions(conf, funcs):
	for f in funcs:
		if not conf.CheckFunc(f):
			print "You need '" + f + "()' to compile this program"

if not os.path.exists("config.log"):
	conf = Configure(env)

	checkCHeaders(conf, Split("""
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

	if conf.CheckCHeader('sys/inotify.h'):
		env.Append(CCFLAGS = ' -DHAVE_SYS_INOTIFY_H')

	conf.CheckLibWithHeader('X11', 'X11/X.h', 'C')
	conf.CheckLibWithHeader('lua', 'lua.h', 'C')
	conf.CheckLibWithHeader('m', 'math.h', 'C')
	conf.CheckLibWithHeader('dl', 'dlfcn.h', 'C')

	checkFunctions(conf, Split("""
		select
		strdup
		strcpy
		strerror
	"""))

	env = conf.Finish()

print
print package + ' ' + version
print '-----------------'
print 'Install path........: ' + env['prefix']
print 'Binary..............: ' + env['prefix'] + '/bin'
print 'Configuration.......: ' + env['sysconfdir']
print 'Sublets.............: ' + env['datadir']
print
print 'Debugging messages..: ' + debug
print

# Conscripts
SConscript('src/SConscript', 'env', build_dir = 'build', duplicate = 0)
