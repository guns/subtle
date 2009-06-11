#
# Cpu
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the cpu usage
# Version: 0.1
# Date: Mon May 18 21:00 CET 2009
# $Id$
#

class Cpu < Subtle::Sublet
  @use   = 0
  @sum   = 0
  @last  = 0
  @delta = 0
  @load  = 0

  def initialize
    self.interval = 30
    @use          = 0
    @sum          = 0
    @last         = Time.now.to_i
    @delta        = 0
    @load         = 0
    end

  def run
    begin
      file = ""

      # Read tempt state file
      File.open("/proc/stat", "r") do |f|
        file = f.read
      end

      user, nice, system = file.match(/cpu\s+(\d+) (\d+) (\d+)/).captures
      sum = user.to_i + nice.to_i + system.to_i

      @time  = Time.now.to_i
      @delta = @time - @last
      @delta = 1 if(0 == @delta)
      @last  = @time

      @use   = ((sum - @sum) / @delta / 100.0)
      @sum   = sum
      @load  = (@use * 100.0).ceil % 100

      self.data = @load.to_s + "%" + color(COLORS["bg_focus"]) + "//"
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end  
  end
end
