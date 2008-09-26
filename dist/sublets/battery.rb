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

class Battery < Sublet
  @path      = 0
  @capacity  = 0
  @remaining = 0
  @rate      = 0
  @state     = ""

  def initialize
    @interval = 60
    @path     = Dir["/proc/acpi/battery/*"][0] #< Get battery slot

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

      @remaining = file.match(/remaining capacity:\s*(\d+).*/)[1].to_i
      @rate      = file.match(/present rate:\s*(\d+).*/)[1].to_i
      @state     = file.match(/charging state:\s*(\w+).*/)[1]

      case @state
        when "charged" then 
          ac = "AC"
        when "discharging" then
          remain = @remaining / @rate * 60 #< Get remaining time
          ac     = "%d:%d" % [remain / 60, Math.floor(remain % 60)]
        when "charging" then
          ac = "CHG"
      end
  
      @data = "%d%%/%s" % [(@remaining * 100 / @capacity).floor, ac]
    rescue => err
      p err
    end
  end
end
