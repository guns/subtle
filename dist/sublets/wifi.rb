#
# Wifi
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the status of a wifi device
# Version: 0.1
# Date: Sat Sep 05 11:00 CET 2009
# $Id$
#

class Wifi < Subtle::Sublet
  attr_accessor :icon, :re_essid, :re_quality, :device

  def initialize
    self.interval = 60
    @icon         = Subtle::Icon.new(ENV["XDG_DATA_HOME"] + "/icons/wifi_01.xbm")

    # Precompile regex
    @re_essid     = Regexp.new('.*ESSID:"?(.*?)"?\s*\n')
    @re_quality   = Regexp.new('.*Quality:?=? ?(\d+)\s*/?\s*(\d*)')

    # Select wifi device
    @device       = "wlan0"
  end

  def run
    begin
      # Fetch data from iwconfig
      iwconfig         = `iwconfig #{@device}`
      match, essid     = iwconfig.match(@re_essid).to_a
      match, low, high = iwconfig.match(@re_quality).to_a

      self.data = "%s%s (%d/%d)" % [ @icon, essid, low, high ]
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
