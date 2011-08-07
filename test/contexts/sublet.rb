#
# @package test
#
# @file Test Subtlext::Sublet functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Sublet' do
  SUBLET_COUNT = 1
  SUBLET_ID    = 0
  SUBLET_NAME  = 'dummy'

  setup do # {{{
    Subtlext::Sublet[0]
  end # }}}

  asserts 'Check attributes' do # {{{
    SUBLET_ID == topic.id and SUBLET_NAME == topic.name
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Sublet.all

    list.is_a?(Array) and SUBLET_COUNT == list.size
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Sublet[SUBLET_ID]
    string = Subtlext::Sublet[SUBLET_NAME]
    sym    = Subtlext::Sublet[SUBLET_NAME.to_sym]
    all    = Subtlext::Sublet['.*']

    index == string and index == sym and index == all
  end # }}}

  asserts 'Update sublet' do # {{{
    nil == topic.update
  end # }}}

  asserts 'Get geometry' do # {{{
    topic.geometry.is_a? Subtlext::Geometry
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql? topic and topic == topic
  end # }}}

  asserts 'Convert to string' do # {{{
    SUBLET_NAME == topic.to_str
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
