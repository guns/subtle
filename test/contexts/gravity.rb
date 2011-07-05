#
# @package test
#
# @file Test Subtlext::Gravity functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Gravity' do
  GRAVITY_COUNT = 30
  GRAVITY_ID    = 12
  GRAVITY_NAME  = 'center'

  setup do # {{{
    Subtlext::Gravity[GRAVITY_ID]
  end # }}}

  asserts 'Check attributes' do # {{{
    GRAVITY_ID == topic.id and GRAVITY_NAME == topic.name and
      topic.geometry.is_a? Subtlext::Geometry
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Gravity.all

    list.is_a?(Array) and GRAVITY_COUNT == list.size
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Gravity[GRAVITY_ID]
    string = Subtlext::Gravity[GRAVITY_NAME + '$']
    sym    = Subtlext::Gravity[GRAVITY_NAME.to_sym]
    all    = Subtlext::Gravity['.*']

    index == string and index == sym and
      all.is_a? Array and GRAVITY_COUNT == all.size
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql? Subtlext::Gravity[GRAVITY_ID] and topic == topic
  end # }}}

  asserts 'Check associations' do # {{{
    clients = topic.clients

    clients.is_a? Array and 1 == clients.size
  end # }}}

  asserts 'Convert to string' do # {{{
    GRAVITY_NAME == topic.to_str
  end # }}}

  asserts 'Create new gravity' do # {{{
    g = Subtlext::Gravity.new 'test'
    g.geometry = Subtlext::Geometry.new 0, 0, 100, 100
    g.save

    sleep 0.5

    GRAVITY_COUNT + 1 == Subtlext::Gravity.all.size
  end # }}}

  asserts 'Kill a gravity' do # {{{
    Subtlext::Gravity['test'].kill

    sleep 0.5

    GRAVITY_COUNT == Subtlext::Gravity.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
