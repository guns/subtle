#
# Memory
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the memory usage
# Version: 0.1
# Date: Fri Jan 09 16:00 CET 2009
# $Id$
#

class Memory < Subtle::Sublet
  @total  = 0
  @active = 0

  def initialize
    self.interval = 30
  end

  def run
    file = ""

    begin
      File.open("/proc/meminfo", "r") do |f|
        file = f.read
      end

      @total  = file.match(/MemTotal:\s*(\d+)\s*kB/)[1].to_i / 1024 || 0
      @active = file.match(/Active:\s*(\d+)\s*kB/)[1].to_i / 1024 || 0

      self.data = "%dM/%dM" % [@active.floor, @total.floor]
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
