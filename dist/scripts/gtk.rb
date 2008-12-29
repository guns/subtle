#
# subtle - test script
# Copyright (c) 2005-2008 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

require("gtk2")
require("/home/unexist/projects/subtle/subtlext")

# Variables {{{
$subtle       = nil
@window       = nil
@list_clients = nil
@list_sublets = nil
@list_tags    = nil
@list_views   = nil
# }}}

# Functions {{{
def GtkUpdateClients
  begin
    @list_clients.clear
    $subtle.clients.each do |client|
      @list_clients.append().set_value(0, client.name)
    end
  rescue
  end
end

def GtkUpdateSublets
  begin
    @list_sublets.clear
    $subtle.sublets.each do |sublet|
      @list_sublets.append().set_value(0, sublet.name)
    end
  rescue
  end
end

def GtkUpdateTags
  begin
    @list_tags.clear
    $subtle.tags.each do |tag|
      @list_tags.append().set_value(0, tag.name)
    end
  rescue
  end
end

def GtkUpdateViews
  begin
    @list_views.clear
    $subtle.views.each do |view|
      @list_views.append().set_value(0, view.name)
    end
  rescue
  end
end

def GtkMessage(text)
  dialog = Gtk::MessageDialog.new(@window, 
    Gtk::Dialog::DESTROY_WITH_PARENT,
    Gtk::MessageDialog::ERROR,
    Gtk::MessageDialog::BUTTONS_CLOSE,
    text
  )
  dialog.run()
  dialog.destroy()
end
# }}}

# Main {{{
begin
  $subtle = Subtle.new(":2")

  @list_clients = Gtk::ListStore.new(String)
  @list_sublets = Gtk::ListStore.new(String)
  @list_tags    = Gtk::ListStore.new(String)
  @list_views   = Gtk::ListStore.new(String)
rescue
  GtkMessage($!)
  exit
end
# }}}

# Window {{{
@window = Gtk::Window.new($subtle.to_s)
@window.border_width = 10

@window.signal_connect("delete_event") do
  false
end

@window.signal_connect("destroy") do
  Gtk.main_quit
end

vbox = Gtk::VBox.new()
@window.add(vbox)

link = Gtk::LinkButton.new($subtle.to_s)
link.uri = "http://unexist.scrapping.cc/projects/show/subtle"
vbox.pack_start(link)

link.signal_connect("clicked") do
  GtkUpdateClients()
  GtkUpdateSublets()
  GtkUpdateTags()
  GtkUpdateViews() 
end

label_action = Gtk::Label.new("", true)
# }}}

# Table {{{
table = Gtk::Table.new(4, 13)
vbox.pack_start(table)
# }}}

# Clients {{{
# Row1 {{{
label = Gtk::Label.new("", true)
label.set_markup("<b>Clients</b>")
label.set_alignment(0, 0)
label.set_padding(0, 3)
table.attach_defaults(label, 0, 4, 0, 1)
# }}}

# Row2 {{{
combo_c1name = Gtk::ComboBox.new()
combo_c1name.model = @list_clients
table.attach_defaults(combo_c1name, 0, 2, 1, 2)

button = Gtk::Button.new("tags")
button.signal_connect("clicked") do
  begin
    name = combo_c1name.active_text

    if(!name.nil?)
      c = $subtle.find_client(name)
      label_action.set_markup("<b>Tags of client #{name}: " + c.tags.join(", ") + "</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 3, 1, 2)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") do
  begin
    name = combo_c1name.active_text

    if(!name.nil?)
      $subtle.del_client(name)
      label_action.set_markup("<b>Deleted client #{name}</b>")
      GtkUpdateClients()
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 3, 4, 1, 2)
# }}}

# Row3 {{{
combo_c2name = Gtk::ComboBox.new()
combo_c2name.model = @list_clients
table.attach_defaults(combo_c2name, 0, 1, 2, 3)

combo_c2tag = Gtk::ComboBox.new()
combo_c2tag.model = @list_tags
table.attach_defaults(combo_c2tag, 1, 2, 2, 3)

