pass = 0.0
fail = 0.0

1000.times do
  `rm -f build/tests/vm/page-parallel.result`
  `rm -f build/tests/vm/page-parallel.output`
  s = `make build/tests/vm/page-parallel.result`
  passed = /pass tests\/vm\/page-parallel/ === s
  if passed
    pass += 1
    print "passed! "
  else
    puts s
    fail += 1
    print "FAIL! "
  end
  puts ("%03.2f%% passed" % ((pass / (pass + fail)) * 100))
end