
sub check_vtrr {
    our ($test);
    my (@output) = read_text_file ("$test.output");
    common_checks ("run", @output);

    @output = get_core_output ("run", @output);

    if (!(grep (/PASS$/, @output))) {
        foreach (@output) {
            print $_;
            print "\n";
        }
        fail "missing PASS in output";
    }
    pass;
}

1;
