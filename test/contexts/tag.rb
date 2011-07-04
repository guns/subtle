#
# @package test
#
# @file Test Subtlext::Tag functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context "Tag" do
  TAG_COUNT = 12

  setup do # {{{
    Subtlext::Tag["terms"]
  end # }}}

  asserts("Check attributes") do # {{{
    1 == topic.id and "terms" == topic.name
  end # }}}

  asserts("Get list") do # {{{
    list = Subtlext::Tag.all

    list.is_a?(Array) and TAG_COUNT == list.size
  end # }}}

  asserts("Finder") do # {{{
    ary = Subtlext::Tag['.*']

    "terms" == Subtlext::Tag["terms"].name and ary.is_a? Array and
      TAG_COUNT == ary.size
  end # }}}

  asserts("Equal and compare") do # {{{
    topic.eql? Subtlext::Tag["terms"] and topic == topic
  end # }}}

  asserts("Check associations") do # {{{
    clients = topic.clients
    views   = topic.views

    clients.is_a?(Array) and 1 == clients.size and
      views.is_a?(Array) and 1 == views.size
  end # }}}

  asserts("Convert to string") do # {{{
    "terms" == topic.to_str
  end # }}}

  asserts("Runtime: Create new tag") do # {{{
    t = Subtlext::Tag.new("test")
    t.save

    sleep 1

    13 == Subtlext::Tag.all.size
  end # }}}

  asserts("Runtime: Kill a tag") do # {{{
    Subtlext::Tag["test"].kill

    sleep 1

    12 == Subtlext::Tag.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
