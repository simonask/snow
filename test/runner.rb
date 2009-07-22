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

def blue(str)
  "\x1b[1;36m#{str}#{RESET_COLOR}"
end

names = ARGV.sort

passed = 0
pending = 0
failed = 0

names.each do |name|
  next if name == "test"
  next unless File.exist?(name)
  
  puts blue("#{name.upcase}")
  command = "./#{name}"
  system(command)
  exitcode = $?
  if exitcode == 0
    passed += 1
  elsif exitcode == 2
    pending += 1
  else
    failed += 1
    likely = nil
    if exitcode == 5
      likely = "looks like a failed assert or a debug trap"
    end
    likely = " (#{likely})" if likely
    puts "#{red('FAILED')} with exitcode #{exitcode}#{likely}."
  end
  puts
end
puts("#{passed} #{passed == 1 ? 'suite' : 'suites'} passed, #{failed} failed, #{pending} pending")