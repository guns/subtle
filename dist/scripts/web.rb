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

$s = nil

before do
  begin
    $s = Subtle.new(":2")
  rescue
    throw(:halt, [401, "subtle is not running"])
  end
end

get("/") do
  haml(:index)
end

post("/") do
  @name = params["views[jump]"] || nil

  if(!@name.nil?)
    $s.find_view(@name).jump
    @action = "Jumping to view #{@name}"

    haml(:index)
  end

end

use_in_file_templates!

__END__

@@ layout
!!! 1.1
%html
  %head
    %title= "subtle #{$s.version}"

  %body= yield()

@@ index
.views
  %span{:style => "font-weight: bold"}= "subtle #{$s.version}"
  %form{:id => "views_form", :action => "/", :method => "post"}
    %select{:name => "views[jump]", :tabindex => "1"}

      -$s.views.each do |v|
        %option{:value => v.name, :selected => v.name == $s.current_view.name}= v.name

    %input{:type => "submit", :value => "jump", :tabindex => "2"}

    .action= @action if(!@action.nil?)
