#!/bin/bash
#
# subtle - window manager
# Copyright (c) 2005-2008 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

SUBTLER="subtler -d :2"
DMENU="dmenu -y -0 -nb #9AA38A -nf #ffffff -sb #A7E737 -sf #ffffff"
GROUP="-c"
ACTION="-T"

while getopts ":cvtuh" OPTION; do
	case $OPTION in
		"c") GROUP="-c"		;;
		"v") GROUP="-v"		;;
		"t") ACTION="-T"	;;
		"u") ACTION="-u"	;;
	
		"?")
			echo "Unknown option -$OPTARG."
			exit -1
			;;
		"h")
			cat << EOF
Usage: `basename "$0"` [OPTIONS]

  -c  Show tagmenu for clients
  -v  Show tag menu for views
  -h  Show this help and exit.

EOF
			exit
			;;
		esac
done

# Listing format differs
FIELD=3
[ $GROUP == "-c" ] && FIELD=5

NAME="`$SUBTLER $GROUP -l | cut -d ' ' -f $FIELD | $DMENU -p "Name: "`"

if [ $ACTION == "-T" ] ; then
	TAG="`$SUBTLER -t -l | $DMENU -p "Tags: "`"
else
	TAG="`$SUBTLER $GROUP $NAME -g | $DMENU -p "Tags: "`"
fi

# Assemble commandline
echo $SUBTLER $GROUP $NAME $ACTION $TAG
