my @pm = qw(lib/Geo/GDAL.pm lib/Geo/OGR.pm lib/Geo/OSR.pm lib/Geo/GDAL/Const.pm);

my %package;
my $package;
my $sub;
my $attr;
for my $pm (@pm) {
    open(my $fh, "<", $pm) or die "cannot open < $pm: $!";
    while (<$fh>) {
        chomp;
        s/^\s+//;
        my($w) = /^(\S+)\s/;
        if ($w eq 'package') {
            $package = $_;
            $package =~ s/^(\S+)\s+//;
            $package =~ s/;.*//;
            $sub = '';
            $attr = '';
            next;
        }
        if ($w eq 'sub') {
            $sub = $_;
            $sub =~ s/^(\S+)\s+//;
            $sub =~ s/\W.*//;
            $package{$package}{subs}{$sub} = 1;
            $attr = '';
            next;
        }
        if ($w =~ /^\*/) {
            $sub = $w;
            $sub =~ s/^\*//;
            $sub =~ s/\W.*//;
            $package{$package}{subs}{$sub} = 1;
            $attr = '';
            next;
        }
        if (!$sub and $w =~ /^[\$@\%]/ and /=/) {
            $attr = $w;
            $attr =~ s/^[\$@\%]//;
            $attr =~ s/\W.*//;
            #print "attr: $attr\n";
            $package{$package}{attr}{$attr} = 1;
            $sub = '';
        }
        if (/use base/) {
            #print "$_\n";
        }
        if (/\@ISA/ and /=/) {
            my $isa = $_;
            $isa =~ s/\@ISA//;
            $isa =~ s/=//;
            $isa =~ s/qw//;
            $isa =~ s/\(//;
            $isa =~ s/\)//;
            $isa =~ s/;//;
            @isa = split /\s+/, $isa;
            for my $isa (@isa) {
                next if $isa eq '';
                push @{$package{$package}{isas}}, $isa;
            }
        }
        #print "sub=$sub, $_\n";
        if ($sub) {
            push @{$package{$package}{code}{$sub}}, $_;
            next;
        }
        if ($attr) {
            push @{$package{$package}{code}{$attr}}, $_;
            $attr = '' if /;/;
            next;
        }
    }
    close $fh;
}

my @dox = qw(lib/Geo/GDAL.dox lib/Geo/OGR.dox lib/Geo/OSR.dox lib/Geo/GDAL/Const.dox);

my $package;
my $sub;
my $attr;
for my $dox (@dox) {
    open(my $fh, "<", $dox) or die "cannot open < $dox: $!";
    while (<$fh>) {
        chomp;
        s/^[\s#]+//;
        next if $_ eq '';
        ($w) = /^(\S+)\s/;
        if ($w eq '@class') {
            $package = $_;
            $package =~ s/^(\S+)\s+//;
            $attr = '';
            $sub = '';
            next;
        }
        if ($w eq '@isa') {
            next;
        }
        if ($w eq '@ignore') {
            $sub = $_;
            $sub =~ s/^(\S+)\s+//;
            delete $package{$package}{subs}{$sub};
            next;
        }
        if ($w eq '@cmethod' or $w eq '@method') {
            $sub = $_;
            $sub =~ s/^(\S+)\s+//;
            $d = $sub;
            if (/(\w+)\(/) {
                $sub = $1;
            } elsif (/(\w+)$/) {
                $sub = $1;
            } else {
                print STDERR "sub?: $_\n";
            }
            $package{$package}{dox}{$sub}{d} = $d;
            $attr = '';
            next;
        }
        if ($w eq '@attr') {
            $attr = $_;
            $attr =~ s/^(\S+)\s+//;
            $attr =~ s/\s*list\s+/@/;
            $attr = '$'.$attr unless $attr =~ /^@/;;
            $d = $attr;
            $attr =~ s/@//;
            #print "attr: '$d'\n";
            $package{$package}{attrs}{$attr} = 1;
            $package{$package}{dox}{$attr}{d} = $d;
            $sub = '';
            next;
        }
        if ($sub) {
            push @{$package{$package}{dox}{$sub}{c}}, $_;
            next;
        }
        if ($attr) {
            push @{$package{$package}{dox}{$attr}{c}}, $_;
            next;
        }
        if ($package) {
            push @{$package{$package}{package_dox}}, $_;
            next;
        }
    }
    close $fh;
}

for my $package (sort keys %package) {
    next if $package eq '';
    print "#** \@class $package\n";
    for my $l (@{$package{$package}{package_dox}}) {
        print "# $l\n";
    }
    print "#*\n";
    print "package $package;\n\n";

    print "use base qw(",join(' ', @{$package{$package}{isas}}),")\n\n";

    for my $attr (sort keys %{$package{$package}{attrs}}) {
        my $d = $package{$package}{dox}{$attr}{d};
        $d = $attr unless $d;
        print "#** \@attr $d \n";
        for my $c (@{$package{$package}{dox}{$attr}{c}}) {
            print "# $c\n";
        }
        print "#*\n";
        for my $l (@{$package{$package}{code}{$attr}}) {
            print "$l\n";
        }
        print "\n";
    }

    for my $sub (sort keys %{$package{$package}{subs}}) {
        my $d = $package{$package}{dox}{$sub}{d};
        $d = $sub unless $d;
        print "#** \@method $d\n";
        for my $c (@{$package{$package}{dox}{$sub}{c}}) {
            print "# $c\n";
        }
        print "#*\n";
        print "sub $sub {\n";
        my $code = $package{$package}{code}{$sub};
        pop @{$code} if $code->[$#$code] eq '}';
        for my $l (@{$package{$package}{code}{$sub}}) {
            print "    $l\n";
        }
        print "}\n\n";
    }
}
