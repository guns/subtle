# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestFixnum < Test::Unit::TestCase
  def test_fixnum # {{{
    # Test: to_grav
    @grav = 0.to_grav
    assert_not_nil(@grav)
    assert_instance_of(String, @grav)
  end # }}}
end
