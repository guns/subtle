#
# @package test
#
# @file Test Subtlext::Screen functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Screen" do
  setup { Subtlext::Screen.current }

  asserts("Check attributes") do
    0 == topic.id and "0x16+1024+752" == topic.geometry.to_str
  end

  asserts("Get list") do
    list = Subtlext::Screen.all

    list.is_a?(Array) and 1 == list.size
  end

  asserts("Find and compare") do
    topic == Subtlext::Screen[0]
  end

  asserts("Convert to string")  { "0x16+1024+752" == topic.to_str }
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
