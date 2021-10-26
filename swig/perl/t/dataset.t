use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

my $d = Geo::GDAL::Driver('MEM')->Create();

my $e = $d->Extent;

is_deeply($e, [0,0,256,256], "Default extent is default size.");

my $g = $d->GeoTransform;

is_deeply($g, [0,1,0,0,0,1], "Default transform.");

$e = $d->Extent(10,20,10,20);

is_deeply($e, [10,20,20,40], "Extent of a tile.");

my @tile = $d->Tile($d->Extent);

is_deeply(\@tile, [0,0,256,256], "As one tile.");

@tile = $d->Tile($d->Extent(10,20,10,20));

is_deeply(\@tile, [10,20,10,20], "Subtile.");

eval {
    $d = Geo::GDAL::Driver('MEM')->Create(Width => 12.3);
};
ok($@ =~ /^TypeError/, "Catch swig exceptions in error messages.");
