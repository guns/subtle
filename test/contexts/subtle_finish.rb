#
# @package test
#
# @file Test Subtlext::Subtle functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Subtle - Finish' do
  asserts 'Check reload' do # {{{
    Subtlext::Subtle.reload
    Subtlext::Subtle.running?
  end # }}}

  asserts 'Check restart' do # {{{
    Subtlext::Subtle.reload
    Subtlext::Subtle.running?
  end # }}}

  asserts 'Check quit' do # {{{
    # Kill all clients
    Subtlext::Client.all.each do |c|
      c.kill
    end

    sleep 0.5

    Subtlext::Subtle.quit

    sleep 1

    !Subtlext::Subtle.running?
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker
