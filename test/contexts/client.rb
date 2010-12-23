#
# @package test
#
# @file Test Subtlext::Client functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Client" do
  setup { Subtlext::Client.current }

  asserts("Check attributes") do
    0 == topic.id and "xterm" == topic.name and
      "xterm" == topic.instance and "XTerm" == topic.klass
  end

  asserts("Get list") do
    list = Subtlext::Client.all

    list.is_a?(Array) and 1 == list.size
  end

  asserts("Finder") do
    "xterm" == Subtlext::Client[0].name
  end

  asserts("Equal and compare") do
    topic.eql? Subtlext::Client.current and topic == topic
  end

  asserts("Convert to string") { "xterm" == topic.to_str }

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

    before == middle1 - 1 and 2 == middle2 and before == after
  end

  asserts("Runtime: Kill a client") do
    topic.kill

    sleep 1

    0 == Subtlext::Client.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
