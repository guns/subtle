#! /bin/bash
set -x

ver="0.7.1"
rev=`svn info|sed -n 's/Revision: \([0-9]*\)/\1/p'`
if test -z "$rev"; then
	$rev="UNKNOWN"
fi

sed -i -e 's/AC_INIT(subtle, \(.*\))/AC_INIT(subtle, '$ver'-r'$rev')/' configure.in

aclocal
autoheader
automake --add-missing --copy
autoconf
