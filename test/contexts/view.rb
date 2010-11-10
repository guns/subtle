#
# @package test
#
# @file Test Subtlext::View functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
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

  asserts("Find and compare") do
    topic == Subtlext::View["terms"]
  end

  asserts("Check associations") do
    clients = topic.clients
    tags    = topic.tags

    clients.is_a?(Array) and 1 == clients.size and
      views.is_a?(Array) and 2 == tags.size
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

    sleep 1

    middle = topic.tags.size
    topic.untag(tag)

    sleep 1

    after = topic.tags.size

    before == middle - 1 and before == after
  end

  asserts("Runtime: Kill a view") do
    Subtlext::View["test"].kill

    sleep 1

    4 == Subtlext::View.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
