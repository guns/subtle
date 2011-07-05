#
# @package test
#
# @file Test Subtlext::Tray functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Tray' do
  TRAY_COUNT = 1
  TRAY_ID    = 0
  TRAY_NAME  = 'test.rb'

  setup do # {{{
    Subtlext::Tray[TRAY_ID]
  end # }}}

  asserts 'Check attributes' do # {{{
    TRAY_ID == topic.id and TRAY_NAME == topic.name and
      TRAY_NAME == topic.instance and TRAY_NAME == topic.klass.downcase
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Tray.all

    list.is_a? Array and TRAY_COUNT == list.size
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Tray[TRAY_ID]
    string = Subtlext::Tray[TRAY_NAME]
    sym    = Subtlext::Tray[TRAY_NAME.to_sym]
    all    = Subtlext::Tray['.*']

    index == string and index == sym and index == all
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql? Subtlext::Tray[TRAY_ID] and topic == topic
  end # }}}

  asserts 'Convert to string' do # {{{
    TRAY_NAME == topic.to_str
  end # }}}

  asserts 'Send button' do # {{{
    nil == topic.send_button(1)
  end # }}}

  asserts 'Send key' do # {{{
    nil == topic.send_key('a')
  end # }}}

  asserts 'Kill a tray' do # {{{
    topic.kill

    sleep 1

    0 == Subtlext::Tray.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
