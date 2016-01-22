use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };
Geo::GDAL::PushFinderLocation('../../data');

$srs1 = Geo::OSR::SpatialReference->create(EPSG=>2936);
$srs2 = Geo::OSR::SpatialReference->create(Text=>$srs1->AsText);

ok($srs1->ExportToProj4 eq $srs2->ExportToProj4, "create EPSG, Text, Proj4");

my $src = Geo::OSR::SpatialReference->new();
$src->ImportFromEPSG(2392);

my $dst = Geo::OSR::SpatialReference->new();
$dst->ImportFromEPSG(2393);
ok(($src and $dst), "create Geo::OSR::SpatialReference");

SKIP: {
    skip "PROJSO not set", 1 if (!$ENV{PROJSO} and $^O eq 'MSWin32');
    my ($t1, $t2);
    eval {
	$t1 = Geo::OSR::CoordinateTransformation->new($src, $dst);
	$t2 = Geo::OSR::CoordinateTransformation->new($dst, $src);
    };
    ok($t1, "create Geo::OSR::CoordinateTransformation $@");

    skip "create Geo::OSR::CoordinateTransformation failed",1 unless ($t1 and $t2);

    my @points = ([2492055.205, 6830493.772],
		  [2492065.205, 6830483.772],
		  [2492075.205, 6830483.772]);

    my $p1 = $points[0][0];

    my @polygon = ([[2492055.205, 6830483.772],
		    [2492075.205, 6830483.772],
		    [2492075.205, 6830493.772],
		    [2492055.205, 6830483.772]]);

    my $p2 = $polygon[0][0][0];
    
    $t1->TransformPoints(\@points);
    $t1->TransformPoints(\@polygon);

    $t2->TransformPoints(\@points);
    $t2->TransformPoints(\@polygon);

    ok(int($p1) == int($points[0][0]), "from EPSG 2392 to 2393 and back in line"); 
    ok(int($p2) == int($polygon[0][0][0]), "from EPSG 2392 to 2393 and back in polygon"); 
    
}

