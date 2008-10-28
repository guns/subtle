#
# Loadavg
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the loadavg
# Version: 0.1
# Date: Sat Sep 13 19:00 CET 2008
# $Id$
#

class Loadavg < Sublet
  def initialize
    self.interval = 10
  end

  def run
    file = ""

    begin
      File.open("/proc/loadavg", "r") do |f|
        file = f.read
      end

      self.data = file.slice(0, 14)
    rescue => err
      p err
    end
  end
end
