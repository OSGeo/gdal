use strict;
use warnings;
use v5.10;
use Scalar::Util 'blessed';
use Test::More tests => 5;
BEGIN { use_ok('Geo::GDAL') };

# test GCPs

my $ds = Geo::GDAL::Driver('GTiff')->Create(Name=>'/vsimem/tmp.tiff', Bands=>1, Width=>312);
my ($w, $h) = $ds->Size;
ok($w == 312, "set width ok");

my $t = Geo::GDAL::GeoTransform->new(1.23,1,0, 1,0,1);
$t = $ds->GeoTransform($t);
ok(sprintf("%.2f", $t->[0]) eq '1.23', "geotransform ok");

my $xmin = $t->[0];
my $ymin = $t->[3] + $t->[5]*$h;
my $xmax = $t->[0] + $t->[1]*$w;
my $ymax = $t->[3];
#say "extent $xmin $ymin $xmax $ymax";

my @gcp;
$gcp[1] = Geo::GDAL::GCP->new($xmin, $ymax, 0, 0, 0);
$gcp[0] = Geo::GDAL::GCP->new($xmax, $ymax, 0, $w, 0);
$gcp[3] = Geo::GDAL::GCP->new($xmin, $ymin, 0, 0, $h);
$gcp[2] = Geo::GDAL::GCP->new($xmax, $ymin, 0, $w, $h);
my $gt = Geo::GDAL::GeoTransform::FromGCPs(\@gcp, 0);

#say "transform: @$gt";
ok(sprintf("%.2f", $t->[0]) eq '1.23', "geotransform from gcps ok");

$gt = Geo::GDAL::GeoTransform::FromGCPs(@gcp, 0);

#say "transform: @$gt";
ok(sprintf("%.2f", $t->[0]) eq '1.23', "geotransform from gcp array ok");

