#
# example - subtlext demo script
# Copyright (c) 2005-2009 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

require("subtle/subtlext")

puts "subtle %s on %s" % 
  [Subtlext::Subtle.version, Subtlext::Subtle.running? ? Subtlext::Subtle.display : "none"]

puts "Tags: %s" % [Subtlext::Tag[:all].join(", ")]

# Views
views = []
Subtlext::View[:all].each do |v|
  views.push("%s (%s)" % [v.current? ? "[#{v}]" : v, v.tags.join(", ")])
end
puts "Views: %s" % [views.join(", ")]

# Clients
clients = []
Subtlext::Client[:all].each do |c|
  clients.push("%s (%s)" % [c, c.tags.join(", ")])
end
puts "Clients: %s" % [clients.join(", ")]
