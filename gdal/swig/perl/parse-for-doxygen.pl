use strict;
use warnings;
use Modern::Perl;

my @pm = qw(lib/Geo/GDAL.pm lib/Geo/OGR.pm lib/Geo/OSR.pm lib/Geo/GDAL/Const.pm lib/Geo/GNM.pm);

my %internal_methods = map {$_=>1} qw/TIEHASH CLEAR FIRSTKEY NEXTKEY FETCH STORE
                                      DESTROY DISOWN ACQUIRE RELEASE_PARENTS
                                      UseExceptions DontUseExceptions this AllRegister RegisterAll
                                      callback_d_cp_vp/;
my %private_methods = map {$_=>1} qw/PushErrorHandler PopErrorHandler Error ErrorReset
                                     GetLastErrorNo GetLastErrorType GetLastErrorMsg/;
my %constant_prefixes = map {$_=>1} qw/DCAP_/;

my %package;
my $package;
my $sub;
my $attr;
for my $pm (@pm) {
    open(my $fh, "<", $pm) or die "cannot open < $pm: $!";
    while (<$fh>) {
        chomp;
        my $code = $_;
        s/^\s+//;
        next if $_ eq '';
        next if $_ =~ /^#####/; # skip swig comments
        my($w) = /^(\S+)\s/;
        $w //= '';
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
            next if $sub eq ''; # skip anonymous subs
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
        if ($package and /\@ISA/ and /=/) {
            my $isa = $_;
            $isa =~ s/our //;
            $isa =~ s/\@ISA//;
            $isa =~ s/=//;
            $isa =~ s/qw//;
            $isa =~ s/\(//;
            $isa =~ s/\)//;
            $isa =~ s/\///g;
            $isa =~ s/;//;
            my @isa = split /\s+/, $isa;
            for my $isa (@isa) {
                next if $isa eq '';
                push @{$package{$package}{isas}}, $isa;
            }
        }
        #print "sub=$sub, $_\n";
        if ($sub) {
            push @{$package{$package}{code}{$sub}}, $code;
            next;
        }
        if ($attr) {
            push @{$package{$package}{code}{$attr}}, $code;
            $attr = '' if /;/;
            next;
        }
    }
    close $fh;
}

my @dox = qw(lib/Geo/GDAL.dox lib/Geo/OGR.dox lib/Geo/OSR.dox lib/Geo/GNM.dox);

