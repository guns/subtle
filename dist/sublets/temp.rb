#
# Temp
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the cpu temperature
# Version: 0.1
# Date: Mon May 18 21:00 CET 2009
# $Id$
#

class Temp < Subtle::Sublet
  @temp = ""

  def initialize
    self.interval = 60
  end

  def run
    begin
      file = ""

      # Read tempt state file
      File.open("/proc/acpi/thermal_zone/THRM/temperature", "r") do |f|
        file = f.read
      end

      @temp = file.match(/temperature:\s+(\d+)/).captures

      self.data = @temp.to_s + " C" + color(COLORS["bg_focus"]) + "//"
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
