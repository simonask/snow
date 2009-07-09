#!/usr/bin/env ruby

$CC = ENV['CC']
$CFLAGS = ENV['CFLAGS']

names = ARGV.sort

names.each do |name|
  next if name == "test"
  target = name
  source = "#{name}.c"
  
  if !File.exist?(target) || File.mtime("test.c") > File.mtime(target) || File.mtime(source) > File.mtime(target)
    command = "#{$CC} #{$CFLAGS} -o #{name} test.c #{name}.c"
    puts command
    puts `#{command}`
    exitcode = $?
    if $? != 0
      $stderr.puts "Failed to build '#{name}'! Aborting..."
      exit $?
    end
  end
end
