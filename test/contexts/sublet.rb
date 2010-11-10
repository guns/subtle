#
# @package test
#
# @file Test Subtlext::Sublet functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Sublet" do
  setup { Subtlext::Sublet[0] }

  asserts("Check attributes") do
    0 == topic.id and "dummy" == topic.name
  end

  asserts("Get list") do
    list = Subtlext::Sublet.all

    list.is_a?(Array) and 1 == list.size
  end

  asserts("Find and compare") do
    topic == Subtlext::Sublet[0]
  end

  asserts("Set colors") do
    topic.foreground = "#ffffff"
    topic.background = "#ffffff"

    topic.foreground == topic.background
  end

  asserts("Convert to string") { "dummy" == topic.to_str }
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
