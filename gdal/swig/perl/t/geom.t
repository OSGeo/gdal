use strict;
use warnings;
use v5.10;
use Scalar::Util 'blessed';
use Test::More tests => 15;
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
    my $type = 'Point';
    my $geom1 = Geo::OGR::Geometry->new(GeometryType => $type);
    my $geom2 = Geo::OGR::Geometry->new(GeometryType => $type);
    my $points = [1,2];
    $geom1->Points($points);
    $geom2->Points($points);
    my $geom3 = $geom1->Collect($geom2);
    $geom1 = $geom3->Geometry(1);
    undef $geom2;
    undef $geom3;
    my $points2 = $geom1->Points;
    ok(is_deeply($points, $points2), "Parent geometry is kept alive while children exist.");
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
