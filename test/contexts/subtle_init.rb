#
# @package test
#
# @file Test Subtlext::Subtle functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Subtle - Init" do
  asserts("Check running") do
    Subtlext::Subtle.running?
  end

  asserts("Check colors") do
    Subtlext::Subtle.colors.is_a?(Hash) and 21 == Subtlext::Subtle.colors.size
  end

  asserts("Check font") do
    "-*-*-medium-*-*-*-14-*-*-*-*-*-*-*" == Subtlext::Subtle.font
  end

  asserts("Check spawn") do
    if((xterm = find_executable0("xterm")).nil?)
      raise "xterm not found in path"
    else
      Subtlext::Subtle.spawn("#{xterm} -display :10")
    end

    sleep 1

    1 == Subtlext::Client.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
