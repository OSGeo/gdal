use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# test utilities

# named_parameters
# call with a list

{
    my $ret = Geo::GDAL::named_parameters([2], a => 1, b => undef, c => 'abc');
    ok($ret->{a} == 2, "named_parameters, list, new value");
    ok($ret->{c} eq 'abc', "named_parameters, list, default value");
}

# call with a hash ref

{
    my $ret = Geo::GDAL::named_parameters([{a => 2}], a => 1, b => undef, c => 'abc');
    ok($ret->{a} == 2, "named_parameters, hash ref, new value $ret->{a}");
    ok($ret->{c} eq 'abc', "named_parameters, list, default value");
}

# call with a hash

{
    my $ret = Geo::GDAL::named_parameters([a => 2], a => 1, b => undef, c => 'abc');
    ok($ret->{a} == 2, "named_parameters, hash, new value");
    ok($ret->{c} eq 'abc', "named_parameters, list, default value");
}


# string2int
my %int2string = (1 => 'a', 2 => 'b', 3 => 'c');
my %string2int = (a => 1, b => 2, c => 3);

# undef is returned
{
    my $a = undef;
    my $b = Geo::GDAL::string2int($a, \%string2int, \%int2string);
    ok(!defined $b, "string2int undef is returned");
}

# default is returned
{
    my $a = undef;
    my $b = Geo::GDAL::string2int($a, \%string2int, \%int2string, 'b');
    ok($b == 2, "string2int default is returned");
}

# return given if known int
{
    my $a = 2;
    my $b = Geo::GDAL::string2int($a, \%string2int, \%int2string);
    ok($b == 2, "string2int return given if known int");
}

# return string converted to int if known
{
    my $a = 'c';
    my $b = Geo::GDAL::string2int($a, \%string2int, \%int2string);
    ok($b == 3, "string2int return string converted to int if known");
}

# error if string is not known
{
    my $a = 'x';
    my $b;
    eval {
        $b = Geo::GDAL::string2int($a, \%string2int, \%int2string);
    };
    ok($@, "string2int error if string is not known");
}

