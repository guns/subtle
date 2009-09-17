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
  attr_accessor :icon, :cpus, :sum, :last, :delta

  def initialize
    self.interval = 30
    @icon         = Subtle::Icon.new(ENV["XDG_DATA_HOME"] + "/icons/cpu.xbm")
    @cpus         = 0
    @last         = []
    @delta        = []
    @sum          = []

    # Init and count CPUs
    begin
      file = IO.readlines("/proc/stat").join

      file.scan(/cpu(\d+)/) do |num| 
        n         = num.first.to_i
        @cpus    += 1
        @last[n]  = 0
        @delta[n] = 0
        @sum[n]   = 0
      end
    rescue
      raise "Init error"
    end
  end

  def run
    begin
      data = ""
      time = Time.now.to_i
      file = IO.readlines("/proc/stat").join

      file.scan(/cpu(\d+) (\d+) (\d+) (\d+)/) do |num, user, nice, system| 
        n         = num.to_i
        @delta[n] = time - @last[n]
        @delta[n] = 1 if(0 == @delta[n])
        @last[n]  = time

        sum       = user.to_i + nice.to_i + system.to_i
        use       = ((sum - @sum[n]) / @delta[n] / 100.0)
        @sum[n]   = sum
        percent   = (use * 100.0).ceil % 100

        data << percent.to_s + "% "
      end

      self.data = @icon + data.chop
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end  
  end
end
