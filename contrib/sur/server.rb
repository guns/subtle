#
# @package sur
#
# @file Server functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require "rubygems"
require "fileutils"
require "yaml"
require "sinatra"
require "haml"
require "dm-core"
require "dm-types"
require "digest/md5"
require "curb"
require "xmlrpc/marshal"

require "subtle/sur/specification"

module Subtle # {{{
  module Sur # {{{
    # Model classes for datamapper
    module Model # {{{

      # Sublet class database model
      class Sublet # {{{
        include DataMapper::Resource

        property(:id,          Serial)
        property(:name,        String, :unique_index => :name)
        property(:contact,     String)
        property(:description, String)
        property(:version,     String, :unique_index => :name)
        property(:date,        ::DataMapper::Types::EpochTime)
        property(:path,        String)
        property(:digest,      String)
        property(:ip,          String)
        property(:subtlext,    String, :required => false)
        property(:sur,         String, :required => false)
        property(:downloads,   Integer, :default => 0)
        property(:annotations, Integer, :default => 0)
        property(:created_at,  ::DataMapper::Types::EpochTime)

        has(n, :authors, :model => "Sur::Model::Assoc::Author")
        has(n, :tags,    :model => "Sur::Model::Assoc::Tag")
      end # }}}

      # Tag class database model
      class Tag # {{{
        include DataMapper::Resource

        property(:id,         Serial)
        property(:name,       String, :unique => true)
        property(:created_at, ::DataMapper::Types::EpochTime)

        has(n, :taggings, :model => "Sur::Model::Assoc::Tag")
      end # }}}

      # User class database model
      class User # {{{
        include DataMapper::Resource

        property(:id,         Serial)
        property(:name,       String, :unique => true)
        property(:created_at, ::DataMapper::Types::EpochTime)
      end # }}}

      # Assoc classes for datamapper
      module Assoc

        # Tagging class database model
        class Author # {{{
          include DataMapper::Resource

          property(:id,         Serial)
          property(:user_id,    Integer, :unique_index => :author)
          property(:sublet_id,  Integer, :unique_index => :author)
          property(:created_at, ::DataMapper::Types::EpochTime)

          belongs_to(:user,   :model => "Sur::Model::User")
          belongs_to(:sublet, :model => "Sur::Model::Sublet")
        end # }}}

        # Tag class database model
        class Tag # {{{
          include DataMapper::Resource

          property(:id,         Serial)
          property(:tag_id,     Integer, :unique_index => :tag)
          property(:sublet_id,  Integer, :unique_index => :tag)
          property(:created_at, ::DataMapper::Types::EpochTime)

          belongs_to(:tag,    :model => "Sur::Model::Tag")
          belongs_to(:sublet, :model => "Sur::Model::Sublet")
        end # }}}

        # Tag class database model
        class Annotate # {{{
          include DataMapper::Resource

          property(:id,         Serial)
          property(:sublet_id,  Integer)
          property(:user_id,    Integer)
          property(:created_at, ::DataMapper::Types::EpochTime)

          belongs_to(:sublet, :model => "Sur::Model::Sublet")
          belongs_to(:user,   :model => "Sur::Model::User")
        end # }}}
      end
    end # }}}

    # Server class for repository management
    class Server # {{{
      # Database file
      DATABASE = Dir.pwd + "/repository.db"

      # Repository storage
      REPOSITORY = Dir.pwd + "/repository"

      # Cache file
      CACHE = Dir.pwd + "/cache.yaml"

      # Identi.ca username
      USERNAME = "subtle"

      # Identi.ca password
      PASSWORD = ""

      ## Sur::Server::initialize {{{
      # Create a new Server object
      #
      # @param [Number]  port  Default port
      # @return [Object] New Sur::Server
      #
      # @since 0.0
      #
      # @example
      #   client = Sur::Server.new
      #   => #<Sur::Server:xxx>

      def initialize(port = 4567)
        DataMapper.setup(:default, "sqlite3://" + DATABASE)

        # Create database
        if(!File.exists?(DATABASE))
          DataMapper.auto_migrate!
        end

        # Create store
        if(!File.exists?(REPOSITORY))
          Dir.mkdir(REPOSITORY)
        end

        # Configure sinatra application
        set :port, port
      end # }}}

      ## Sur::Server::run {{{
      # Run sinatra application
      #
      # @since 0.0
      #
      # @example
      #  Sur::Client.new.run
      #  => nil

      def run
        helpers do # {{{
          def build_sublets # :nodoc: {{{
            list = []

            # Fetch sublets from database
            Sur::Model::Sublet.all(:order => [ :name, :version.desc ]).each do |s|

              # Collect authors
              authors = []
              Sur::Model::Assoc::Author.all(:sublet_id => s.id).each do |a|
                authors.push(a.user.name)
              end

              # Collect tags
              tags = []
              Sur::Model::Assoc::Tag.all(:sublet_id => s.id).each do |a|
                tags.push(a.tag.name)
              end

              # Create spec
              spec = Sur::Specification.new do |spec|
                spec.name             = s.name
                spec.authors          = authors
                spec.contact          = s.contact
                spec.description      = s.description
                spec.version          = s.version
                spec.date             = s.date
                spec.tags             = tags
                spec.digest           = s.digest
                spec.subtlext_version = s.subtlext
                spec.sur_version      = s.sur
              end

               list.push(spec)
            end

            # Store cache
            yaml = list.to_yaml
            File.open(Sur::Server::CACHE, "w+") do |out|
              YAML::dump(yaml, out)
            end

            puts ">>> Regenerated sublets cache with #{list.size} sublets"
          end # }}}

          def get_all_sublets_in_interval(args) # :nodoc: {{{
            sublets = []

            # Find sublets in interval
            if(2 == args.size)
              Sur::Model::Sublet.all(
                  :created_at.gte => Time.at(args[0]),
                  :created_at.lte => Time.at(args[1])
                  ).each do |s|
                sublets << "%s-%s" % [ s.name, s.version ]
              end
            end

            XMLRPC::Marshal.dump_response(sublets)
          end # }}}
        end # }}}

        template :layout do # {{{
          <<EOF
  !!! 1.1
  %html
    %head
      %title SUR - Subtle User Repository

      %style{:type => "text/css"}
        :sass
          body
            :color #000000
            :background-color #ffffff
            :font-family Verdana,sans-serif
            :font-size 12px
            :margin 0px
            :padding 0px
          h1
            :color #444444
            :font-size 20px
            :margin 0 0 10px
            :padding 2px 10px 1px 0
          h2
            :color #444444
            :font-size 16px
            :margin 0 0 10px
            :padding 2px 10px 1px 0
          input
            :margin-top 1px
            :margin-bottom 1px
            :vertical-align middle
          input[type="text"]
            :border 1px solid #D7D7D7
          input[type="text"]:focus
            :border 1px solid #888866
          input[type="submit"]
            :background-color #F2F2F2
            :border 1px outset #CCCCCC
            :color #222222
          input[type="submit"]:hover
            :background-color #CCCCBB
          a#download:link, a#download:visited, a#download:active
            :color #000000
            :text-decoration none
            :border-bottom 1px dotted #E4E4E4
          a#download:hover
            :color #000000
            :text-decoration none
          a:link, a:visited, a:active
            :color #E92D45
            :text-decoration none
          a:hover
            :text-decoration underline
          .center
            :margin 0px auto
          .italic
            :font-style italic
          .gray
            :color #999999
          #box
            :background-color #FCFCFC
            :border 1px solid #E4E4E4
            :color #505050
            :line-height 1.5em
            :padding 6px
            :margin-bottom 10px
            :margin-right 10px
            :margin-top 10px
            :float left
            :min-width 280px
            :min-height 240px
          #form
            :width 900px
            :padding 10px 0px 10px 0px
          #frame
            :padding 10px
          #left
            :margin-right 10px
            :margin-top 10px
            :float left
            :width 630px
          #right
            :margin-right 10px
            :margin-top 10px
            :float right
            :width 230px
            :text-align right
          #clear
            :clear both

    %body
      #frame
        =yield
  EOF
        end # }}}

        template :index do # {{{
          <<EOF
  %h2 Sublets
  %p
    Small Ruby scripts written in a
    %a{:href => "http://en.wikipedia.org/wiki/Domain_Specific_Language"} DSL
    that provide things like system information for the
    %a{:href => "http://subtle.subforge.org"} subtle
    panel.

  #form
    #left
      %form{:method => "get", :action => "/search"}
        Search:
        %input{:type => "text", :size => 40, :name => "query"}
        %input{:type => "submit", :name => "submit", :value => "Go"}

    #right
      %a{:href => "http://subforge.org/wiki/sur/Specification"} Sublet specification
      |
      %a{:href => "http://sur.subtle.de/sublets"} All sublets

  #clear

  #box
    %h2 Latest sublets
    %ul
      -@newest.each do |s|
        %li
          %a#download{:href => "http://sur.subtle.de/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]

  #box
    %h2 Most downloaded
    %ul
      -@most.each do |s|
        %li
          %a#download{:href => "http://sur.subtle.de/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%d)" % [ s.downloads ]

  #box
    %h2 Never downloaded
    %ul
      -@never.each do |s|
        %li
          %a#download{:href => "http://sur.subtle.de/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]

    %h2 Just broken
    %ul
      -@worst.each do |s|
        %li
          %a#download{:href => "http://sur.subtle.de/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%d)" % [ s.annotations ]
  EOF
        end # }}}

        template :tag do # {{{
          <<EOF
  #box
    %h2
      Sublets tagged with
      %span.italic= @tag

    %ul
      -if(!@list.nil?)
        -@list.each do |s|
          %li
            %a#download{:href => "/get/%s" % [ s.digest ] }
              ="%s-%s" % [ s.name, s.version ]
            ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]
  #clear

  .center
    %a{:href => "javascript:history.back()"} Back
  EOF
        end # }}}

        template :sublets do # {{{
          <<EOF
  #box
    %h2= "All sublets (%d)" % [ @list.size ]

    %ul
      -@list.each do |s|
        %li
          %a#download{:href => "/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]
          %span.gray= "(%d downloads)" % [ s.downloads ]
  #clear

  .center
    %a{:href => "javascript:history.back()"} Back
  EOF
        end # }}}

        template :search do # {{{
          <<EOF
  #box
    %h2
      Search for
      %span.italic= @query

    %ul
      -@list.each do |s|
        %li
          %a#download{:href => "/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]
          %span.gray= "(%d downloads)" % [ s.downloads ]
  #clear

  .center
    %a{:href => "javascript:history.back()"} Back
  EOF
        end # }}}

        get "/" do # {{{
          @newest = Sur::Model::Sublet.all(:order => [ :created_at.desc ], :limit => 10)
          @most   = Sur::Model::Sublet.all(:downloads.gte => 0, :order => [ :downloads.desc ], :limit => 10)
          @never  = Sur::Model::Sublet.all(:downloads => 0, :order => [ :created_at.asc ], :limit => 4)
          @worst  = Sur::Model::Sublet.all(:annotations.gt => 0, :order => [ :annotations.asc ], :limit => 4)

          haml(:index)
        end # }}}

        get "/get/:digest" do # {{{
          if((s = Sur::Model::Sublet.first(:digest => params[:digest])) && File.exist?(s.path))

            s.downloads = s.downloads + 1
            s.save

            send_file(s.path, :type => "application/x-tar", :filename => File.basename(s.path))
          else
            puts ">>> WARNING: Couldn't find sublet with digest `#{params[:digest]}`"
            error = 404
          end
        end # }}}

        get "/list" do # {{{
          # Check cache age
          if(!File.exist?(CACHE) or (86400 < (Time.now - File.new(CACHE).ctime)))
            build_sublets
          end

          send_file(CACHE, :type => "text/plain", :last_modified => File.new(CACHE).ctime)
        end # }}}

        get "/tag/:tag" do # {{{
          @tag  = params[:tag].capitalize
          @list = Sur::Model::Sublet.all(Sur::Model::Sublet.tags.tag.name => params[:tag])

          haml(:tag)
        end # }}}

        get "/sublets" do # {{{
          @list = Sur::Model::Sublet.all(:order => [ :name.asc ])

          haml(:sublets)
        end # }}}

        get "/search" do # {{{
          @query = params[:query]
          @list  = Sur::Model::Sublet.all(:name.like => params[:query].gsub(/\*/, "%"))

          haml(:search)
        end # }}}

        post "/annotate" do # {{{
          if((s = Sur::Model::Sublet.first(:digest => params[:digest])))
            # Check if user exists
            if(!u = Sur::Model::User.first(:name => params[:user]))
              u = Sur::Model::User.new(
                :name       => user,
                :created_at => Time.now
              )

              raise if(!u.save)
              u.update

              puts ">>> Added user `#{u.name}`"
            end

            a = Sur::Model::Assoc::Annotate.new(
              :sublet_id  => s.id,
              :user_id    => u.id,
              :created_at => Time.now
            )
            raise if(!a.save)

            s.annotations = s.annotations + 1
            s.save

            puts ">>> Added annotation from `#{u.name}` for `#{s.name}'"
          else
            puts ">>> WARNING: Couldn't find sublet with digest `#{params[:digest]}`"
            error = 404
          end
        end # }}}

        post "/submit" do # {{{
          if(params[:file] && params[:file][:tempfile] && params[:user])
            file = params[:file][:tempfile]
            spec = Sur::Specification.extract_spec(file.path)
            mesg = ""

            # Validate spec
            if(spec.valid?)
              begin
                # Add or find sublet
                if(!(s = Sur::Model::Sublet.first(:name => spec.name, :version => spec.version)))
                  digest = Digest::MD5.hexdigest(File.read(file.path))
                  path   = File.join(REPOSITORY, spec.to_s + ".sublet")
                  ip     = request.env['REMOTE_ADDR'][/.[^,]+/]

                  s = Sur::Model::Sublet.new(
                    :name        => spec.name,
                    :contact     => spec.contact,
                    :description => spec.description,
                    :version     => spec.version,
                    :date        => spec.date,
                    :path        => path,
                    :digest      => digest,
                    :ip          => ip,
                    :subtlext    => spec.subtlext_version,
                    :sur         => spec.sur_version,
                    :created_at  => Time.now
                  )
                  raise if(!s.save)
                  s.update

                  puts ">>> Added sublet `#{s.name}'"
                end

                # Add or find user
                if(!(u = Sur::Model::User.first(:name => params[:user])))
                  u = Sur::Model::User.new(
                    :name => params[:user]
                  )
                  raise if(!u.save)
                  u.update

                  puts ">>> Added user `#{params[:user]}'"
                end

                # Parse authors
                spec.authors.each do |user|
                  # Check if user exists
                  if(!u = Sur::Model::User.first(:name => user))
                    u = Sur::Model::User.new(
                      :name       => user,
                      :created_at => Time.now
                    )

                    raise if(!u.save)
                    u.update

                    puts ">>> Added user `#{u.name}`"
                  end

                  # Check if user assoc exists
                  if(!(assoc = Sur::Model::Assoc::Author.first(
                      :user_id   => u.id,
                      :sublet_id => s.id)))
                    assoc = Sur::Model::Assoc::Author.new(
                      :user_id     => u.id,
                      :sublet_id   => s.id,
                      :created_at  => Time.now
                    )
                    raise if(!assoc.save)

                    puts ">>> Added user `#{u.name}` to `#{s.name}'"
                  end
                end

                # Parse tags
                spec.tags.each do |tag|
                  # Check if tag exists
                  if(!t = Sur::Model::Tag.first(:name => tag))
                    t = Sur::Model::Tag.new(
                      :name       => tag,
                      :created_at => Time.now
                    )

                    puts ">>> Added tag `#{t.name}`"

                    raise if(!t.save)
                    t.update
                  end

                  # Check if tag assoc exists
                  if(!(assoc = Sur::Model::Assoc::Tag.first(
                      :tag_id    => t.id,
                      :sublet_id => s.id)))
                    assoc = Sur::Model::Assoc::Tag.new(
                      :tag_id      => t.id,
                      :sublet_id   => s.id,
                      :created_at  => Time.now
                    )
                    raise if(!assoc.save)

                    puts ">>> Added tag `#{t.name}` to `#{s.name}'"
                  end
                end

                FileUtils.copy(file.path, path)
                build_sublets

                # Post via identi.ca
                unless(USERNAME.empty? && PASSWORD.empty?)
                  c = Curl::Easy.perform("http://identi.ca/api/statuses/update.xml")

                  c.userpwd = "%s:%s" % [ USERNAME, PASSWORD ]
                  c.http_post(
                    Curl::PostField.content("status",
                      "New sublet: %s (%s) !subtle" % [ spec.name, spec.version ]
                    )
                  )
                end
              rescue
                error = 406
              end
            else
              error = 406
            end
          else
            error = 415
          end
        end # }}}

      post "/xmlrpc" do # {{{
        xml = @request.body.read

        if(xml.empty?)
          error = 400
          return
        end

        # Parse xml
        method, arguments = XMLRPC::Marshal.load_call(xml)
        method = method.gsub(/([A-Z])/, '_\1').downcase

        # Check if method exists
        if(respond_to?(method))
          content_type("text/xml", :charset => "utf-8")
          send(method, arguments)
        else
          error = 404
        end
      end # }}}

       Sinatra::Application.run!
      end # }}}

    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
