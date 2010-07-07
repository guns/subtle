#
# gtk - subtlext test script with Gtk+
# Copyright (c) 2005-2010 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

require("gtk2")
require("subtle/subtlext")

class Remote < Gtk::Window
  attr_accessor :clients, :sublets, :tags, :views, :table, :action

  def initialize # {{{
    super

    raise "subtle not running" unless(Subtlext::Subtle.running?)

    # Options
    self.title        = "subtle #{Subtlext::VERSION}"
    self.resizable    = false
    self.keep_above   = true
    self.border_width = 10

    set_wmclass("remote", "subtle")
    stick

    # Signals
    signal_connect("delete_event") do
      false
    end

    signal_connect("destroy") do
      Gtk.main_quit
    end

    # List stores
    @clients = Gtk::ListStore.new(String)
    @sublets = Gtk::ListStore.new(String)
    @tags    = Gtk::ListStore.new(String)
    @views   = Gtk::ListStore.new(String)

    # Create vbox
    vbox = Gtk::VBox.new()
    add(vbox)

    # Create widgets
    @table = Gtk::Table.new(4, 13)
    vbox.pack_start(@table)

    create_clients
    create_sublets
    create_tags
    create_views

    # Update list stores
    update_clients
    update_views
    update_tags
    update_sublets

    # Footer
    hbox = Gtk::HBox.new()
    vbox.pack_start(hbox)

    link = Gtk::LinkButton.new("subtle (#{Subtlext::VERSION})")
    link.uri = "http://subtle.subforge.org"
    hbox.pack_start(link)

    link = Gtk::LinkButton.new("Gtk+ (#{Gtk::MAJOR_VERSION}.#{Gtk::MINOR_VERSION}.#{Gtk::MICRO_VERSION})")
    link.uri = "http://ruby-gnome2.sourceforge.jp"
    hbox.pack_start(link)

    @action = Gtk::Label.new("subtle (#{Subtlext::VERSION})", true)
    vbox.pack_start(@action)

    show_all
  end # }}}

  private

  def label(markup) # {{{
    label = Gtk::Label.new("", true)
    label.set_markup(markup)
    label.set_alignment(0, 0)
    label.set_padding(0, 3)

    label
  end # }}}

  def button(caption, &blk) # {{{
    button = Gtk::Button.new(caption)
    button.signal_connect("clicked") do
      begin
        blk.call
      rescue
        message($!)
      end
    end

    button
  end # }}}

  def create_clients # {{{
    # Row1
    @table.attach_defaults(label("<b>Clients</b>"), 0, 4, 0, 1)

    # Row2
    combo_c1name = Gtk::ComboBox.new
    combo_c1name.model = @clients
    @table.attach_defaults(combo_c1name, 0, 2, 1, 2)

    @table.attach_defaults(
      button("tags") do
        name = combo_c1name.active_text

        if(!name.nil?)
          c = Subtlext::Client[name]
          @action.set_markup("<b>Tags of client #{name}: " + c.tags.join(", ") + "</b>")
        end
      end, 2, 3, 1, 2
    )

    @table.attach_defaults(
      button("delete") do
        name = combo_c1name.active_text

        if(!name.nil?)
          Subtlext::Client[name].kill
          @action.set_markup("<b>Deleted client #{name}</b>")
          update_clients()
        end
      end, 3, 4, 1, 2
    )

    # Row3
    combo_c2name = Gtk::ComboBox.new()
    combo_c2name.model = @clients
    table.attach_defaults(combo_c2name, 0, 1, 2, 3)

    combo_c2tag = Gtk::ComboBox.new()
    combo_c2tag.model = @tags
    table.attach_defaults(combo_c2tag, 1, 2, 2, 3)

    @table.attach_defaults(
      button("tag") do
        name = combo_c2name.active_text
        tag  = combo_c2tag.active_text

        if(!name.nil? && !tag.nil?)
          Subtlext::Client[name].tag(tag)
          @action.set_markup("<b>Added tag #{tag} to client #{name}</b>")
        end
      end, 2, 3, 2, 3
    )

    @table.attach_defaults(
      button("untag") do
        name = combo_c2name.active_text
        tag  = combo_c2tag.active_text

        if(!name.nil? && !tag.nil?)
          Subtlext::Client[name].untag(tag)
          @action.set_markup("<b>Deleted tag #{tag} from client #{name}</b>")
        end
      end, 3, 4, 2, 3
    )

    # Row4
    combo_c3name = Gtk::ComboBox.new()
    combo_c3name.model = @clients
    @table.attach_defaults(combo_c3name, 0, 1, 3, 4)

    combo_c3action = Gtk::ComboBox.new()
    [ "full", "float", "stick" ].each do |name|
      combo_c3action.append_text(name)
    end
    combo_c3action.active = 0
    @table.attach_defaults(combo_c3action, 1, 2, 3, 4)

    @table.attach_defaults(
      button("toggle") do
        name   = combo_c3name.active_text
        action = combo_c3action.active_text

        if(!name.nil? && !action.nil?)
          Subtlext::Client[name].send("toggle_" + action)
          @action.set_markup("<b>Toggled #{action} of client #{name}</b>")
        end
      end, 2, 3, 3, 4
    )

    @table.attach_defaults(
      button("focus") do
        name = combo_c3name.active_text

        if(!name.nil?)
          Subtlext::Client[name].focus
          @action.set_markup("<b>Set focus to client #{name}</b>")
        end
      end, 3, 4, 3, 4
    )
  end # }}}

  def create_sublets # {{{
    # Row1
    @table.attach_defaults(label("<b>Sublets</b>"), 0, 4, 4, 5)

    # Row2
    combo_s1name = Gtk::ComboBox.new()
    combo_s1name.model = @sublets
    @table.attach_defaults(combo_s1name, 0, 2, 5, 6)

    @table.attach_defaults(
      button("delete") do
        name = combo_s1name.text

        if(!name.nil?)
          Subtlext::Sublet[name].kill
          @action.set_markup("<b>Deleted sublet #{name}</b>")
          update_sublets()
        end
      end, 2, 4, 5, 6
    )
  end # }}}

  def create_tags # {{{
    @table.attach_defaults(label("<b>Tags</b>"), 0, 4, 6, 7)

    # Row2
    entry_t1name = Gtk::Entry.new()
    table.attach_defaults(entry_t1name, 0, 2, 7, 8)

    @table.attach_defaults(
      button("create") do
        name = entry_t1name.text

        if(!name.nil?)
          Subtlext::Subtle.add_tag(name)
          @action.set_markup("<b>Added tag #{name}</b>")
          update_tags()
        end
      end, 2, 4, 7, 8
    )

    # Row3
    combo_t2name = Gtk::ComboBox.new()
    combo_t2name.model = @tags
    @table.attach_defaults(combo_t2name, 0, 2, 8, 9)

    @table.attach_defaults(
      button("create") do
        name = combo_t2name.active_text

        if(!name.nil?)
          Subtlext::Subtle.del_tag(name)
          @action.set_markup("<b>Deleted tag #{name}</b>")
          update_tags()
        end
      end, 2, 4, 8, 9
    )
  end # }}}

  def create_views # {{{
    # Row1
    @table.attach_defaults(label("<b>Views</b>"), 0, 4, 9, 10)

    # Row2
    entry_v1name = Gtk::Entry.new
    @table.attach_defaults(entry_v1name, 0, 2, 10, 11)

    @table.attach_defaults(
      button("create") do
        name = entry_v1name.text

        if(!name.nil?)
          Subtlext::Subtle.add_view(name)
          update_views()
          @action.set_markup("<b>Added view #{name}</b>")
          update_views()
        end
      end, 2, 4, 10, 11
    )

    # Row3
    combo_v2name = Gtk::ComboBox.new
    combo_v2name.model = @views
    table.attach_defaults(combo_v2name, 0, 1, 11, 12)

    @table.attach_defaults(
      button("jump") do
        name = combo_v2name.active_text

        if(!name.nil?)
          Subtlext::View[name].jump
          @action.set_markup("<b>Jumped to view #{name}</b>")
        end
      end, 1, 2, 11, 12
    )

    @table.attach_defaults(
      button("tags") do
        name = combo_v2name.active_text

        if(!name.nil?)
          v = Subtlext::View[name]
          @action.set_markup("<b>Tags of view #{name}: " + v.tags.join(", ") + "</b>")
        end
      end, 2, 3, 11, 12
    )

    @table.attach_defaults(
      button("delete") do
        name = combo_v2name.active_text

        if(!name.nil?)
          Subtlext::View[name].kill
          @action.set_markup("<b>Deleted view #{name}</b>")
          update_views()
        end
      end, 3, 4, 11, 12
    )

    # Row4
    combo_v3name = Gtk::ComboBox.new()
    combo_v3name.model = @views
    @table.attach_defaults(combo_v3name, 0, 1, 12, 13)

    combo_v3tag = Gtk::ComboBox.new()
    combo_v3tag.model = @tags
    @table.attach_defaults(combo_v3tag, 1, 2, 12, 13)

    @table.attach_defaults(
      button("tag") do
        name = combo_v3name.active_text
        tag  = combo_v3tag.active_text

        if(!name.nil? && !tag.nil?)
          Subtlext::View[name].tag(tag)
          @action.set_markup("<b>Added tag #{tag} to view #{name}</b>")
        end
      end, 2, 3, 12, 13
    )

    @table.attach_defaults(
      button("untag") do
        name = combo_v3name.active_text
        tag  = combo_v3tag.active_text

        if(!name.nil? && !tag.nil?)
          Subtlext::View[name].untag(tag)
          @action.set_markup("<b>Deleted tag #{tag} from view #{name}</b>")
        end
      end, 3, 4, 12, 13
    )
  end # }}}

  def update_clients # {{{
    begin
      @clients.clear
      Subtlext::Client[:all].each do |c|
        @clients.append.set_value(0, c.instance)
      end
    rescue
    end
  end # }}}

  def update_sublets # {{{
    begin
      @sublets.clear
      Subtlext::Sublet[:all].each do |s|
        @sublets.append.set_value(0, s.name)
      end
    rescue
    end
  end # }}}

  def update_tags # {{{
    begin
      @tags.clear
      Subtlext::Tag[:all].each do |t|
        @tags.append.set_value(0, t.name)
      end
    rescue
    end
  end # }}}

  def update_views # {{{
    begin
      @views.clear
      Subtlext::View[:all].each do |v|
        @views.append.set_value(0, v.name)
      end
    rescue
    end
  end # }}}

  def message(text) # {{{
    dialog = Gtk::MessageDialog.new(self,
      Gtk::Dialog::DESTROY_WITH_PARENT,
      Gtk::MessageDialog::ERROR,
      Gtk::MessageDialog::BUTTONS_CLOSE,
      text
    )
    dialog.run
    dialog.destroy
  end # }}}
end

# Init
Gtk.init
window = Remote.new
Gtk.main

# vim:ts=2:bs=2:sw=2:et:fdm=marker
