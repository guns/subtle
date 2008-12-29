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

subtle = Subtle.new(":2")

# Window {{{
window = Gtk::Window.new("subtle")
window.border_width = 10

window.signal_connect("delete_event") do
  false
end

window.signal_connect("destroy") do
  Gtk.main_quit
end

vbox = Gtk::VBox.new()
window.add(vbox)

label = Gtk::Label.new(subtle.to_s, true)
vbox.pack_start(label)

label_action = Gtk::Label.new("", true)
# }}}

# Clients {{{
label = Gtk::Label.new("Clients", true)
vbox.pack_start(label)

table = Gtk::Table.new(4, 3)
vbox.pack_start(table)

# Row1 {{{
combo = Gtk::ComboBox.new()
subtle.clients.each do |client|
  combo.append_text(client.name)
end
table.attach_defaults(combo, 0, 1, 0, 1)

button = Gtk::Button.new("tags")
button.signal_connect("clicked") {
  puts "tags"
}
table.attach_defaults(button, 1, 2, 0, 1)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") {
  puts "delete"
}
table.attach_defaults(button, 2, 3, 0, 1)
# }}}

# Row2 {{{
combo = Gtk::ComboBox.new()
subtle.clients.each do |client|
  combo.append_text(client.name)
end
table.attach_defaults(combo, 0, 1, 1, 2)

combo = Gtk::ComboBox.new()
subtle.tags.each do |tag|
  combo.append_text(tag.name)
end
table.attach_defaults(combo, 1, 2, 1, 2)

button = Gtk::Button.new("tag")
button.signal_connect("clicked") {
  puts "delete"
}
table.attach_defaults(button, 2, 3, 1, 2)

button = Gtk::Button.new("untag")
button.signal_connect("clicked") {
  puts "untag"
}
table.attach_defaults(button, 3, 4, 1, 2)
# }}}

# Row3 {{{
combo = Gtk::ComboBox.new()
subtle.clients.each do |client|
  combo.append_text(client.name)
end
table.attach_defaults(combo, 0, 1, 2, 3)

combo = Gtk::ComboBox.new()
combo.append_text("full")
combo.append_text("float")
combo.append_text("urgent")
table.attach_defaults(combo, 1, 2, 2, 3)

button = Gtk::Button.new("toggle")
button.signal_connect("clicked") {
  puts "toggle"
}
table.attach_defaults(button, 2, 3, 2, 3)
# }}}
# }}}

# Sublets {{{
label = Gtk::Label.new("Sublets", true)
vbox.pack_start(label)

table = Gtk::Table.new(1, 1)
vbox.pack_start(table)

# Row1 {{{
combo = Gtk::ComboBox.new()
subtle.sublets.each do |sublet|
  combo.append_text(sublet.name)
end
table.attach_defaults(combo, 0, 1, 0, 1)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") {
  puts "delete"
}
table.attach_defaults(button, 1, 2, 0, 1)
# }}}
# }}}

# Tags {{{
label = Gtk::Label.new("Tags", true)
vbox.pack_start(label)

table = Gtk::Table.new(2, 2)
vbox.pack_start(table)

# Row1 {{{
entry = Gtk::Entry.new()
table.attach_defaults(entry, 0, 1, 0, 1)

button = Gtk::Button.new("create")
button.signal_connect("clicked") {
  puts "create"
}
table.attach_defaults(button, 1, 2, 0, 1)
# }}}

# Row2 {{{
combo = Gtk::ComboBox.new()
subtle.tags.each do |tag|
  combo.append_text(tag.name)
end
table.attach_defaults(combo, 0, 1, 1, 2)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") {
  puts "delete"
}
table.attach_defaults(button, 1, 2, 1, 2)
# }}}
# }}}

# Views {{{
label = Gtk::Label.new("Views", true)
vbox.pack_start(label)

table = Gtk::Table.new(3, 4)
vbox.pack_start(table)

# Row1 {{{
entry = Gtk::Entry.new()
table.attach_defaults(entry, 0, 1, 0, 1)

button = Gtk::Button.new("create")
button.signal_connect("clicked") {
  puts "create"
}
table.attach_defaults(button, 1, 2, 0, 1)
# }}}

# Row2 {{{
combo_v2name = Gtk::ComboBox.new()
subtle.views.each do |view|
  combo_v2name.append_text(view.name)
end
table.attach_defaults(combo_v2name, 0, 1, 1, 2)

button_v2jump = Gtk::Button.new("jump")
button_v2jump.signal_connect("clicked") {
  name = combo_v2name.active_text

  if(!name.nil?)
    subtle.find_view(name).jump()
    label_action.text = "Jumped to view #{name}"
  end
}
table.attach_defaults(button_v2jump, 1, 2, 1, 2)

button = Gtk::Button.new("tags")
button.signal_connect("clicked") {
  puts "tags"
}
table.attach_defaults(button, 2, 3, 1, 2)

button = Gtk::Button.new("delete")
button.signal_connect("clicked") {
  puts "delete"
}
table.attach_defaults(button, 3, 4, 1, 2)
# }}}

# Row3 {{{
combo = Gtk::ComboBox.new()
subtle.views.each do |view|
  combo.append_text(view.name)
end
table.attach_defaults(combo, 0, 1, 2, 3)

combo = Gtk::ComboBox.new()
subtle.tags.each do |tag|
  combo.append_text(tag.name)
end
table.attach_defaults(combo, 1, 2, 2, 3)

button = Gtk::Button.new("tag")
button.signal_connect("clicked") {
  puts "tag"
}
table.attach_defaults(button, 2, 3, 2, 3)

button = Gtk::Button.new("untag")
button.signal_connect("clicked") {
  puts "untag"
}
table.attach_defaults(button, 3, 4, 2, 3)
# }}}
# }}}

vbox.pack_start(label_action)

label = Gtk::Label.new("subtle (#{subtle.version}) Gtk+ (#{Gtk::MAJOR_VERSION}.#{Gtk::MINOR_VERSION}.#{Gtk::MICRO_VERSION})", true)
vbox.pack_start(label)


window.show_all()

Gtk.main()
