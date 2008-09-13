#
# Notify
#
# Author: Christoph Kappel
# Contact: unexist@dorfelite.net
# Description: Notify example
# Version: 0.1
# Date: Sat Sep 13 19:00 CET 2008
# $Id$
#

class Notify < Sublet
  @@watch = "/tmp/watch"

  def initialize
    super(@@watch)
  end

  def run
    begin
      file = ""

      File.open(@@watch, "r") do |f|
        file = f.read
      end

      file
    rescue => err
      err
    end
  end
end
