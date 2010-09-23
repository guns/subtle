#
# @package sur
#
# @file Runner functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require "subtle/sur/client"

module Subtle # {{{
  module Sur # {{{
    # Runner class for argument dispatching
    class Runner # {{{
      # Trap SIGINT
      trap "INT" do
        exit
      end

      ## Subtle::Sur::Runner::run {{{
      # Start runner
      #
      # @param [Array, #read]  args  Argument array
      #
      # @raise [String] Command error
      # @since 0.0
      #
      #  Subtle::Sur::Runner.new.run(args)
      #  => nil

      def run(args)
        # Default options
        @repo    = "local"
        @version = nil
        @tags    = false
        @port    = 4567
        @color   = true
        @regex   = false
        @reload  = false
        @assume  = false

        # GetoptLong only works on ARGV so we use something own
        begin
          args.delete_if do |arg|
            case arg
              when "-h", "--help"
                usage(args[0])
                exit
              when "-l", "--local"
                @repo = "local"

                true
              when "-r", "--remote"
                @repo = "remote"

                true
              when "-e", "--regex"
                @regex = true

                true
              when "-t", "--tag"
                @tags = true

                true
              when "-v", "--version"
                @version = extract(args, arg)

                true
              when "-p", "--port"
                @port = extract(args, arg).to_i

                true
              when "-c", "--no-color"
                @color = false

                true
              when "-R", "--reload"
                @reload = true

                true
              when "-y", "--yes"
                @assume = true

                true
            end
          end
        rescue
          raise "Couldn't find required arguments"
        end

        # Run
        case args[0]
          when "annotate"  then Subtle::Sur::Client.new.annotate(args[1], @version)
          when "build"     then Subtle::Sur::Client.new.build(args[1])
          when "help"      then usage(nil)
          when "install"   then
            if(args[1].nil?)
              usage(args[0])
            else
              Sur::Client.new.install(args[1], @version, @tags, @reload)
            end
          when "list"      then Subtle::Sur::Client.new.list(@repo, @color)
          when "notes"     then Subtle::Sur::Client.new.notes(args[1])
          when "query"     then
            if(args[1].nil?)
              usage(args[0])
            else
              Subtle::Sur::Client.new.query(args[1], @repo,
                @version, @regex, @tags, @color)
            end
          when "reorder"   then Subtle::Sur::Client.new.reorder
          when "server"    then
            require "sur/server"

            Sur::Server.new(@port).run
          when "submit"    then Subtle::Sur::Client.new.submit(args[1])
          when "template"  then Subtle::Sur::Specification.template(args[1])
          when "test"
            require "sur/test/sublet"

            args.delete_at(0) #< Remove test

            Subtle::Sur::Test.new.run(args)
          when "uninstall" then
            if(args[1].nil?)
              usage(args[0])
            else
              Subtle::Sur::Client.new.uninstall(args[1],
                @version, @tags, @reload)
            end
          when "update"    then Subtle::Sur::Client.new.update(@repo)
          when "upgrade"   then Subtle::Sur::Client.new.upgrade(@color,
            @assume, @reload)
          when "version"   then version
          else usage(nil)
        end
      end # }}}

      private

      def extract_at(args, idx) # {{{
        ret = args.at(idx)
        args.delete_at(idx)

        ret
      end # }}}

      def extract(args, arg) # {{{
        idx = args.index(arg)
        ret = extract_at(args, idx + 1)

        ret
      end # }}}

      def usage(arg) # {{{
        case arg
          when "annotate"
            puts "Usage: sur annotate NAME [OPTIONS]\n\n" \
                 "Mark sublet as to be reviewed\n\n" \
                 "Options:\n" \
                 "  -v, --version VERSION     Annotate a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur list -l\n" \
                 "sur list -r\n"
          when "install"
            puts "Usage: sur install NAME [OPTIONS]\n\n" \
                 "Install a sublet by given name\n\n" \
                 "Options:\n" \
                 "  -R, --reload              Reload sublets after install\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Search for a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur install clock\n" \
                 "sur install clock -v 0.3\n"
          when "list"
            puts "Usage: sur list [OPTIONS]\n\n" \
                 "List sublets in repository\n\n" \
                 "Options:\n" \
                 "  -l, --local               Select local repository (default)\n" \
                 "  -r, --remote              Select remote repository\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur list -l\n" \
                 "sur list -r\n"
          when "query"
            puts "Usage: sur query NAME [OPTIONS]\n\n" \
                 "Query a repository for a sublet by given name\n\n" \
                 "Options:\n" \
                 "  -e, --regex               Use regex for query\n" \
                 "  -l, --local               Select local repository (default)\n" \
                 "  -r, --remote              Select remote repository\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Search for a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur query -l clock\n" \
                 "sur query -r clock -v 0.3\n"
          when "server"
            puts "Usage: sur server [OPTIONS]\n\n" \
                 "Serve sublets on localhost ann optionally on a given port\n\n" \
                 "Options:\n" \
                 "  -p, --port PORT           Select a specific port\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur server -p 3000\n"
          when "uninstall"
            puts "Usage: sur uninstall NAME [OPTIONS]\n\n" \
                 "Uninstall a sublet by given name and optionally by given version\n\n" \
                 "Options:\n" \
                 "  -R, --reload              Reload sublets after uninstall\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Select a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur uninstall clock\n" \
                 "sur uninstall clock -v 0.3\n"
          when "update"
            puts "Usage: sur update [-l|-r|-h]\n\n" \
                 "Update local/remote sublet cache\n\n" \
                 "Options:\n" \
                 "  -l, --local               Select local repository (default)\n" \
                 "  -r, --remote              Select remote repository\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur update -l\n" \
                 "sur update -r\n"
          when "upgrade"
            puts "Usage: sur upgrade [-R|-h]\n\n" \
                 "Upgrade all sublets if possible\n\n" \
                 "Options:\n" \
                 "  -R, --reload              Reload sublets after upgrading\n" \
                 "  -y, --yes                 Assume yes to questions\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur upgrade\n" \
                 "sur upgrade -R\n"
          else
            puts "Usage: sur [OPTIONS]\n\n" \
                 "Options:\n" \
                 "  annotate NAME [-v VERSION|-h]           Mark sublet to be reviewed\n" \
                 "  build SPEC                              Create a sublet package\n" \
                 "  help                                    Show this help and exit\n" \
                 "  install NAME [-R|-t|-v VERSION|-h]      Install a sublet\n" \
                 "  list [-l|-r|-h]                         List local/remote sublets\n" \
                 "  notes                                   Show notes about an installed sublet\n" \
                 "  query NAME [-e|-l|-t|-v VERSION|-h]     Query for a sublet (e.g clock, clock -v 0.3)\n" \
                 "  reorder                                 Reorder installed sublets for loading order\n" \
                 "  server [-p PORT|-h]                     Serve sublets (default: http://localhost:4567)\n" \
                 "  submit FILE                             Submit a sublet to SUR\n" \
                 "  template FILE                           Create a new sublet template in current dir\n" \
                 "  test FILE                               Test given sublets for syntax and functionality\n" \
                 "  uninstall NAME [-R|-t|-v VERSION|-h]    Uninstall a sublet\n" \
                 "  update [-l|-r|-h]                       Update local/remote sublet cache\n" \
                 "  upgrade [-R|-y|-h]                      Upgrade all installed sublets\n" \
                 "  version                                 Show version info and exit\n"
        end

        puts "\nPlease report bugs to <unexist@dorfelite.net>\n"
      end # }}}

      def version # {{{
        puts "Sur #{VERSION} - Copyright (c) 2009 Christoph Kappel\n" \
             "Released under the GNU General Public License\n"
      end # }}}
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
