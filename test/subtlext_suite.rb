# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("subtlext")
require("test/unit")

class Test::Unit::TestCase
  def setup # {{{
    # Test: Connection
    @subtle = Subtle.new
    assert_not_nil(@subtle)
    assert_instance_of(Subtle, @subtle)
  end # }}}
end

require("test/unit/subtle_test")
require("test/unit/client_test")
require("test/unit/fixnum_test")
require("test/unit/gravity_test")
require("test/unit/tag_test")
require("test/unit/view_test")
