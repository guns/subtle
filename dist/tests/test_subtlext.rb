# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")
require("subtle/subtlext")

class TestSubtlext < Test::Unit::TestCase
  def test_subtle # {{{
    # Test: Connection
    @subtle = Subtle.new(":2")
    assert_not_nil(@subtle)
    assert_instance_of(Subtle, @subtle)

    # Test: Info
    @version = @subtle.version
    assert_not_nil(@version)
    assert_instance_of(String, @version)

    @display = @subtle.display
    assert_not_nil(@display)
    assert_instance_of(String, @display)

    @running = @subtle.running?
    assert_not_nil(@running)
    assert((@running.class == TrueClass or @running.class == FalseClass))

    @string = @subtle.to_s
    assert_not_nil(@string)
    assert_instance_of(String, @string)

    # Test: Clients
    @client = @subtle.find_client("xterm")

    if(!@client.nil?)
      @clients = @subtle.clients
      assert_not_nil(@clients)
      assert_instance_of(Array, @clients)
    else
      puts "Clients tests can only be run if one or more 'xterm' is running."
    end

    # Test: Tags
    @tags = @subtle.tags
    assert_not_nil(@tags)
    assert_instance_of(Array, @tags)

    @tag1 = @subtle.add_tag("test")
    assert_not_nil(@tag1)
    assert_instance_of(Tag, @tag1)

    @tag2 = @subtle.find_tag("test")
    assert_not_nil(@tag2)
    assert_instance_of(Tag, @tag2)

    @subtle.del_tag("test")

    # Test: Sublets
    @sublets = @subtle.sublets
    assert_not_nil(@sublets)
    assert_instance_of(Array, @sublets)

    # Test: Views
    @views = @subtle.views
    assert_not_nil(@views)
    assert_instance_of(Array, @views)

    @cv = @subtle.current_view
    assert_not_nil(@cv)
    assert_instance_of(View, @cv)

    @view1 = @subtle.add_view("test")
    assert_not_nil(@view1)
    assert_instance_of(View, @view1)

    @view2 = @subtle.find_view("test")
    assert_not_nil(@view2)
    assert_instance_of(View, @view2)

    @subtle.del_view("test")    
  end # }}}

  def test_client # {{{
    # Test: Connection
    @subtle = Subtle.new(":2")
    assert_not_nil(@subtle)
    assert_instance_of(Subtle, @subtle)

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

  def test_fixnum # {{{
    # Test: to_grav
    @grav = 0.to_grav
    assert_not_nil(@grav)
    assert_instance_of(String, @grav)
  end # }}}
  
  def test_gravity # {{{
    # Test: Gravity
    @gravities = [ 
      "UNKNOWN", "BOTTOM_LEFT", "BOTTOM", "BOTTOM_RIGHT", "LEFT", 
      "CENTER", "RIGHT", "TOP_LEFT", "TOP", "TOP_RIGHT"
    ]

    0.times do |i|
      assert(Gravity.get_const(gravities[i]) == i)
    end
  end # }}}

  def test_tag # {{{
    # Test: Connection
    @subtle = Subtle.new(":2")
    assert_not_nil(@subtle)
    assert_instance_of(Subtle, @subtle)

    # Test: Instance
    @tag = Tag.new("test")
    assert_not_nil(@tag)
    assert_instance_of(Tag, @tag)

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

  def test_view # {{{
    # Test: Connection
    @subtle = Subtle.new(":2")
    assert_not_nil(@subtle)
    assert_instance_of(Subtle, @subtle)

    # Test: Instance
    @view = View.new("test")
    assert_not_nil(@view)
    assert_instance_of(View, @view)

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
    assert_instance_of(Tag, @tag)

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
