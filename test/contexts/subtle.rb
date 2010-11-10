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

context "Subtle" do
  asserts("Check colors") do
    Subtlext::Subtle.colors.is_a?(Hash) and 21 == Subtlext::Subtle.colors.size
  end

  asserts("Check spawn") do
    if((xterm = find_executable0("xterm")).nil?)
      raise "xterm not found in path"
    else
      Subtlext::Subtle.spawn("#{xterm} -display :10")
    end
  end

  asserts("Check reload") do
    Subtlext::Subtle.reload.nil?
    Subtlext::Subtle.running?
  end

  asserts("Check restart") do
    Subtlext::Subtle.reload
    Subtlext::Subtle.running?
  end

  asserts("Check quit") do
    Subtlext::Subtle.quit

    sleep 1

    !Subtlext::Subtle.running?
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
