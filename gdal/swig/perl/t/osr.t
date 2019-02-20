use strict;
use warnings;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

SKIP: {
    my $srs1 = Geo::OSR::SpatialReference->new(EPSG=>2936);
    my $srs2 = Geo::OSR::SpatialReference->new(Text=>$srs1->AsText);
    ok($srs1->ExportToProj4 eq $srs2->ExportToProj4, "new EPSG, Text, Proj4");

    my $src = Geo::OSR::SpatialReference->new(EPSG => 2392);
    my $dst = Geo::OSR::SpatialReference->new(EPSG => 2393);
    ok(($src and $dst), "new Geo::OSR::SpatialReference");
}

SKIP: {
    my $src = Geo::OSR::SpatialReference->new(EPSG => 2392);
    my $dst = Geo::OSR::SpatialReference->new(EPSG => 2393);

    my ($t1, $t2);
    eval {
	$t1 = Geo::OSR::CoordinateTransformation->new($src, $dst);
	$t2 = Geo::OSR::CoordinateTransformation->new($dst, $src);
    };
    skip "Unable to load PROJ.4 library", 3 if $@ =~ /Unable to load/;

    ok($t1, "new Geo::OSR::CoordinateTransformation $@");

    skip "new Geo::OSR::CoordinateTransformation failed", 2 unless ($t1 and $t2);

    # northing, easting order
    my @points = ([6830493.772, 2492055.205],
		  [6830483.772, 2492065.205],
		  [6830483.772, 2492075.205]);

    my $p1 = $points[0][0];

    my @polygon = ([[6830483.772, 2492055.205],
		    [6830483.772, 2492075.205],
		    [6830493.772, 2492075.205],
		    [6830483.772, 2492055.205]]);

    my $p2 = $polygon[0][0][0];
    
    $t1->TransformPoints(\@points);
    $t1->TransformPoints(\@polygon);

    $t2->TransformPoints(\@points);
    $t2->TransformPoints(\@polygon);

    ok(int($p1) == int($points[0][0]), "from EPSG 2392 to 2393 and back in line"); 
    ok(int($p2) == int($polygon[0][0][0]), "from EPSG 2392 to 2393 and back in polygon"); 
    
}
