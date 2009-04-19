# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestGravity < Test::Unit::TestCase
  def test_gravity # {{{
    # Test: Gravity
    @gravities = [ 
      "UNKNOWN", "BOTTOM_LEFT", "BOTTOM", "BOTTOM_RIGHT", "LEFT", 
      "CENTER", "RIGHT", "TOP_LEFT", "TOP", "TOP_RIGHT"
    ]

    0.times do |i|
      assert(Subtlext::Gravity.get_const(gravities[i]) == i)
    end
  end # }}}
end
