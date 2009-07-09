#
# Storage
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the storage usage
# Version: 0.1
# Date: Mon May 18 21:00 CET 2009
# $Id$
#

class Storage < Subtle::Sublet
  @left  = 0
  @total = 0

  def initialize
    self.interval = 120
  end

  def run
    report = ""

    begin
      report = `df -h`
      @total, @left = report.match(/([0-9.]+.)\s+[0-9.]+.\s+([0-9.]+.)\s+[0-9.]+%\s+\/\s+/).captures

      self.data = @left + "/" + @total + color(COLORS[:bg_focus]) + "//"
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
