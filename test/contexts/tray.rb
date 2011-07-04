#
# @package test
#
# @file Test Subtlext::Tray functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context "Tray" do
  TRAY_COUNT = 1

  setup do # {{{
    Subtlext::Tray[0]
  end # }}}

  asserts("Check attributes") do # {{{
    0 == topic.id and "subtle" == topic.name and
      "subtle" == topic.instance and "subtle" == topic.klass
  end # }}}

  asserts("Get list") do # {{{
    list = Subtlext::Tray.all

    list.is_a?(Array) and TRAY_COUNT == list.size
  end # }}}

  asserts("Finder") do # {{{
    ary = Subtlext::Tray['.*']

    "subtle" == Subtlext::Tray[0].name and ary.is_a? Array and
      TRAY_COUNT == ary.size
  end # }}}

  asserts("Equal and compare") do # {{{
    topic.eql? Subtlext::Tray[0] and topic == topic
  end # }}}

  asserts("Convert to string") do # {{{
    "subtle" == topic.to_str
  end # }}}

  asserts("Runtime: click") do # {{{
    nil == topic.click
  end # }}}

  asserts("Runtime: Kill a tray") do # {{{
    topic.kill

    sleep 1

    0 == Subtlext::Tray.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
