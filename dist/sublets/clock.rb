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

class Clock < Subtle::Sublet
  def initialize
    self.interval = 60
  end

  def run
    self.data = color("#0dd2e5") + Time.now().strftime("%H:%M")
  end
end
