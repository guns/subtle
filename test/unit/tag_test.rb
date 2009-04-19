# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestTag < Test::Unit::TestCase
  def test_tag # {{{
    # Test: Instance
    @tag = Subtlext::Tag.new("test")
    assert_not_nil(@tag)
    assert_instance_of(Subtlext::Tag, @tag)

    @tag.save

    # Test: Properties
    @id = @tag.id
    assert_not_nil(@id)
    assert_instance_of(Fixnum, @id)

    @name = @tag.name
    assert_not_nil(@name)
    assert_instance_of(String, @name)

    # Test: Info
    @string = @tag.to_s
    assert_not_nil(@string)
    assert_instance_of(String, @string)

    # Tidy up
    @subtle.del_tag(@tag)
  end # }}}
end
