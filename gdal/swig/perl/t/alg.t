use strict;
use warnings;
use v5.10;
use Scalar::Util 'blessed';
use Test::More tests => 9;
BEGIN { use_ok('Geo::GDAL') };

{
    my $b = Geo::GDAL::Driver('MEM')->Create(Width => 40, Height => 40)->Band;
    my $d = $b->ReadTile;
    for my $y (20..29) {
        for my $x (10..19) {
            $d->[$y][$x] = 1;
        }
    }
    $b->WriteTile($d);
    my $l = $b->Polygonize;
    my $c = $l->GetFeatureCount;
    ok($c == 2, "polygonize made 2 features");
    $l->ForFeatures(
        sub {
            my $f = shift;
            my $g = $f->Geometry;
            ok($g->AsText eq 
               'POLYGON ((0 0,0 -40,40 -40,40 0,0 0),(10 -20,20 -20,20 -30,10 -30,10 -20))', 
               'big geometry with hole') if $f->{val} == 0;
            ok($g->AsText eq 
               'POLYGON ((10 -20,10 -30,20 -30,20 -20,10 -20))', 
               'small geometry') if $f->{val} == 1;
        });
}

{
    my $b = Geo::GDAL::Driver('MEM')->Create(Width => 20, Height => 20)->Band;
    my $d = $b->ReadTile;
    for my $y (0..19) {
        for my $x (0..19) {
            $d->[$y][$x] = 5;
        }
    }
    $d->[15][4] = 1;
    $d->[10][13] = 1;
    $d->[11][13] = 1;
    $d->[2][4] = 1;
    $d->[3][4] = 1;
    $d->[3][5] = 1;
    $b->WriteTile($d);
    my $r = $b->Sieve(Threshold => 2, Options => {Connectedness => 4});
    $d = $r->ReadTile;
    ok($d->[15][4] == 5, "Sieved area with size 1 away.");
}

{
    my $d = Geo::GDAL::Driver('MEM')->Create(Width => 20, Height => 20, Bands => 3);
    $d->Band(1)->ColorInterpretation('RedBand');
    $d->Band(2)->ColorInterpretation('GreenBand');
    $d->Band(3)->ColorInterpretation('BlueBand');
    for my $b ($d->Bands) {
        my $d = $b->ReadTile;
        for my $y (0..19) {
            for my $x (0..19) {
                $d->[$y][$x] = int(rand(256));
            }
        }
        $b->WriteTile($d);
    }
    my $b = $d->Dither;
    my $ct = $b->ColorTable;
    my @n = $ct->ColorEntries;
    #say STDERR "@n";
    ok(@n == 256, "Dither using computed ct");
}

{
    my $b = Geo::GDAL::Driver('MEM')->Create(Width => 20, Height => 20, Type => 'Byte')->Band;
    my $d = $b->ReadTile;
    $d->[9][9] = 1;
    $b->WriteTile($d);
    my $b2 = $b->Distance(Options => {Type => 'Byte'});
    $d = $b2->ReadTile;
    ok($d->[0][0] == 13, "Distance raster");
}

{
    my $d = Geo::GDAL::Driver('MEM')->Create(Width => 20, Height => 20, Type => 'Byte');
    my $dest = '/vsimem/tmp';
    my $result;
    eval {
        $result = $d->Warp($dest, {to => 'SRC_METHOD=NO_GEOTRANSFORM'});
        $result = blessed($result);
    };
    ok($@ eq '' && $result && $result eq 'Geo::GDAL::Dataset', "Warp dataset ($result) $@");
}

{
    my $d = Geo::GDAL::Driver('MEM')->Create(Width => 20, Height => 20, Type => 'Byte');
    my $dest = '/vsimem/tmp';
    my $result;
    eval {
        $result = Geo::GDAL::Dataset::Warp([$d], $dest, {to => 'SRC_METHOD=NO_GEOTRANSFORM'});
        $result = blessed($result);
    };
    ok($@ eq '' && $result && $result eq 'Geo::GDAL::Dataset', "Warp datasets ($result) $@");
}
