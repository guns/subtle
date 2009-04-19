# 
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
# 
# $Id$
# 

require("test/unit")

class TestSubtle < Test::Unit::TestCase
  def test_subtle # {{{
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

      @client = @subtle.current_client
      assert_not_nil(@client)
      assert_instance_of(Subtlext::Client, @client)
    else
      puts "Clients tests can only be run if one or more 'xterm' is running."
    end

    # Test: Tags
    @tags = @subtle.tags
    assert_not_nil(@tags)
    assert_instance_of(Array, @tags)

    @tag1 = @subtle.add_tag("test")
    assert_not_nil(@tag1)
    assert_instance_of(Subtlext::Tag, @tag1)

    @tag2 = @subtle.find_tag("test")
    assert_not_nil(@tag2)
    assert_instance_of(Subtlext::Tag, @tag2)

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
    assert_instance_of(Subtlext::View, @cv)

    @view1 = @subtle.add_view("test")
    assert_not_nil(@view1)
    assert_instance_of(Subtlext::View, @view1)

    @view2 = @subtle.find_view("test")
    assert_not_nil(@view2)
    assert_instance_of(Subtlext::View, @view2)

    @subtle.del_view("test")    

    @subtle.reload  
  end # }}}
end
