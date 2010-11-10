#
# @package test
#
# @file Test Subtlext::Icon functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Icon" do
  setup { Subtlext::Icon.new(8, 8) }

  asserts("Init types and compare") do
    icon = Subtlext::Icon.new("icon/clock.xbm")

    topic.height == icon.height and topic.width == icon.width
  end

  asserts("Check attributes") do
    8 == topic.height and 8 == topic.width
  end

  asserts("Draw routines") do
    topic.draw(1, 1)
    topic.draw_rect(1, 1, 6, 6, false)
    topic.clear

    true
  end

  asserts("Convert to string") { topic.to_str.match(/<>![0-9]+<>/) }
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
