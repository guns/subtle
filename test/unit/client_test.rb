# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestClient < Test::Unit::TestCase
  def test_client # {{{
    # Test: Instance
    @client = @subtle.find_client("xterm")

    if(@client.nil?)
      flunk("Clients tests can only be run if one or more 'xterm' is running.")
    end

    assert_not_nil(@client)
    assert_instance_of(Client, @client)

    # Test: Properties
    @id = @client.id
    assert_not_nil(@id)
    assert_instance_of(Fixnum, @id)

    @win = @client.win
    assert_not_nil(@id)
    assert_instance_of(Fixnum, @win)

    @name = @client.name
    assert_not_nil(@name)
    assert_instance_of(String, @name)

    @gravity = @client.gravity
    assert_not_nil(@gravity)
    assert_instance_of(String, @gravity)

    @client.gravity = Gravity::CENTER

    # Test: Focus
    @focus = @client.focus?
    assert_not_nil(@focus)
    assert((@focus.class == TrueClass or @focus.class == FalseClass))

    @client.focus

    # Test: Toggle
    @toggles = [ "toggle_full", "toggle_float", "toggle_stick" ]

    (@toggles + @toggles).each do |t|
      @client.send(t)
    end

    # Test: Info
    @string = @client.to_s
    assert_not_nil(@string)
    assert_instance_of(String, @string)

    # Test: Tagging
    @tag = @subtle.add_tag("test")
    assert_not_nil(@tag)
    assert_instance_of(Tag, @tag)

    @client.tag(@tag)
    @client.untag(@tag)

    # Test: Operator
    @client + "test"
    @client - "test"

    @tags = @client.tags
    assert_not_nil(@tags)
    assert_instance_of(Array, @tags)

    # Tidy up
    @subtle.del_tag(@tag)
  end # }}}
end
