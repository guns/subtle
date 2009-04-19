# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

@@sublets = []

class Sublet
  attr_accessor(:interval, :path, :data)

  def self.inherited(recv) # {{{
    @@sublets.push(recv)
  end # }}}
end

class TestSublet < Test::Unit::TestCase
  def test_sublet # {{{
    assert(!@@sublets.empty?, "Failed to find sublet")

    @@sublets.each do |klass|
      assert_block("Failed to run sublet") do
        sublet = klass.new
        assert_instance_of(klass, sublet)

        data = sublet.run
        assert_not_nil(data)
        assert((data.class == Fixnum or data.class == String), "Data must be of type Fixnum or String")

        true
      end
    end # }}}
  end
end
