use strict;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# Documented subs in Geo::GDAL 

my $binary = Geo::GDAL::CPLHexToBinary('ab10');
my $hex = Geo::GDAL::CPLBinaryToHex($binary);
ok($hex eq 'AB10', "CPLHexToBinary and CPLBinaryToHex, got $hex");

my $dms = Geo::GDAL::DecToDMS(62, 'Long');
ok($dms eq " 62d 0' 0.00\"E", "DecToDMS, got '$dms'"),
$dms = Geo::GDAL::DecToPackedDMS(62.15);
my $dec = Geo::GDAL::PackedDMSToDec($dms);
ok($dec == 62.15, "DecToPackedDMS and PackedDMSToDec, got $dec");

my $s = Geo::GDAL::EscapeString("abc");
ok($s eq 'abc', "EscapeString, got $s");

Geo::GDAL::SetConfigOption('foo', 'bar');
$s = Geo::GDAL::GetConfigOption('foo');
ok($s eq 'bar', "SetConfigOption and GetConfigOption, got $s");

Geo::GDAL::SetCacheMax(1000000);
$s = Geo::GDAL::GetCacheMax();
ok($s == 1000000, "SetCacheMax and GetCacheMax, got $s");
$s = Geo::GDAL::GetCacheUsed();
ok($s == 0, "GetCacheUsed, got $s");

eval {
    Geo::GDAL::PushFinderLocation('abc');
    Geo::GDAL::PopFinderLocation();
    Geo::GDAL::FinderClean();
};
ok($@ eq '', "FinderClean, PushFinderLocation and PopFinderLocation, got $@");

$s = Geo::GDAL::FindFile('', 'gcs.csv');
ok($s, "FindFile, got $s");

my @list;

eval {
    @list = Geo::GDAL::AccessTypes();
};
ok((@list > 0 and $@ eq ''), "AccessTypes, got $@");

eval {
    @list = Geo::GDAL::DataTypes();
};
ok((@list > 0 and $@ eq ''), "DataTypes, got $@");

eval {
    @list = Geo::GDAL::GetDriverNames();
};
ok((@list > 0 and $@ eq ''), "GetDriverNames, got $@");

eval {
    @list = Geo::GDAL::NodeTypes();
};
ok((@list > 0 and $@ eq ''), "NodeTypes, got $@");

eval {
    @list = Geo::GDAL::RIOResamplingTypes();
};
ok((@list > 0 and $@ eq ''), "RIOResamplingTypes, got $@");

eval {
    @list = Geo::GDAL::ResamplingTypes();
};
ok((@list > 0 and $@ eq ''), "ResamplingTypes, got $@");

for my $type (Geo::GDAL::DataTypes()) {
    my $nr = $Geo::GDAL::TYPE_STRING2INT{$type};
    my $c = Geo::GDAL::GetDataTypeName($nr);
    ok($type eq $c, "Data type $type");
    eval {
        my $scalar = Geo::GDAL::DataTypeIsComplex($type);
        @list = Geo::GDAL::DataTypeValueRange($type);
        $scalar = Geo::GDAL::GetDataTypeSize($type);
        $scalar = Geo::GDAL::PackCharacter($type);
    };
    ok(($@ eq '' and ($type eq 'Unknown' or @list == 2)), 
       "$type: DataTypeIsComplex, DataTypeValueRange, GetDataTypeSize, PackCharacter, got $@ and ".scalar(@list));
}

for my $name (Geo::GDAL::GetDriverNames()) {
    my $driver;
    eval {
        $driver = Geo::GDAL::GetDriver($name);
    };
    ok($driver, "$name: GetDriver");
}


my $xml = '<PAMDataset><Metadata><MDI key="PyramidResamplingType">NEAREST</MDI></Metadata></PAMDataset>';

my $a = Geo::GDAL::ParseXMLString($xml);
my @elements;
traverse($a);
ok((@elements == 6 and $elements[5][0] eq 'Text' and $elements[5][1] eq 'NEAREST'), 
   "XML parsing and traversing");

sub traverse {
    my $node = shift;
    my $type = $node->[0];
    my $data = $node->[1];
    $type = Geo::GDAL::NodeType($type);
    push @elements, [$type, $data];
    for my $child (@$node[2..$#$node]) {
        traverse($child);
    }
}

my $xml2 = Geo::GDAL::SerializeXMLTree($a);
$xml2 =~ s/\s//g;
$xml2 =~ s/key/ key/g;
ok($xml2 eq $xml, "SerializeXMLTree, got $xml2");

my $VersionInfo = Geo::GDAL::VersionInfo('VERSION_NUM');
ok($VersionInfo, "VersionInfo, got $VersionInfo");

my $dataset = Geo::GDAL::Driver('MEM')->Create();
my $transform = Geo::GDAL::GeoTransform->new;
$dataset->GeoTransform($transform);
$transform = $dataset->GeoTransform();

$dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff');
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


__END__

# Classes

AsyncReader
Band
ColorTable
Dataset
Driver
GCP
GeoTransform
MajorObject
RasterAttributeTable
Transformer
VSIF

__END__
 
public method 	Debug ()
 
public method 	GOA2GetAccessToken ()
public method 	GOA2GetAuthorizationURL ()
public method 	GOA2GetRefreshToken ()

public method 	GetJPEG2000StructureAsString ()

Algorithms:

my $src_srs = Geo::OSR::SpatialReference->new(EPSG => 2392);
my $dst_srs = Geo::OSR::SpatialReference->new(EPSG => 2393);
my $ResampleAlg = 'NearestNeighbour';
my $maxerror = 0.0;
my $dataset = Geo::GDAL::AutoCreateWarpedVRT($src, $src_srs, $dst_srs, $ResampleAlg, $maxerror);

public method 	DitherRGB2PCT (scalar red, scalar green, scalar blue, scalar target, scalar colors, subref callback, scalar callback_data)
 
public method 	ComputeMedianCutPCT (Geo::GDAL::Band red, Geo::GDAL::Band green, Geo::GDAL::Band blue, scalar num_colors, scalar colors, subref callback, scalar callback_data)
 
public method 	ComputeProximity (Geo::GDAL::Band src, Geo::GDAL::Band proximity, hashref options, subref callback, scalar callback_data)

public method 	Polygonize (Geo::GDAL::Band src, Geo::GDAL::Band mask, Geo::OGR::Layer out, scalar PixValField, hashref options, subref callback, scalar callback_data)
 
public method 	RasterizeLayer (Geo::GDAL::Dataset ds, arrayref bands, Geo::OGR::Layer layer, scalar transformer, scalar arg, arrayref burn_values, hashref options, subref callback, scalar callback_data)
 
public method 	RegenerateOverview (Geo::GDAL::Band src, Geo::GDAL::Band overview, scalar resampling, subref callback, scalar callback_data)
 
public method 	RegenerateOverviews (Geo::GDAL::Band src, arrayref overviews, scalar resampling, subref callback, scalar callback_data)
 
public method 	ReprojectImage (scalar src_ds, scalar dst_ds, scalar src_wkt=undef, scalar dst_wkt=undef, scalar ResampleAlg='NearestNeighbour', scalar WarpMemoryLimit=0, scalar maxerror=0.0, subref callback, scalar callback_data)
 
public method 	SieveFilter (Geo::GDAL::Band src, Geo::GDAL::Band mask, Geo::GDAL::Band dst, scalar threshold, scalar connectedness, hashref options, subref callback, scalar callback_data)
 

 
