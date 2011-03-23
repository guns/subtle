#
# @package test
#
# @file Test Subtlext::View functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context "View" do
  setup { Subtlext::View.current }

  asserts("Check attributes") do
    0 == topic.id and "terms" == topic.name
  end

  asserts("Get list") do
    list = Subtlext::View.all

    list.is_a?(Array) and 4 == list.size
  end

  asserts("Finder") do
    "terms" == Subtlext::View["terms"].name
  end

  asserts("Equal and compare") do
    topic.eql? Subtlext::View.current and topic == topic
  end

  asserts("Check associations") do
    clients = topic.clients
    tags    = topic.tags

    clients.is_a?(Array) and 1 == clients.size and
      tags.is_a?(Array) and 2 == tags.size
  end

  asserts("Check icon") do
    nil == topic.icon
  end

  asserts("Convert to string") { "terms" == topic.to_str }

  asserts("Runtime: Create new view") do
    v = Subtlext::View.new("test")
    v.save

    sleep 1

    5 == Subtlext::View.all.size
  end

  asserts("Runtime: Switch views") do
    view_next = topic.next
    view_next.jump

    sleep 1

    view_prev = view_next.prev
    topic.jump

    sleep 1

    view_prev == topic and topic.current?
  end

  asserts("Runtime: Add/remove tags") do
    tag = Subtlext::Tag.all.last

    # Compare tag counts
    before = topic.tags.size
    topic.tag(tag)

    sleep 0.5

    middle1 = topic.tags.size
    topic.tags = [ tag, "default" ]

    sleep 0.5

    middle2 = topic.tags.size
    topic.untag(tag)

    sleep 0.5

    after = topic.tags.size

    before == middle1 - 1 and 2 == middle2 and 1 == after
  end

  asserts("Runtime: Store values") do
    topic[:test] = "test"

    "test" == Subtlext::View.current[:test]
  end

  asserts("Runtime: Kill a view") do
    Subtlext::View["test"].kill

    sleep 1

    4 == Subtlext::View.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
