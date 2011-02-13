#
# @package test
#
# @file Test Subtlext::Screen functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Screen" do
  setup { Subtlext::Screen.current }

  asserts("Check attributes") do
    0 == topic.id and "0x16+1024+752" == topic.geometry.to_str
  end

  asserts("Get list") do
    list = Subtlext::Screen.all

    list.is_a?(Array) and 1 == list.size
  end

  asserts("Find and compare") do
    topic == Subtlext::Screen[0]
  end

  asserts("Finder") do
    Subtlext::Screen[0] == Subtlext::Screen.find(
      Subtlext::Geometry.new(100, 100, 100, 100)
    )
  end

  asserts("Equal and compare") do
    topic.eql? Subtlext::Screen.current and topic == topic
  end

  asserts("Runtime: Change view") do
    view1 = topic.view

    sleep 0.5

    topic.view = "www"

    sleep 0.5

    view2 = topic.view
    topic.view = view1

    sleep 0.5

    view3 = topic.view

    view1 == view3 and "www" == view2.name
  end

  asserts("Convert to string")  { "0x16+1024+752" == topic.to_str }
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
