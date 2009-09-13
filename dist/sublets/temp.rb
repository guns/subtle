#
# Temp
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the cpu temperature
# Version: 0.1
# Date: Mon May 18 21:00 CET 2009
# Tags: Default Proc
# $Id$
#

class Temp < Subtle::Sublet
  attr_accessor :path, :temp

  def initialize
    self.interval = 60
    @temp         = ""

    begin
      @path = Dir["/proc/acpi/thermal_zone/*"][0] #< Get temp slot
    rescue => err
      err
    end
  end

  def run
    begin
      file = ""

      # Read tempt state file
      File.open(@path + "/temperature", "r") do |f|
        file = f.read
      end

      @temp = file.match(/temperature:\s+(\d+)/).captures

      self.data = @temp.to_s + "C "
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