button = Gtk::Button.new("tag")
button.signal_connect("clicked") do
  begin
    name = combo_c2name.active_text
    tag  = combo_c2tag.active_text

    if(!name.nil? && !tag.nil?)
      $subtle.find_client(name).tag(tag)
      label_action.set_markup("<b>Added tag #{tag} to client #{name}</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 3, 2, 3)

button = Gtk::Button.new("untag")
button.signal_connect("clicked") do
  begin
    name = combo_c2name.active_text
    tag  = combo_c2tag.active_text

    if(!name.nil? && !tag.nil?)
      $subtle.find_client(name).untag(tag)
      label_action.set_markup("<b>Deleted tag #{tag} from client #{name}</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 3, 4, 2, 3)
# }}}

# Row4 {{{
combo_c3name = Gtk::ComboBox.new()
combo_c3name.model = @list_clients
table.attach_defaults(combo_c3name, 0, 1, 3, 4)

combo_c3action = Gtk::ComboBox.new()
["full", "float", "urgent"].each do |name|
  combo_c3action.append_text(name)
end
combo_c3action.active = 0
table.attach_defaults(combo_c3action, 1, 2, 3, 4)

button = Gtk::Button.new("toggle")
button.signal_connect("clicked") do
  begin
    name   = combo_c3name.active_text
    action = combo_c3action.active_text

    if(!name.nil? && !tag.nil?)
      $subtle.find_client(name).send("toggle_" + action)
      label_action.set_markup("<b>Toggled #{action} of client #{name}</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 4, 3, 4)
# }}}
# }}}

# Sublets {{{
# Row1 {{{
label = Gtk::Label.new("", true)
label.set_markup("<b>Sublets</b>")
label.set_alignment(0, 0)
label.set_padding(0, 3)
table.attach_defaults(label, 0, 4, 4, 5)
# }}}

# Row2 {{{
combo_s1name = Gtk::ComboBox.new()
combo_s1name.model = @list_sublets
table.attach_defaults(combo_s1name, 0, 2, 5, 6)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") do
  begin
    name = combo_s1name.text

    if(!name.nil?)
      $subtle.del_sublet(name)
      label_action.set_markup("<b>Deleted sublet #{name}</b>")
      GtkUpdateSublets()
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 4, 5, 6)
# }}}
# }}}

# Tags {{{
# Row1 {{{
label = Gtk::Label.new("", true)
label.set_markup("<b>Tags</b>")
label.set_alignment(0, 0)
label.set_padding(0, 3)
table.attach_defaults(label, 0, 4, 6, 7)
# }}}

# Row2 {{{
entry_t1name = Gtk::Entry.new()
table.attach_defaults(entry_t1name, 0, 2, 7, 8)

button = Gtk::Button.new("create")
button.signal_connect("clicked") do
  begin
    name = entry_t1name.text

    if(!name.nil?)
      $subtle.add_tag(name)
      label_action.set_markup("<b>Added tag #{name}</b>")
      GtkUpdateTags()
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 4, 7, 8)
# }}}

# Row3 {{{
combo_t2name = Gtk::ComboBox.new()
combo_t2name.model = @list_tags
table.attach_defaults(combo_t2name, 0, 2, 8, 9)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") do
  begin
    name = combo_t2name.text

    if(!name.nil?)
      $subtle.del_tag(name)
      label_action.set_markup("<b>Deleted tag #{name}</b>")
      GtkUpdateTags()
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 4, 8, 9)
# }}}
# }}}

# Views {{{
# Row1 {{{
label = Gtk::Label.new("", true)
label.set_markup("<b>Views</b>")
label.set_alignment(0, 0)
label.set_padding(0, 3)
table.attach_defaults(label, 0, 4, 9, 10)
# }}}

# Row2 {{{
entry_v1name = Gtk::Entry.new()
table.attach_defaults(entry_v1name, 0, 2, 10, 11)

button = Gtk::Button.new("create")
button.signal_connect("clicked") do
  begin
    name = entry_v1name.text

    if(!name.nil?)
      $subtle.add_view(name)
      GtkUpdateViews()
      label_action.set_markup("<b>Added view #{name}</b>")
      GtkUpdateViews()
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 4, 10, 11)
# }}}

# Row3 {{{
combo_v2name = Gtk::ComboBox.new()
combo_v2name.model = @list_views
table.attach_defaults(combo_v2name, 0, 1, 11, 12)

button = Gtk::Button.new("jump")
button.signal_connect("clicked") do
  begin
    name = combo_v2name.active_text

    if(!name.nil?)
      $subtle.find_view(name).jump()
      label_action.set_markup("<b>Jumped to view #{name}</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 1, 2, 11, 12)

button = Gtk::Button.new("tags")
button.signal_connect("clicked") do
  begin
    name = combo_v2name.active_text

    if(!name.nil?)
      v = $subtle.find_view(name)
      label_action.set_markup("<b>Tags of view #{name}: " + v.tags.join(", ") + "</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 3, 11, 12)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") do
  begin
    name = combo_v2name.active_text

    if(!name.nil?)
      $subtle.del_view(name)
      label_action.set_markup("<b>Deleted view #{name}</b>")
      GtkUpdateViews()
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 3, 4, 11, 12)
# }}}

# Row4 {{{
combo_v3name = Gtk::ComboBox.new()
combo_v3name.model = @list_views
table.attach_defaults(combo_v3name, 0, 1, 12, 13)

combo_v3tag = Gtk::ComboBox.new()
combo_v3tag.model = @list_tags
table.attach_defaults(combo_v3tag, 1, 2, 12, 13)

button = Gtk::Button.new("tag")
button.signal_connect("clicked") do
  begin
    name = combo_v3name.active_text
    tag  = combo_v3tag.active_text

    if(!name.nil? && !tag.nil?)
      $subtle.find_view(name).tag(tag)
      label_action.set_markup("<b>Added tag #{tag} to view #{name}</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 2, 3, 12, 13)

button = Gtk::Button.new("untag")
button.signal_connect("clicked") do
  begin
    name = combo_v3name.active_text
    tag  = combo_v3tag.active_text

    if(!name.nil? && !tag.nil?)
      $subtle.find_view(name).untag(tag)
      label_action.set_markup("<b>Deleted tag #{tag} from view #{name}</b>")
    end
  rescue
    GtkMessage($!)
  end
end
table.attach_defaults(button, 3, 4, 12, 13)
# }}}
# }}}

# Hbox {{{
hbox = Gtk::HBox.new()
vbox.pack_start(hbox)

link = Gtk::LinkButton.new("subtle (#{$subtle.version})")
link.uri = "http://unexist.scrapping.cc/projects/show/subtle"
hbox.pack_start(link)

link = Gtk::LinkButton.new("Gtk+ (#{Gtk::MAJOR_VERSION}.#{Gtk::MINOR_VERSION}.#{Gtk::MICRO_VERSION})")
link.uri = "http://ruby-gnome2.sourceforge.jp"
hbox.pack_start(link)
# }}}

vbox.pack_start(label_action)

# Init
GtkUpdateClients()
GtkUpdateSublets()
GtkUpdateTags()
GtkUpdateViews()

@window.show_all()

Gtk.main()
