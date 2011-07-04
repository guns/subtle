#
# @package test
#
# @file Test Subtlext::Icon functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context "Icon" do
  setup do # {{{
    Subtlext::Icon.new("icon/clock.xbm")
  end # }}}

  asserts("Equal and compare") do # {{{
    topic.eql? Subtlext::Icon.new(8, 8) and topic == topic
  end # }}}

  asserts("Check attributes") do # {{{
    8 == topic.height and 8 == topic.width
  end # }}}

  asserts("Draw routines") do # {{{
    topic.draw_point(1, 1)
    topic.draw_rect(1, 1, 6, 6, false)
    topic.clear

    true
  end # }}}

  asserts("Copy area") do # {{{
    icon = Subtlext::Icon.new("icon/clock.xbm")

    topic.copy_area(icon, 0, 0, 4, 4, 2, 2)

    true
  end # }}}

  asserts("Convert to string") do # {{{
    topic.to_str.match(/<>![0-9]+<>/)
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
