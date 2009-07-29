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
  def initialize
    self.interval = 30
  end

  def run
    file = ""

    begin
      File.open("/proc/meminfo", "r") do |f|
        file = f.read
      end

      # Collect data
      @total   = file.match(/MemTotal:\s*(\d+)\s*kB/)[1].to_i || 0
      @free    = file.match(/MemFree:\s*(\d+)\s*kB/)[1].to_i || 0
      @buffers = file.match(/Buffers:\s*(\d+)\s*kB/)[1].to_i || 0
      @cached  = file.match(/Cached:\s*(\d+)\s*kB/)[1].to_i || 0

      @used    = (@total - (@free + @buffers + @cached)) / 1024
      @total   = @total / 1024

      self.data = @used.to_s + "/" + @total.to_s + "  |  "
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
