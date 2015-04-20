use strict;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# Geo::GDAL::Dataset

my %dt = map {$_=>1} Geo::GDAL::Dataset::Domains();
for my $dt (Geo::GDAL::Driver('MEM')->Create->Domains()) {
    ok($dt{$dt}, "Dataset domain: $dt");
}

my $dataset = Geo::GDAL::Driver('MEM')->Create(Width => 8, Height => 10);
my $band = $dataset->Band;
my $band2 = $dataset->Band(1);
my @bands = $dataset->Bands;
ok(@bands == 1, "Bands");

my @list = Geo::GDAL::DataTypes();
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

$data->[5][5] = 3;
$dataset->Band(1)->WriteTile($data);
$data = $dataset->Band(1)->ReadTile;
ok($data->[5][5] == 3, "WriteTile");
#for my $row (@$data) {
#    print "@$row\n";
#}

__END__

tests to do

CreateMaskBand

GCPs
GetGCPProjection

SpatialReference

StartTransaction
CommitTransaction
RollbackTransaction
