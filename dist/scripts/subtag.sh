#!/bin/bash
#
# subtag - dmenu based tagging script
# Copyright (c) 2005-2009 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

# Customization
FONTNAME="fixed-10"
FONTCOLOR="#ffffff"
FORGROUND="#ffffff"
BACKGROUND="#5d5d5d"
HIGHLIGHT="#0dd2e5"

# Path and globals
SUBTLER="subtler"
DMENU="dmenu -fn $FONTNAME -nb $BACKGROUND -nf $FORGROUND -sb $HIGHLIGHT -sf $FONTCOLOR"
GROUP="-c"
ACTION="-T"
CURRENT=0
FIELD=3

# Get options
while getopts ":cvtuCh" OPTION; do
	case $OPTION in
		"c") GROUP="-c"		;;
		"v") GROUP="-v"		;;
		"t") ACTION="-T"	;;
		"u") ACTION="-U"	;;
    "C") CURRENT=1    ;;
	
		"?")
			echo "Unknown option -$OPTARG"
			exit -1
			;;
		"h")
			cat << EOF
Usage: `basename "$0"` [OPTIONS]

  -c  Tag clients
  -v  Tag views
  -C  Choose current active client or view
  -h  Show this help and exit

EOF
			exit
			;;
		esac
done

# Listing format differs
if [ $GROUP == "-c" ] ; then
  FIELD="6 | tr -d '()'" #< Remove brackets from WM_CLASS
fi

# Fetch client/view name
if [ $CURRENT -eq 0 ] ; then
  NAME="`$SUBTLER $GROUP -l | cut -d ' ' -f $FIELD | $DMENU -p "Select name: "`"
else
  NAME="`$SUBTLER $GROUP -C`"
fi

# Select tag
TAG="`$SUBTLER -t -l | $DMENU -p "Select tag for $NAME: "`"

# Assemble commandline
$SUBTLER $GROUP $NAME $ACTION $TAG
