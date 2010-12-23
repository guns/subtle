#
# @package test
#
# @file Test Subtlext::Color functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

context "Color" do
  setup { Subtlext::Color.new("#ff0000") }

  asserts("Check attributes") do
    65535 == topic.red and 0 == topic.green and 0 == topic.blue
  end

  asserts("Type conversions") do
    hex = topic.to_hex

    true
  end

  asserts("Equal and compare") do
    topic.eql? Subtlext::Color.new("#ff0000") and topic == topic
  end

  asserts("Convert to string") { topic.to_str.match(/<>#[0-9]+<>/) }
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
