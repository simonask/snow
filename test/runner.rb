#!/usr/bin/env ruby

RESET_COLOR = "\x1b[0m"

def red(str)
  "\x1b[1;31m#{str}#{RESET_COLOR}"
end

def yellow(str)
  "\x1b[1;33m#{str}#{RESET_COLOR}"
end

def green(str)
  "\x1b[1;32m#{str}#{RESET_COLOR}"
end

names = ARGV.sort

passed = 0
pending = 0
failed = 0

names.each do |name|
  next if name == "test"
  next unless File.exist?(name)
  
  print name.ljust(60)
  command = "./#{name}"
  `#{command}`
  exitcode = $?
  if exitcode == 0
    passed += 1
    puts green("OK")
  elsif exitcode == 2
    pending += 1
    puts yellow("PENDING")
  else
    failed += 1
    puts "#{red('FAILED')} (code: #{exitcode})"
  end
end

puts("#{passed} passed, #{failed} failed, #{pending} pending.")