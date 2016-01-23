use strict;
use warnings;
use v5.10;
use Scalar::Util 'blessed';
use Test::More tests => 6;
BEGIN { use_ok('Geo::GDAL') };

my $e;

eval {
    my $ds = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/tmp', Bands => 0);
};
ok(($@ =~ /called at t\/error.t/), "Error in GDAL is confessed (by a call in cpl_exceptions.i).");

eval {
    Geo::GDAL::VSIF::MkDir('/cannot/make/this');
};
ok(($@ =~ /called at t\/error.t/), "Error in bindings is confessed (by a call in typemaps_perl.i).");

Geo::GDAL->errstr;
$e = Geo::GDAL->errstr;
ok($e eq '', 'Calling Geo::GDAL::errstr clears the error stack.');

eval {
    Geo::GDAL::GetDataTypeSize('bar');
};
ok(($@ =~ /called at t\/error.t/), "Error in Perl layer is confessed (by a call to Geo::GDAL::error in {gdal,ogr,osr}_perl.i).");

$e = Geo::GDAL->errstr;
ok($@ =~ /$e/, 'Pure errors can be obtained from Geo::GDAL::errstr');
