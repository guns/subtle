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
    1 == topic.id and "xterm" == topic.name and
      "xterm" == topic.instance and "XTerm" == topic.klass
  end

  asserts("Get list") do
    list = Subtlext::Client.all

    list.is_a?(Array) and 1 == list.size
  end

  asserts("Find and compare") do
    topic == Subtlext::Tag["terms"]
  end

  asserts("Convert to string") { "terms" == topic.to_str }

  asserts("Runtime: Kill a client") do
    topic.kill

    sleep 1

    0 == Subtlext::Client.all.size
  end
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
