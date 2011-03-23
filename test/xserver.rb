#!/usr/bin/ruby
#
# @package test
#
# @file Start Xvfb server
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

require "mkmf"

# Find Xvfb
if((xvfb = find_executable0("Xvfb")).nil?)
  raise "Xvfb not found in path"
end

system("#{xvfb} :10 -screen 0 1024x768x16 -I &> /dev/null")

# vim:ts=2:bs=2:sw=2:et:fdm=marker
