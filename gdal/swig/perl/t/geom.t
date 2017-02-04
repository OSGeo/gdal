use strict;
use warnings;
use v5.10;
use Scalar::Util 'blessed';
use Test::More tests => 13;
BEGIN { use_ok('Geo::GDAL') };

{
    my $type = 'Point';
    my $geom = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [1,2];
    $geom->Points($points);
    my $points2 = $geom->Points;
    ok(is_deeply($points, $points2), "Set and get points of a $type");
}

{
    my $type = 'MultiPoint';
    my $geom = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [[1,2],[3,4]];
    $geom->Points($points);
    my $points2 = $geom->Points;
    ok(is_deeply($points, $points2), "Set and get points of a $type");
}

{
    my $type = 'LineString';
    my $geom = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [[1,2],[3,4]];
    $geom->Points($points);
    my $points2 = $geom->Points;
    ok(is_deeply($points, $points2), "Set and get points of a $type");
}

{
    my $type = 'MultiLineString';
    my $geom = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [[[1,2],[3,4]],[[5,6],[7,8]]];
    $geom->Points($points);
    my $points2 = $geom->Points;
    ok(is_deeply($points, $points2), "Set and get points of a $type");
}

{
    my $type = 'Polygon';
    my $geom = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [[[1,2],[3,4]],[[5,6],[7,8]]];
    $geom->Points($points);
    my $points2 = $geom->Points;
    ok(is_deeply($points, $points2), "Set and get points of a $type");
}

{
    my $type = 'MultiPolygon';
    my $geom = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [[[[1,2],[3,4]],[[5,6],[7,8]]],[[[1.1,2.2],[3.3,4.4]],[[5.5,6.6],[7.7,8.8]]]];
    $geom->Points($points);
    my $points2 = $geom->Points;
    ok(is_deeply($points, $points2), "Set and get points of a $type");
}
