# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestFixnum < Test::Unit::TestCase
  def test_fixnum # {{{
    # Test: to_horz
    @grav = 0.to_horz
    assert_not_nil(@grav)
    assert_instance_of(Fixnum, @grav)

    # Test: to_vert
    @grav = 0.to_vert
    assert_not_nil(@grav)
    assert_instance_of(Fixnum, @grav)

    # Test: to_mode33
    @grav = 0.to_mode33
    assert_not_nil(@grav)
    assert_instance_of(Fixnum, @grav)

    # Test: to_mode66
    @grav = 0.to_mode66
    assert_not_nil(@grav)
    assert_instance_of(Fixnum, @grav)
  end # }}}
end
