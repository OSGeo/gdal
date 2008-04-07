use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

$srs1 = Geo::OSR::SpatialReference->create(EPSG=>2936);
$srs2 = Geo::OSR::SpatialReference->create(Text=>$srs1->AsText);
#print STDERR $srs2->ExportToProj4,"\n";
ok($srs1->ExportToProj4 eq $srs2->ExportToProj4, "create EPSG, Text, Proj4");
