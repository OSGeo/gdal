use strict;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# package Geo::GDAL::Band
# sub AttributeTable
# sub CategoryNames
# sub Checksum
# sub ColorInterpretation
# sub ColorInterpretations
# sub ColorTable
# sub ComputeBandStats
# sub ComputeRasterMinMax
# sub ComputeStatistics
# sub Contours
# sub CreateMaskBand
# sub DataType
# sub Domains
# sub Fill
# sub FillNodata
# sub FlushCache
# sub GetBandNumber
# sub GetBlockSize
# sub GetColorTable
# sub GetDataset
# sub GetDefaultHistogram
# sub GetHistogram
# sub GetMaskBand
# sub GetMaskFlags
# sub GetMaximum
# sub GetMinimum
# sub GetOverview
# sub GetOverviewCount
# sub GetStatistics
# sub HasArbitraryOverviews
# sub MaskFlags
# sub NoDataValue
# sub PackCharacter
# sub ReadRaster
# sub ReadTile
# sub RegenerateOverview
# sub RegenerateOverviews
# sub ScaleAndOffset
# sub SetColorTable
# sub SetDefaultHistogram
# sub SetStatistics
# sub Size
# sub Unit
# sub WriteRaster
# sub WriteTile


my $dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Width => 4, Height => 6);
my $band = $dataset->Band;

$band->CategoryNames('a','b');
my @list = $band->CategoryNames;
ok(($list[0] eq 'a' and $list[1] eq 'b'), "CategoryNames");

@list = $band->GetBlockSize;
ok(($list[0] == 4 and $list[1] == 6), "GetBlockSize");

@list = $band->Size;
ok(($list[0] == 4 and $list[1] == 6), "Size");

my $ds = $band->GetDataset;
ok((defined($ds) and blessed($ds) and $ds->isa('Geo::GDAL::Dataset')), "GetDataset");

$band->Unit('metri');
$ds = $band->Unit();
ok($ds eq 'metri', "Unit");

$band->ScaleAndOffset(0.1, 5);
@list = $band->ScaleAndOffset();
ok(($list[0] == 0.1 and $list[1] == 5), "ScaleAndOffset");

my $nr = $band->GetBandNumber;
ok($nr == 1, "GetBandNumber");

my $rat = Geo::GDAL::RasterAttributeTable->new;
$band->AttributeTable($rat);
$rat = $band->AttributeTable();
ok((defined($rat) and blessed($rat) and $rat->isa('Geo::GDAL::RasterAttributeTable')), "RasterAttributeTable");

my $c = $band->ColorInterpretation;
my %c = map {$_=>1} Geo::GDAL::Band::ColorInterpretations;
ok($c{$c}, "Get ColorInterpretation");
$c = (keys %c)[0];
$band->ColorInterpretation($c);
ok($band->ColorInterpretation eq $c, "Set ColorInterpretation");

@list = $band->Domains;
ok(@list > 1, "Domains");

$c = Geo::GDAL::ColorTable->new;
$c->ColorEntry(0, 100, 50, 150, 300);
@list = $c->ColorTable;
ok($list[0][0] == 100, "Colortable");
$band->SetColorTable($c);
$c = $band->GetColorTable();
ok((defined($c) and blessed($c) and $c->isa('Geo::GDAL::ColorTable')), "Get ColorTable");
@list = $c->ColorTable;
ok($list[0][0] == 100, "Set and Get Colortable");

$dataset = Geo::GDAL::Driver('MEM')->Create(Width => 4, Height => 4);
$dataset->AddBand('Int32');
$band = $dataset->Band(2);
$band->	Fill(123);
my $data = $band->ReadTile;
ok($data->[0][0] == 123, "Fill with integer");
for my $row (@$data) {
#    print "@$row\n";
}
$dataset->AddBand('Float64');
$band = $dataset->Band(3);
$band->	Fill(123.45);
$data = $band->ReadTile;
ok($data->[0][0] == 123.45, "Fill with real");
for my $row (@$data) {
#    print "@$row\n";
}
#$dataset->AddBand('CFloat64');
#$band = $dataset->Band(4);
#$band->Fill(123.45, 10);
#$data = $band->ReadTile;
#for my $row (@$data) {
#    print "@$row\n";
#}

#use Statistics::Descriptive;
#my $stat = Statistics::Descriptive::Full->new();      
$band = $dataset->Band(3);
for my $y (0..3) {
    for my $x (0..3) {
        $data->[$y][$x] = rand 10;
        #$stat->add_data($data->[$y][$x]);
    }
}
$band->WriteTile($data);
for my $row (@$data) {
    #print "@$row\n";
}

my $x;
my ($min, $max, $mean, $stddev);

#print $stat->mean()," ",$stat->standard_deviation(),"\n";

@list = $band->ComputeRasterMinMax;
ok(@list == 2, "ComputeRasterMinMax");

$x = $band->GetMinimum;
ok(!defined($x), "GetMinimum");
@list = $band->GetMinimum;
ok(@list == 2, "GetMinimum");

$x = $band->GetMaximum;
ok(!defined($x), "GetMaximum");
@list = $band->GetMaximum;
ok(@list == 2, "GetMaximum");

