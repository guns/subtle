#
# @package test
#
# @file Test Subtlext::Gravity functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context "Gravity" do
  GRAVITY_COUNT = 30

  setup do # {{{
    Subtlext::Gravity["center"]
  end # }}}

  asserts("Check attributes") do # {{{
    12 == topic.id and "center" == topic.name and
      "0x0+100+100" == topic.geometry.to_s
  end # }}}

  asserts("Get list") do # {{{
    list = Subtlext::Gravity.all

    list.is_a?(Array) and GRAVITY_COUNT == list.size
  end # }}}

  asserts("Finder") do # {{{
    ary = Subtlext::Gravity['.*']

    "center" == Subtlext::Gravity["center"].name and ary.is_a? Array
      GRAVITY_COUNT == ary.size
  end # }}}

  asserts("Equal and compare") do # {{{
    topic.eql? Subtlext::Gravity["center"] and topic == topic
  end # }}}

  asserts("Check associations") do # {{{
    clients = topic.clients

    clients.is_a?(Array) and 1 == clients.size
  end # }}}

  asserts("Convert to string") do # {{{
    "center" == topic.to_str
  end # }}}

  asserts("Runtime: Create new gravity") do # {{{
    g = Subtlext::Gravity.new("test")
    g.geometry = Subtlext::Geometry.new(0, 0, 100, 100)
    g.save

    sleep 0.5

    31 == Subtlext::Gravity.all.size
  end # }}}

  asserts("Runtime: Kill a gravity") do # {{{
    Subtlext::Gravity["test"].kill

    sleep 0.5

    30 == Subtlext::Gravity.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
