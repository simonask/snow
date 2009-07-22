#!/usr/bin/env ruby

$o = $stdout

targets = Dir.glob(File.join(File.dirname(__FILE__), "*.c")).
            map { |file| File.basename(file).gsub(/^(.+)\.c$/, '\1') }.
            reject { |target| target == "test"}
all_sources = ["test.c"]

$o.puts "SUBDIRS = ../snow"
$o.puts "bin_PROGRAMS = #{targets.join(' ')}"
targets.each do |target|
  $o.puts "#{target}_SOURCES = #{target}.c #{all_sources.join(' ')}"
  $o.puts "#{target}_LDADD = ../snow/libsnow.la"
end
$o.puts
$o.puts "all: #{targets.join(' ')}"
$o.puts
$o.puts "test: all"
$o.puts "\texec ./runner.rb #{targets.join(' ')}"