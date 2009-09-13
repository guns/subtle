#
# Notify
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Notify example
# Version: 0.1
# Date: Sat Sep 13 19:00 CET 2008
# Tags: Default Notify
# $Id$
#

class Notify < Subtle::Sublet
  attr_accessor :file

  def initialize
    @file = ENV["HOME"] + "/notify.log"

    watch @file
  end

  def run
    begin
      self.data = File.readlines(@file).to_s.chop
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end  
  end
end