for my $dox (@dox) {
    open(my $fh, "<", $dox) or die "cannot open < $dox: $!";
    while (<$fh>) {
        chomp;
        next if $_ eq '';
        s/^[#]+//;
        s/^ //;
        my ($w) = /^(\S+)\s/;
        $w //= '';
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
            $sub =~ s/\s+$//;
            #delete $package{$package}{subs}{$sub};
            $package{$package}{dox}{$sub}{d} = $sub;
            $package{$package}{dox}{$sub}{at} = $w;
            $package{$package}{dox}{$sub}{ignore} = 1;
            next;
        }
        if ($w eq '@ignore_class') {
            my $class = $_;
            $class =~ s/^(\S+)\s+//;
            $package{$class}{ignore} = 1;
            next;
        }
        if ($w eq '@cmethod' or $w eq '@method' or $w eq '@sub') {
            $sub = $_;
            $sub =~ s/^(\S+)\s+//;
            $sub =~ s/\s+$//;
            my $d = $sub;
            if (/(\w+)\(/) {
                $sub = $1;
            } elsif (/(\w+)$/) {
                $sub = $1;
            } else {
                print STDERR "sub?: $_\n";
            }
            $package{$package}{dox}{$sub}{d} = $d;
            $package{$package}{dox}{$sub}{at} = $w;
            $attr = '';
            next;
        }
        if ($w eq '@attr') {
            $attr = $_;
            $attr =~ s/^(\S+)\s+//;
            $attr =~ s/\s*list\s+/@/;
            $attr = '$'.$attr unless $attr =~ /^@/;;
            my $d = $attr;
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

#use Data::Dumper;
#print Dumper(%package);
#exit;

for my $package (sort keys %package) {
    next if $package eq '';
    next if $package eq 'Geo::GDAL::Const';
    next if $package{$package}{ignore};
    for my $sub (sort keys %{$package{$package}{dox}}) {
        next if $sub =~ /^\$/;
        if ($package{$package}{dox}{$sub} and not $package{$package}{subs}{$sub}) {
            print STDERR "Warning: non-existing $package::$sub documented.\n";
        }
    }
    print "#** \@class $package\n";
    # package may have brief, details, todo, isa
    for my $l (@{$package{$package}{package_dox}}) {
        print "# $l\n";
    }
    print "#*\n";
    print "package $package;\n\n";

    print "use base qw(",join(' ', @{$package{$package}{isas}}),")\n\n" if $package{$package}{isas};

    for my $attr (sort keys %{$package{$package}{attrs}}) {
        next if $package{$package}{dox}{$attr}{ignore};
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
        next if $package{$package}{dox}{$sub}{ignore};
        next if $sub =~ /^_/; # no use showing these
        next if $sub =~ /swig_/; # skip attribute setters and getters
        next if $sub =~ /GDAL_GCP_/; # skip GDAL::GCP package subroutines from class GDAL

        next if $sub =~ /RELEASE_PARENT/;

        next if $sub =~ /GT_/; # done in methods geometry type test and modify

        # processed constants (Const.pm is not given to Doxygen at all)
        # to do: GF_, GRIORA_, GPI_, OF_, DMD_, CPLES_, GMF_, GARIO_, GTO_
        # OLMD_
        # SRS_PM_, SRS_WGS84_
        next if $sub =~ /^wkb/;
        next if $sub =~ /^OFT/;
        next if $sub =~ /^OFST/;
        next if $sub =~ /^OJ/;
        next if $sub =~ /^ALTER_/;
        next if $sub =~ /^F_/;
        next if $sub =~ /^OLC/;
        next if $sub =~ /^ODsC/;
        next if $sub =~ /^ODrC/;
        next if $sub =~ /^SRS_PT_/;
        next if $sub =~ /^SRS_PP_/;
        next if $sub =~ /^SRS_UL_/;
        next if $sub =~ /^SRS_UA_/;
        next if $sub =~ /^SRS_DN_/;

        my $at = $package{$package}{dox}{$sub}{at} // '';
        next if $internal_methods{$sub} && !$at; # skip non-documented internal methods

        my $d = $package{$package}{dox}{$sub}{d};
        my $nxt = 0;
        for my $prefix (keys %constant_prefixes) {
            $nxt = 1 if $sub =~ /^$prefix/;
        }
        next if $nxt;
        $d = $sub unless $d;
        $d =~ s/^\$/scalar /;
        $d =~ s/^\\\$/scalar reference /;
        $d =~ s/^\@/list /;
        $d =~ s/^\\\@/array reference /;
        $d =~ s/^\%/hash /;
        $d =~ s/^\\\%/hash reference /;
        my $dp = $d;
        $dp .= '()' unless $dp =~ /\(/;
        print "#** \@method $dp\n";
        if ($private_methods{$d} or $at eq '@ignore') {
            print "# Undocumented method, do not call unless you know what you're doing.\n";
            print "# \@todo Test and document this method.\n";
        }
        if ($at eq '@cmethod') {
            print "# Class method.\n";
        }
        elsif ($at eq '@sub') {
            print "# Package subroutine.\n";
        }
        elsif ($at eq '@method') {
            print "# Object method.\n";
        }
        for my $c (@{$package{$package}{dox}{$sub}{c}}) {
            if ($c =~ /^\+list/) {
                $c =~ s/\+list //;
                my($pkg, $prefix, $exclude) = split / /, $c;
                my %exclude;
                %exclude = map {$_=>1} split /,/, $exclude if $exclude;
                my @list;
                for my $l (sort keys %{$package{$pkg}{subs}}) {
                    next unless $l =~ /^$prefix/;
                    $l =~ s/^$prefix//;
                    next if $exclude{$l};
                    push @list, $l;
                }
                my $last = pop @list;
                print "# ",join(', ', @list),", and $last.\n";
            } else {
                print "# $c\n";
            }
        }
        print "#*\n";
        print "sub $sub {\n";
        my $code = $package{$package}{code}{$sub};
        fix_indentation($code);
        pop @$code if $code->[$#$code] && $code->[$#$code] =~ /^\s*}\s*$/; # remove duplicate ending } of the sub
        for my $l (@$code) {
            print "$l\n";
        }
        print "}\n\n";
    }
}

sub fix_indentation {
    my $code = shift;
    return unless $code && @$code;
    my($space) = $code->[0] =~ /^(\s*)/;
    my $l = length($space);
    if ($l < 4) {
        for (@$code) {
            for my $i ($l..4) {
                $_ = ' '.$_;
            }
        }
    } elsif ($l > 4) {
        for (@$code) {
            for my $i (4..$l) {
                $_ =~ s/^ //;
            }
        }
    }
}