@list = $band->ComputeBandStats;
ok(@list == 2, "ComputeBandStats");

$band->ComputeStatistics(1);
$x = $band->GetMaximum;
ok(defined($x), "GetMaximum");

@list = $band->GetStatistics(1,0);
ok(@list == 4, "GetStatistics");

$band->SetStatistics(0, 1, 2, 3);
@list = $band->GetStatistics(0,0);
ok($list[3] == 3, "SetStatistics");

@list = $band->ComputeBandStats;
ok(@list == 2, "ComputeBandStats");

my $foo;
my $n = 0;
@list = $band->ComputeStatistics(0, sub {$foo = $_[2] unless $foo; $n++; 1}, 'foo'); 
ok(@list == 4, "ComputeStatistics");
ok(($n > 0 and $foo eq 'foo'), "ComputeStatistics callback");

Geo::GDAL::VSIF::Unlink('/vsimem/test.gtiff');
$dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Width => 4, Height => 6);
$band = $dataset->Band;
$c = $band->Checksum;
ok($c == 0, "Checksum");

$c = $band->NoDataValue;
ok(!defined($c), "Get NoDataValue");
$band->NoDataValue(10);
$c = $band->NoDataValue;
ok($c == 10, "Set NoDataValue");

# set one pixel no data
$data = $band->ReadTile;
$data->[2][2] = 10;
$band->WriteTile($data);

my @f = $band->MaskFlags;
ok(@f > 0, "MaskFlags");

@f = $band->GetMaskFlags;
ok($f[0] eq 'NoData', "GetMaskFlags");

# fill the one pixel
$band->FillNodata();
$data = $band->ReadTile;
ok($data->[2][2] == 0, "FillNodata, got $data->[2][2]");

$band->CreateMaskBand('PerDataset');
@f = $band->GetMaskFlags;
ok($f[0] eq 'PerDataset', "CreateMaskBand");

#@list = Geo::GDAL::VSIF::ReadDir('/vsimem/');
#print "files @list\n"; # includes .msk

# $m is not valid here any more, how to test?

Geo::GDAL::VSIF::Unlink('/vsimem/test.gtiff');
$dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Bands => 2);
$dataset->BuildOverviews('average', [2,4]);

my $band1 = $dataset->Band(1);
my $band2 = $dataset->Band(2);

$band1->RegenerateOverviews([$band2]); #scalar resampling, subref callback, scalar callback_data
$band1->RegenerateOverview($band2); #scalar resampling, subref callback, scalar callback_data

my $c = $band1->GetOverviewCount;
ok($c == 2, "GetOverviewCount, got $c");
my $o = $band1->GetOverview(1);
ok(defined($o), "GetOverview");
my $b = $band1->HasArbitraryOverviews;
ok(!$b, "HasArbitraryOverviews");

Geo::GDAL::VSIF::Unlink('/vsimem/test.gtiff');
$dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Type => 'Float64');
$band = $dataset->Band;

my $data = $band->ReadTile;
my ($min, $max);
for my $y (0..@$data-1) {
    for my $x (0..@{$data->[$y]}-1) {
        $data->[$y][$x] = rand 10;
        if (defined $min) {
            $min = $data->[$y][$x] if $data->[$y][$x] < $min;
        } else {
            $min = $data->[$y][$x];
        }
        if (defined $max) {
            $max = $data->[$y][$x] if $data->[$y][$x] > $max;
        } else {
            $max = $data->[$y][$x];
        }
    }
}
$band->WriteTile($data);

my $h = $band->GetHistogram(Max => 10.0, Buckets => 11);
my $sum = 0;
for (@$h) {
    $sum += $_;
}
ok($sum == 256*256, "GetHistogram");
ok(@$h == 11, "GetHistogram");
my ($min, $max, $histogram) = $band->GetDefaultHistogram;
ok(ref($histogram) eq 'ARRAY', "GetDefaultHistogram");
eval {
    $band->SetDefaultHistogram($min, $max, $histogram);
};
ok(!$@, "SetDefaultHistogram");

my @h = ('12345678987654321','9223372036854775806');
$band->SetDefaultHistogram(0, 100, \@h);
my @hist = $band->GetDefaultHistogram(0);
ok($hist[0] == 0, "DefaultHistogram");
ok($hist[1] == 100, "DefaultHistogram");
ok($hist[2][0] eq $h[0], "DefaultHistogram");
ok($hist[2][1] eq $h[1], "DefaultHistogram");

my $buf = $band->ReadRaster();
my $pc = $band->PackCharacter;
my @data = unpack("$pc*", $buf);
my $n = @data;
ok($n == 256*256, "ReadRaster");

$buf = pack("$pc*", @data);
eval {
    $band->WriteRaster(XOff => 0, yoff => 0, buf => $buf);
};
ok(!$@, "WriteRaster");

__END__

 
public Geo::OGR::Layer 	Contours (scalar DataSource, hashref LayerConstructor, scalar ContourInterval, scalar ContourBase, arrayref FixedLevels, scalar NoDataValue, scalar IDField, scalar ElevField, subref callback, scalar callback_data)
 
