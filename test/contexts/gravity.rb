#
# @package test
#
# @file Test Subtlext::Gravity functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Gravity" do
  setup { Subtlext::Gravity["center"] }

  asserts("Check attributes") do
    12 == topic.id and "center" == topic.name and
      "0x0+100+100" == topic.geometry.to_s
  end

  asserts("Get list") do
    list = Subtlext::Gravity.all

    list.is_a?(Array) and 30 == list.size
  end

  asserts("Finder") do
    "center" == Subtlext::Gravity["center"].name
  end

  asserts("Equal and compare") do
    topic.eql? Subtlext::Gravity["center"] and topic == topic
  end

  asserts("Check associations") do
    clients = topic.clients

    clients.is_a?(Array) and 1 == clients.size
  end

  asserts("Convert to string") { "center" == topic.to_str }

  asserts("Runtime: Create new gravity") do
    g = Subtlext::Gravity.new("test")
    g.geometry = Subtlext::Geometry.new(0, 0, 100, 100)
    g.save

    sleep 0.5

    31 == Subtlext::Gravity.all.size
  end

  asserts("Runtime: Kill a gravity") do
    Subtlext::Gravity["test"].kill

    sleep 0.5

    30 == Subtlext::Gravity.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
