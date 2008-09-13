use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

$srs1 = Geo::OSR::SpatialReference->create(EPSG=>2936);
$srs2 = Geo::OSR::SpatialReference->create(Text=>$srs1->AsText);
#print STDERR $srs2->ExportToProj4,"\n";
ok($srs1->ExportToProj4 eq $srs2->ExportToProj4, "create EPSG, Text, Proj4");

my $src = Geo::OSR::SpatialReference->new();
$src->ImportFromEPSG(2392);
my $dst = Geo::OSR::SpatialReference->new();
$dst->ImportFromEPSG(2392);
ok(($src and $dst), "create Geo::OSR::SpatialReference");

SKIP: {
    skip "PROJSO not set", 1 unless $ENV{PROJSO};
    my $t;
    eval {
	$t = Geo::OSR::CoordinateTransformation->new($src, $dst);
    };
    ok($t, "create Geo::OSR::CoordinateTransformation $@");
    
    my @points = ([2492055.205, 6830493.772],
		  [2492065.205, 6830483.772]);
    
    $t->TransformPoints(\@points) if $t;
}
