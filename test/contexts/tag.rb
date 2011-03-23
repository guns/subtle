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
  setup { Subtlext::Tag["terms"] }

  asserts("Check attributes") do
    1 == topic.id and "terms" == topic.name
  end

  asserts("Get list") do
    list = Subtlext::Tag.all

    list.is_a?(Array) and 12 == list.size
  end

  asserts("Finder") do
    "terms" == Subtlext::Tag["terms"].name
  end

  asserts("Equal and compare") do
    topic.eql? Subtlext::Tag["terms"] and topic == topic
  end

  asserts("Check associations") do
    clients = topic.clients
    views   = topic.views

    clients.is_a?(Array) and 1 == clients.size and
      views.is_a?(Array) and 1 == views.size
  end

  asserts("Convert to string") { "terms" == topic.to_str }

  asserts("Runtime: Create new tag") do
    t = Subtlext::Tag.new("test")
    t.save

    sleep 1

    13 == Subtlext::Tag.all.size
  end

  asserts("Runtime: Kill a tag") do
    Subtlext::Tag["test"].kill

    sleep 1

    12 == Subtlext::Tag.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
