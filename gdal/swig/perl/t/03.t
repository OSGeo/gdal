use strict;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# Geo::GDAL::Band

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



__END__

 
public scalar 	Checksum (scalar xoff=0, scalar yoff=0, scalar xsize=undef, scalar ysize=undef)
 
public Geo::OGR::Layer 	Contours (scalar DataSource, hashref LayerConstructor, scalar ContourInterval, scalar ContourBase, arrayref FixedLevels, scalar NoDataValue, scalar IDField, scalar ElevField, subref callback, scalar callback_data)
 
public scalar 	NoDataValue (scalar NoDataValue) 
public method 	FillNodata (scalar mask, scalar max_search_dist=10, scalar smoothing_iterations=0, scalar options={}, subref callback, scalar callback_data)
 
public list 	SetDefaultHistogram (scalar min, scalar max, scalar histogram) 
public list 	GetDefaultHistogram (scalar force=1, subref callback=undef, scalar callback_data=undef)
public list 	GetHistogram (hash parameters)

public method 	CreateMaskBand () 
public Geo::GDAL::Band 	GetMaskBand ()
public scalar 	GetMaskFlags ()
 
public Geo::GDAL::Band 	GetOverview (scalar band)
public scalar 	GetOverviewCount ()
public method 	HasArbitraryOverviews () 

public scalar 	ReadRaster (hash params)
public method 	WriteRaster (hash params)
