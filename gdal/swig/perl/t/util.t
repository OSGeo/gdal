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
