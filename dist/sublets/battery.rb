#
# Battery
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the battery state
# Version: 0.1
# Date: Sat Sep 13 19:00 CET 2008
# $Id$
#

class Battery < Subtle::Sublet
  attr_accessor :path, :capacity, :remaining, :rate, :state

  def initialize
    self.interval = 60
    @path         = Dir["/proc/acpi/battery/*"][0] #< Get battery slot
    @capacity     = 0
    @remaining    = 0
    @rate         = 0
    @state        = ""

    begin
      file = ""

      # Read battery info file
      File.open(@path + "/info", "r") do |f|
        file = f.read
      end

      @capacity = file.match(/last full capacity:\s*(\d+).*/)[1].to_i
    rescue => err
      err
    end
  end

  def run
    begin
      file = ""
      ac   = ""

      # Read battery state file
      File.open(@path + "/state", "r") do |f|
        file = f.read
      end

      @remaining = file.match(/remaining capacity:\s*(.+).*/)[1].to_i || 1
      @rate      = file.match(/present rate:\s*(.+).*/)[1].to_i || 1
      @rate      = 1 if(0 == @rate) #< Rate can be unkown
      @state     = file.match(/charging state:\s*(.+).*/)[1]

      case @state
        when "charged" then 
          ac = "AC"
        when "discharging" then
          remain = @remaining / @rate * 60 #< Get remaining time
          ac     = "%d:%d" % [(remain / 60).floor, remain % 60]
        when "charging" then
          ac = "CHG"
      end
  
      self.data = "%d%%/%s  |  " % [ (@remaining * 100 / @capacity).floor, ac ]
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
