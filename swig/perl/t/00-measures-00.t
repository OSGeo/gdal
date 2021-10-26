use strict;
use warnings;
use bytes;
use v5.10;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# test geometry types

my @t = Geo::OGR::Geometry->GeometryTypes;

for my $geom (@t) {
    next if $geom eq 'Unknown';
    next if $geom eq 'None';

    my $i = Geo::GDAL::s2i(geometry_type => $geom);

    my $j = Geo::OGR::GT_Flatten($i);
    ok(!Geo::OGR::GT_HasZ($j), "$geom, no Z after GT_Flatten");
    ok(!Geo::OGR::GT_HasM($j), "$geom, no M after GT_Flatten");

    $j = Geo::OGR::GT_SetZ($i);
    ok(Geo::OGR::GT_HasZ($j), "$geom, has Z after GT_SetZ");
    ok(bool(Geo::OGR::GT_HasM($i)) eq bool(Geo::OGR::GT_HasM($j)), "$geom, no change to M after GT_SetZ");

    $j = Geo::OGR::GT_SetM($i);
    ok(bool(Geo::OGR::GT_HasZ($i)) eq bool(Geo::OGR::GT_HasZ($j)), "$geom, no change to Z after GT_SetM");
    ok(Geo::OGR::GT_HasM($j), "$geom, has M after GT_SetM");

    for my $z (0,1) {
        for my $m (0,1) {
            $j = Geo::OGR::GT_SetModifier($i,$z,$m);
            ok(bool($z) eq bool(Geo::OGR::GT_HasZ($j)), "$geom, set Z to $z after SetModifier");
            ok(bool($m) eq bool(Geo::OGR::GT_HasM($j)), "$geom, set M to $m after SetModifier");
        }
    }
}

sub bool {
    my $val = shift;
    return $val ? 'true' : 'false';
}
