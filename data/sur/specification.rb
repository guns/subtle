#
# @package sur
#
# @file Specification functions
# @author Christoph Kappel <unexist@dorfelite.net>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require "archive/tar/minitar"

require "rubygems"

module Subtle # {{{
  module Sur # {{{
    # Specification class for specs
    class Specification # {{{
      # Sublet name
      attr_accessor :name

      # Author list
      attr_accessor :authors

      # Contact address
      attr_accessor :contact

      # Sublet description
      attr_accessor :description

      # Sublet notes
      attr_accessor :notes

      # Sublet config notes
      attr_accessor :config

      # Package version
      attr_accessor :version

      # Date of creation
      attr_accessor :date

      # Tag list
      attr_accessor :tags

      # File list
      attr_accessor :files

      # Icon list
      attr_accessor :icons

      # Hash of dependencies
      attr_accessor :dependencies

      # Version of subtle
      attr_accessor :required_version

      # Path of spec
      attr_accessor :path

      # Digest
      attr_accessor :digest

      ## Sur::Specification::initialize {{{
      # Create a new Specification object
      #
      # @yield [self] Init block
      # @yieldparam [Object]  self  Self instance
      # @return [Object] New Specification
      #
      # @since 0.1
      #
      # @example
      #   Sur::Specification.new
      #   => #<Sur::Specification:xxx>

      def initialize
        @name             = "Unnamed"
        @authors          = []
        @dependencies     = {}
        @subtlext_version = nil
        @sur_version      = nil
        @files            = []
        @icons            = []
        @path             = Dir.pwd

        # Pass to block
        yield(self) if(block_given?)
      end # }}}

      ## Sur::Specification::load_spec {{{
      # Load Specification from file
      #
      # @param [String]  file  Specification file name
      # @return [Object] New Specification
      #
      # @raise [String] Loading error
      # @since 0.1
      #
      # @example
      #   Sur::Specification.load_file("sublet.spec")
      #   => #<Sur::Specification:xxx>

      def self.load_spec(file)
        spec = nil

        # Catch IO exceptions
        begin
          # Create spec
          spec      = eval(File.open(file).read)
          spec.path = File.dirname(file)
        rescue
          spec = nil
        end

        # Check object
        if(spec.nil? || !spec.is_a?(Specification) || !spec.valid?)
          raise "Couldn't load specification file `#{file}'"
        end

        spec
      end # }}}

      ## Sur::Specification::extract_spec {{{
      # Extract and load Specification from file
      #
      # @param [String]  file  Tar file name
      # @return [Object] New Specification
      #
      # @raise [String] Loading error
      # @see Sur::Specification.load_spec
      # @since 0.1
      #
      # @example
      #   Sur::Specification.extract_spec("foo.sublet")
      #   => #<Sur::Specification:xxx>

      def self.extract_spec(file)
        spec = nil

        begin
          File.open(file, "rb") do |input|
            Archive::Tar::Minitar::Input.open(input) do |tar|
              tar.each do |entry|
                if(".spec" == File.extname(entry.full_name))
                  spec = eval(entry.read)
                end
              end
            end
          end
        rescue
          spec = nil
        end

        # Check object
        if(spec.nil? || !spec.is_a?(Specification) || !spec.valid?)
          raise "Couldn't extract specification from `#{file}'"
        end

        spec
      end # }}}

      ## Sur::Specification::template {{{
      # Create a new Specification object
      #
      # @param [String]  file  Template name
      #
      # @since 0.1
      #
      # @example
      #   Sur::Specification.template("foobar")
      #   => nil

      def self.template(file)
        require "subtle/sur/version"

        # Build filenname
        ext     = File.extname(file)
        ext     = ".rb" if(ext.empty?)
        name    = File.basename(file, ext)

        folder  = File.join(Dir.pwd, name)
        spec    = File.join(folder, name + ".spec")
        sublet  = File.join(folder, name + ".rb")

        if(!File.exist?(name))
          FileUtils.mkdir_p([ folder ])

          # Create spec
          File.open(spec, "w+") do |output|
            output.puts <<EOF
# -*- encoding: utf-8 -*-
# #{name.capitalize} specification file
# Created with sur-#{Subtle::Sur::VERSION}
Sur::Specification.new do |s|
  s.name        = "#{name.capitalize}"
  s.authors     = [ "#{ENV["USER"]}" ]
  s.date        = "#{Time.now.strftime("%a %b %d %H:%M %Z %Y")}"
  s.contact     = "YOUREMAIL"
  s.description = "SHORT DESCRIPTION"
  s.notes       = <<NOTES
