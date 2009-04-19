# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestView < Test::Unit::TestCase
  def test_view # {{{
    # Test: Instance
    @view = Subtlext::View.new("test")
    assert_not_nil(@view)
    assert_instance_of(Subtlext::View, @view)

    @view.save
    @view.jump

    # Test: Properties
    @id = @view.id
    assert_not_nil(@id)
    assert_instance_of(Fixnum, @id)

    @win = @view.win
    assert_not_nil(@id)
    assert_instance_of(Fixnum, @win)

    @name = @view.name
    assert_not_nil(@name)
    assert_instance_of(String, @name)

    # Test: Info
    @current = @view.current?
    assert_not_nil(@current)
    assert((@current.class == TrueClass or @current.class == FalseClass))

    @string = @view.to_s
    assert_not_nil(@string)
    assert_instance_of(String, @string)

    # Test: Tagging
    @tag = @subtle.add_tag("test")
    assert_not_nil(@tag)
    assert_instance_of(Subtlext::Tag, @tag)

    @view.tag(@tag)
    @view.untag(@tag)

    # Test: Operator
    @view + "test"
    @view - "test"

    @tags = @view.tags
    assert_not_nil(@tags)
    assert_instance_of(Array, @tags)

    # Tidy up
    @subtle.del_view(@view)
    @subtle.del_tag(@tag)
  end # }}}
end
