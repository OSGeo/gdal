use strict;
use warnings;
use Carp;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# package Geo::GDAL::Dataset
#
# sub AddBand
# sub Band
# sub Bands
# sub BuildOverviews
# sub CommitTransaction
# sub Domains
# sub GCPs
# sub GeoTransform
# sub GetDriver
# sub GetFileList
# sub GetGCPProjection
# sub Open
# sub OpenShared

my $dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Width => 123, Height => 45);
undef $dataset; # need to do this since Open is called before the assignment

$dataset = Geo::GDAL::Open('/vsimem/test.gtiff', 'ReadOnly');
ok($dataset, "Open");

$dataset = Geo::GDAL::OpenShared('/vsimem/test.gtiff', 'ReadOnly');
ok($dataset, "OpenShared");

$dataset = Geo::GDAL::OpenEx('/vsimem/test.gtiff');
ok($dataset, "OpenEx");


# sub ReadRaster
# sub RollbackTransaction
# sub Size
# sub SpatialReference
# sub StartTransaction
# sub WriteRaster

$dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Width => 123);
my @list = $dataset->GetFileList();
undef $dataset;

@list = Geo::GDAL::VSIF::ReadDir('/vsimem/');
print "@list\n";
my $driver = Geo::GDAL::IdentifyDriver('/vsimem/test.gtiff');
print $driver->Name,"\n";


$dataset = Geo::GDAL::Open('/vsimem/test.gtiff', 'ReadOnly');
print "$dataset\n";
undef $dataset;

$dataset = Geo::GDAL::OpenShared('/vsimem/test.gtiff', 'ReadOnly');
print "$dataset\n";
$dataset = Geo::GDAL::OpenEx('/vsimem/test.gtiff');
print "$dataset\n";


my %dt = map {$_=>1} Geo::GDAL::Dataset::Domains();
for my $dt (Geo::GDAL::Driver('MEM')->Create->Domains()) {
    ok($dt{$dt}, "Dataset domain: $dt");
}

$dataset = Geo::GDAL::Driver('MEM')->Create(Width => 8, Height => 10);
my $band = $dataset->Band;
my $band2 = $dataset->Band(1);
my @bands = $dataset->Bands;
ok(@bands == 1, "Bands");

@list = Geo::GDAL::DataTypes();
#print "@list\n";
$dataset->AddBand('Int32', {a => 1});
@bands = $dataset->Bands;
ok(@bands == 2, "AddBand");
my $dt = $dataset->Band(2)->DataType;
ok($dt eq 'Int32', "DataType");

my $transform = Geo::GDAL::GeoTransform->new(1,2,3,4,5,6);
$dataset->GeoTransform($transform);
$transform = $dataset->GeoTransform();
is_deeply($transform, [1,2,3,4,5,6], "GeoTransform");

$transform = Geo::GDAL::GeoTransform->new;
$dataset->GeoTransform($transform);

@list = Geo::GDAL::RIOResamplingTypes();
#print "@list\n";

my $buf = $dataset->ReadRaster(0, 0, 8, 10, 8, 10, undef, [1,2]);
my $pc = Geo::GDAL::PackCharacter('Byte');
my @data = unpack("$pc*", $buf);
my $n = @data;
ok($n == 160, "ReadRaster");

$data[80] = 1;
$data[159] = 2;
$buf = pack("$pc*", @data[80..159]);
$dataset->WriteRaster(XOff => 0, yoff => 0, buf => $buf, BandList => [1]);
my $data = $dataset->Band(1)->ReadTile;
ok(($data->[0][0] == 1 and $data->[9][7] == 2), "WriteRaster ReadTile");

$data[80] = 1;
$data[159] = 2;
$buf = pack("$pc*", @data[80..159]);
$dataset->WriteRaster(XOff => 0, yoff => 0, buf => \$buf, BandList => [1]);
$data = $dataset->Band(1)->ReadTile;
ok(($data->[0][0] == 1 and $data->[9][7] == 2), "WriteRaster ReadTile 2");


$data->[5][5] = 3;
$dataset->Band(1)->WriteTile($data);
$data = $dataset->Band(1)->ReadTile;
ok($data->[5][5] == 3, "WriteTile");
#for my $row (@$data) {
#    print "@$row\n";
#}
