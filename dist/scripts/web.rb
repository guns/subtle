#
# subtle - test script
# Copyright (c) 2005-2008 Christoph Kappel
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#
# $Id$
#

require("rubygems")
require("sinatra")
require("/home/unexist/projects/subtle/subtlext")

$subtle = nil

before do
  begin
    $subtle = Subtle.new(":2")
  rescue
    throw(:halt, [401, "subtle is not running"])
  end
end

get("/") do # {{{
  haml(:index)
end # }}}

post("/clients/tags") do # {{{
  name = params["clients[name]"] || nil

  if(!name.nil?)
    c = $subtle.find_client(name)
    @action = "Tags of client #{name}: " + c.tags.join(", ")
  end

  haml(:index)
end # }}}

post("/clients/tag") do # {{{
  name = params["clients[name]"] || nil
  tag  = params["clients[tag]"] || nil

  if(!name.nil? && !tag.nil?)
    $subtle.find_client(name).tag(tag)
    @action = "Added tag #{tag} to client #{name}"
  end

  haml(:index)
end # }}}

post("/clients/untag") do # {{{
  name = params["clients[name]"] || nil
  tag  = params["clients[tag]"] || nil

  if(!name.nil? && !tag.nil?)
    $subtle.find_client(name).untag(tag)
    @action = "Deleted tag #{tag} from client #{name}"
  end

  haml(:index)
end # }}}

post("/clients/toggle") do # {{{
  name   = params["clients[name]"] || nil
  action = params["clients[action]"] || nil

  if(!name.nil? && !action.nil?)
    $subtle.find_client(name).send("toggle_" + action)
    @action = "Toggled #{action} of client #{name}"
  end

  haml(:index)
end # }}}

post("/tags/new") do # {{{
  name = params["tags[name]"] || nil

  if(!name.nil?)
    $subtle.add_tag(name)
    @action = "Added tag #{name}"
  end

  haml(:index)
end # }}}

post("/tags/delete") do # {{{
  name = params["tags[name]"] || nil

  if(!name.nil?)
    $subtle.del_tag(name)
    @action = "Deleted tag #{name}"
  end

  haml(:index)
end # }}}

post("/views/new") do # {{{
  name = params["views[name]"] || nil

  if(!name.nil?)
    $subtle.add_view(name)
    @action = "Added view #{name}"
  end

  haml(:index)
end # }}}

post("/views/jump") do # {{{
  name = params["views[name]"] || nil

  if(!name.nil?)
    $subtle.find_view(name).jump
    @action = "Jumped to view #{name}"
  end

  haml(:index)
end # }}}

post("/views/tags") do # {{{
  name = params["views[name]"] || nil

  if(!name.nil?)
    v = $subtle.find_view(name)
    @action = "Tags of view #{name}: " + v.tags.join(", ")
  end

  haml(:index)
end # }}}

post("/views/tag") do # {{{
  name = params["views[name]"] || nil
  tag  = params["views[tag]"] || nil

  if(!name.nil? && !tag.nil?)
    $subtle.find_view(name).tag(tag)
    @action = "Added tag #{tag} to view #{name}"
  end

  haml(:index)
end # }}}

post("/views/untag") do # {{{
  name = params["views[name]"] || nil
  tag  = params["views[tag]"] || nil

  if(!name.nil? && !tag.nil?)
    $subtle.find_view(name).untag(tag)
    @action = "Deleted tag #{tag} from view #{name}"
  end

  haml(:index)
end # }}}

post("/views/delete") do # {{{
  name = params["views[name]"] || nil

  if(!name.nil?)
    $subtle.del_view(name)
    @action = "Deleted view #{name}"
  end

  haml(:index)
end # }}}

use_in_file_templates!

__END__

@@ layout
!!! 1.1
%html
  %head
    %title= "subtle #{$subtle.version}"

  %body= yield()

  .footer{:style => "margin: 10px 0px 10px 0px; font-size: 11px"}
    %a{:href => "http://unexist.scrapping.cc/projects/show/subtle"}= "subtle (#{$subtle.version})"
    %a{:href => "http://sinatra.rubyforge.org"}= "Sinatra (#{VERSION})"

  .action{:style => "margin-top: 10px; font-weight: bold"}= @action if(!@action.nil?)

