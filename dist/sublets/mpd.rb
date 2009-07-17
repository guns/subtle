#
# Mpd
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Show the current track
# Version: 0.1
# Date: Fri May 23 00:25 CET 2009
# $Id$
#

class Mpd < Subtle::Sublet
  def initialize
    self.interval = 30
  end

  def shorten(str, limit = 30)
    return "subtle" if(str.nil?)

    if(str.length > limit)
      str[0..limit] + ".."
    else
      str
    end
  end

  def seconds(str)
    return 0 if(str.nil?)

    ary = str.split(":")

    ary.first.to_i * 60 + ary.last.to_i
  end

  def run
    begin
      matches = `mpc`.match(/(.*)\n\[(.*)\].* ([0-9:]+)\/([0-9:]+)/)

      if(!matches.nil?)
        title, state, ctime, ttime = matches.captures

        self.data     = shorten(title) + color("#CF6171") + " // "
        #self.interval = seconds(ttime) - seconds(ctime) #< Recalc interval
      else
        self.data = "mpd stopped/not running"
      end
    rescue => err # Sanitize to prevent unloading
      self.data = "subtle"
      p err
    end
  end
end
