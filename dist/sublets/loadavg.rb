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

class Loadavg < Subtle::Sublet
  def initialize
    self.interval = 30
  end

  def run
    file = ""

    begin
      File.open("/proc/loadavg", "r") do |f|
        file = f.read
      end

      self.data = file.slice(0, 14)
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