@@ index
%h1{:style => "font-size: 14px"}
  %a{:href => "/"}= $subtle

-# Clients {{{
.clients 
  %h2{:style => "font-size: 12px"} Clients
  %form{:action => "/clients/tags", :method => "post"}
    %select{:name => "clients[name]", :tabindex => "1"}

      -$subtle.clients.each do |c|
        %option{:value => c.name}= c.name

    %input{:type => "submit", :value => "tags", :onclick => "this.form.action = '/clients/tags'", :tabindex => "2"}
    %input{:type => "submit", :value => "delete", :onclick => "this.form.action = '/clients/delete'", :tabindex => "3"}

  %form{:action => "/clients/tag", :method => "post"}
    %select{:name => "clients[name]", :tabindex => "1"}

      -$subtle.clients.each do |c|
        %option{:value => c.name}= c.name

    %select{:name => "clients[tag]", :tabindex => "2"}

      -$subtle.tags.each do |t|
        %option{:value => t.name}= t.name

    %input{:type => "submit", :value => "tag", :onclick => "this.form.action = '/clients/tag'", :tabindex => "3"}
    %input{:type => "submit", :value => "untag", :onclick => "this.form.action = '/clients/untag'", :tabindex => "4"}

  %form{:action => "/clients/toggle", :method => "post"}
    %select{:name => "clients[name]", :tabindex => "1"}

      -$subtle.clients.each do |c|
        %option{:value => c.name}= c.name
  
    %select{:name => "clients[action]", :tabindex => "2"}
      %option{:value => "full"} full
      %option{:value => "float"} float
      %option{:value => "stick"} stick

    %input{:type => "submit", :value => "toggle", :tabindex => "3"}
-#}}}

-# Sublets {{{
.sublets 
  %h2{:style => "font-size: 12px"} Sublets
  %form{:action => "/sublets/kill", :method => "post"}
    %select{:name => "sublets[name]", :tabindex => "1"}

      -$subtle.sublets.each do |s|
        %option{:value => s.name}= s.name

    %input{:type => "submit", :value => "delete", :tabindex => "2"}
-# }}} 

-# Tags {{{
.tags
  %h2{:style => "font-size: 12px"} Tags
  %form{:action => "/tags/new", :method => "post"}
    %input{:type => "text", :name => "tags[name]", :tabindex => "1"}
    %input{:type => "submit", :value => "create", :tabindex => "2"}
   
  %form{:action => "/tags/delete", :method => "post"}
    %select{:name => "tags[name]", :tabindex => "1"}

      -$subtle.tags.each do |t|
        %option{:value => t.name}= t.name

    %input{:type => "submit", :value => "delete", :tabindex => "2"}
-# }}}

-# Views {{{
.views 
  %h2{:style => "font-size: 12px"} Views
  %form{:action => "/views/new", :method => "post"}
    %input{:type => "text", :name => "views[name]", :tabindex => "1"}
    %input{:type => "submit", :value => "create", :tabindex => "2"}
  
  %form{:action => "/views/jump", :method => "post"}
    %select{:name => "views[name]", :tabindex => "1"}

      -$subtle.views.each do |v|
        %option{:value => v.name, :selected => v.name == $subtle.current_view.name}= v.name

    %input{:type => "submit", :value => "jump", :onclick => "this.form.action = '/views/jump'", :tabindex => "2"}
    %input{:type => "submit", :value => "tags", :onclick => "this.form.action = '/views/tags'", :tabindex => "2"}
    %input{:type => "submit", :value => "delete", :onclick => "this.form.action = '/views/delete'", :tabindex => "3"}

  %form{:action => "/views/tag", :method => "post"}
    %select{:name => "views[name]", :tabindex => "1"}

      -$subtle.views.each do |v|
        %option{:value => v.name, :selected => v.name == $subtle.current_view.name}= v.name

    %select{:name => "views[tag]", :tabindex => "2"}

      -$subtle.tags.each do |t|
        %option{:value => t.name}= t.name

    %input{:type => "submit", :value => "tag", :onclick => "this.form.action = '/views/tag'", :tabindex => "3"}
    %input{:type => "submit", :value => "untag", :onclick => "this.form.action = '/views/untag'", :tabindex => "4"}
-# }}}
