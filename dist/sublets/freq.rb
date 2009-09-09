#
# Freq
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the cpu frequence
# Version: 0.1
# Date: Mon May 18 21:00 CET 2009
# $Id$
#

class Freq < Subtle::Sublet
  attr_accessor :freq

  def initialize
    self.interval = 120
    @freq         = 0
  end

  def run
    begin
      file = ""

      # Read temp state file
      File.open("/proc/cpuinfo", "r") do |f|
        file = f.read
      end

      @freq = file.match(/cpu MHz\s+:\s+([0-9.]+)/).captures.first.to_i.floor.to_s

      self.data = @freq + " Mhz"
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end  
  end
end
