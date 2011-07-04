#
# @package test
#
# @file Test Subtlext::Sublet functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context "Sublet" do
  SUBLET_COUNT = 1

  setup do # {{{
    Subtlext::Sublet[0]
  end # }}}

  asserts("Check attributes") do # {{{
    0 == topic.id and "dummy" == topic.name
  end # }}}

  asserts("Get list") do # {{{
    list = Subtlext::Sublet.all

    list.is_a?(Array) and SUBLET_COUNT == list.size
  end # }}}

  asserts("Finder") do # {{{
    ary = Subtlext::Sublet['.*']

    "dummy" == Subtlext::Sublet[0].name and ary.is_a? Array and
      SUBLET_COUNT == ary.size
  end # }}}

  asserts("Update sublet") do # {{{
    nil == topic.update
  end # }}}

  asserts("Equal and compare") do # {{{
    topic.eql? topic and topic == topic
  end # }}}

  asserts("Set colors") do # {{{
    topic.foreground = "#ffffff"
    topic.background = "#ffffff"

    true
  end # }}}

  asserts("Convert to string") do # {{{
    "dummy" == topic.to_str
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