LONG DESCRIPTION
NOTES
  s.version     = "0.0"
  s.tags        = [ ]
  s.files       = [ "#{name}.rb" ]
  s.icons       = [ ]

  # Version requirements
  # s.required_version = "0.9.2127"
  # s.add_dependency("subtle", "~> 0.1.2")
end
EOF
          end

          # Create sublet
          File.open(sublet, "w+") do |output|
            output.puts <<EOF
# #{name.capitalize} sublet file
# Created with sur-#{Subtle::Sur::VERSION}
configure :#{name} do |s|
  s.interval = 60
end

on :run do |s|
  s.data =
end
EOF
          end

          puts ">>> Created template for `#{name}'"
        else
          raise "File `%s' already exists" % [ name ]
        end
      end # }}}

      ## Sur::Specification::valid? {{{
      # Checks if a specification is valid
      #
      # @return [true, false] Validity of the package
      #
      # @since 0.1
      #
      # @example
      #   Sur::Specification.new.valid?
      #   => true

      def valid?
        (!@name.nil? && !@authors.empty? && !@contact.nil? && \
          !@description.nil? && !@version.nil? && !@files.empty?)
      end # }}}

      ## Sur::Specification::validate {{{
      # Check if a specification is valid
      #
      # @raise [String] Validity error
      # @since 0.1
      #
      # @example
      #   Sur::Specification.new.validate
      #   => nil

      def validate
        fields = []

        # Check fields of spec
        fields.push("name")        if(@name.nil?)
        fields.push("authors")     if(@authors.empty?)
        fields.push("contact")     if(@contact.nil?)
        fields.push("description") if(@description.nil?)
        fields.push("version")     if(@version.nil?)
        fields.push("files")       if(@files.empty?)

        unless(fields.empty?)
          raise "Couldn't find `#{fields.join(", ")}' in specification"
        end
      end # }}}

      ## Sur::Specification::add_dependency {{{
      # Add a gem dependency to the package
      #
      # @param [String]  name     Dependency name
      # @param [String]  version  Required version
      #
      # @since 0.1
      #
      # @example
      #   Sur::Specification.new.add_dependency("subtle", "0.8")
      #   => nil

      def add_dependency(name, version)
        @dependencies[name] = version
      end # }}}

      ## Sur::Specification::satisfied? {{{
      # Check if all dependencies are satisfied
      #
      # @return [true, false] Validity of the package
      #
      # @since 0.1
      #
      # @example
      #   Sur::Specification.new.satisfied?
      #   => true

      def satisfied?
        satisfied = true
        missing   = []

        # Check subtlext version
        unless(@required_version.nil?)
          begin
            require "subtle/subtlext"

            # Check version
            major_have, minor_have, teeny_have = Subtlext::VERSION.split(".").map(&:to_i)
            major_need, minor_need, teeny_need = @required_version.split(".").map(&:to_i)

            if(major_need > major_have or minor_need > minor_have or
               teeny_need.nil? or teeny_have.nil? or teeny_need > teeny_have)
              puts ">>> ERROR: Need at least subtle >= #{@required_version}"

              satisfied = false
            end
          rescue => err
            puts ">>> ERROR: Failed checking version of subtlext"

            satisfied = false
          end
        end

        # Just try to load the gem and catch errors
        @dependencies.each do |k, v|
          begin
            gem(k, v)
          rescue Gem::LoadError
            missing.push("%s (%s)" % [ k, v])

            satisfied = false
          end
        end

        unless(missing.empty?)
          puts ">>> ERROR: Following gems are missing: #{missing.join(", ")}"
        end

        satisfied
      end # }}}

      ## Sur::Specification::to_str {{{
      # Convert Specification to string
      #
      # @return [String] Specification as string
      #
      # @since 0.1
      #
      # @example
      #   Sur::Specification.new.to_str
      #   => "sublet-0.1"

      def to_str
        "#{@name}-#{@version}".downcase
      end # }}}

      # Aliases
      alias :to_s :to_str

      # FIXME: Deprecated
      alias :subtlext_version :required_version
      alias :subtlext_version= :required_version=
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker
