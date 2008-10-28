#
# Clock
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the clock
# Version: 0.1
# Date: Sat Sep 13 19:00 CET 2008
# $Id$
#

class Clock < Sublet
  def initialize
    self.interval = 60
  end

  def run
    puts self.interval
    puts self.data
    self.data = Time.now().strftime("%d%m%y%H%M")
  end
end
