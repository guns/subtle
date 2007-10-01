#! /bin/bash
set -x
rev=`hg log | head -n1 | sed -n 's/changeset:[ ]*\([0-9]*\):[0-9a-zA-Z]*/\1/p'`
#rev=`svn info | sed -n 's/Revision: \([0-9]*\)/\1/p'`
if test -z "$rev"; then
	$rev="UNKNOWN"
fi

sed -i -e 's/AC_INIT(\(.*\), \([0-9\.]*\)[-r0-9]*, \(.*\))/AC_INIT(\1, \2-r'$rev', \3)/' configure.in

aclocal
autoheader
automake --add-missing --copy
autoconf
