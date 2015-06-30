use strict;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# list of subs to test (documented subroutines) obtained with
# perl parse-for-doxygen.pl | grep '^sub \|package'
# the subs should be listed on test programs with 'ok', 'below', or 'above' added

my @list;

# package Geo::GDAL
#
# sub AccessTypes ok

eval {
    @list = Geo::GDAL::AccessTypes();
};
ok((@list > 0 and $@ eq ''), "AccessTypes, got $@");

# sub AutoCreateWarpedVRT

# sub CPLBinaryToHex ok
# sub CPLHexToBinary ok

my $binary = Geo::GDAL::CPLHexToBinary('ab10');
my $hex = Geo::GDAL::CPLBinaryToHex($binary);
ok($hex eq 'AB10', "CPLHexToBinary and CPLBinaryToHex, got $hex");

# sub Child
# sub Children
# sub ComputeMedianCutPCT
# sub ComputeProximity

# sub DataTypeIsComplex ok
# sub DataTypeValueRange ok
# sub DataTypes ok

eval {
    @list = Geo::GDAL::DataTypes();
};
ok((@list > 0 and $@ eq ''), "DataTypes, got $@");

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

# sub Debug

# sub DecToDMS ok
# sub DecToPackedDMS ok

my $dms = Geo::GDAL::DecToDMS(62, 'Long');
ok($dms eq " 62d 0' 0.00\"E", "DecToDMS, got '$dms'"),
$dms = Geo::GDAL::DecToPackedDMS(62.15);
my $dec = Geo::GDAL::PackedDMSToDec($dms);
ok($dec == 62.15, "DecToPackedDMS and PackedDMSToDec, got $dec");

# sub DitherRGB2PCT

# sub EscapeString ok

my $s = Geo::GDAL::EscapeString("abc");
ok($s eq 'abc', "EscapeString, got $s");

# sub FindFile ok

$s = Geo::GDAL::FindFile('', 'gcs.csv');
ok($s, "FindFile, got $s");

# sub FinderClean ok

eval {
    Geo::GDAL::PushFinderLocation('abc');
    Geo::GDAL::PopFinderLocation();
    Geo::GDAL::FinderClean();
};
ok($@ eq '', "FinderClean, PushFinderLocation and PopFinderLocation, got $@");

# sub GOA2GetAccessToken
# sub GOA2GetAuthorizationURL
# sub GOA2GetRefreshToken

# sub GetCacheMax ok
# sub GetCacheUsed ok

Geo::GDAL::SetCacheMax(1000000);
$s = Geo::GDAL::GetCacheMax();
ok($s == 1000000, "SetCacheMax and GetCacheMax, got $s");
$s = Geo::GDAL::GetCacheUsed();
ok($s == 0, "GetCacheUsed, got $s");

# sub GetConfigOption ok

Geo::GDAL::SetConfigOption('foo', 'bar');
$s = Geo::GDAL::GetConfigOption('foo');
ok($s eq 'bar', "SetConfigOption and GetConfigOption, got $s");

# sub GetDataTypeSize above
# sub GetDriver ok

for my $name (Geo::GDAL::GetDriverNames()) {
    my $driver;
    eval {
        $driver = Geo::GDAL::GetDriver($name);
    };
    ok($driver, "$name: GetDriver");
}

# sub GetDriverNames ok

eval {
    @list = Geo::GDAL::GetDriverNames();
};
ok((@list > 0 and $@ eq ''), "GetDriverNames, got $@");

# sub GetJPEG2000StructureAsString
# sub IdentifyDriver below (Driver->Create)
# sub NodeData
# sub NodeType ok
# sub NodeTypes ok

eval {
    @list = Geo::GDAL::NodeTypes();
};
ok((@list > 0 and $@ eq ''), "NodeTypes, got $@");

# sub Open
# sub OpenEx
# sub OpenShared
# sub PackCharacter
# sub PackedDMSToDec above
# sub ParseXMLString ok

my $xml = '<PAMDataset><Metadata><MDI key="PyramidResamplingType">NEAREST</MDI></Metadata></PAMDataset>';

my $a = Geo::GDAL::ParseXMLString($xml);
my @elements;
traverse($a);
ok((@elements == 6 and $elements[5][0] eq 'Text' and $elements[5][1] eq 'NEAREST'), "XML parsing and traversing");

# sub Polygonize
# sub PopFinderLocation ok
# sub PushFinderLocation ok
# sub RIOResamplingTypes ok

eval {
    @list = Geo::GDAL::RIOResamplingTypes();
};
ok((@list > 0 and $@ eq ''), "RIOResamplingTypes, got $@");

# sub RasterizeLayer
# sub ReprojectImage
# sub ResamplingTypes ok

eval {
    @list = Geo::GDAL::ResamplingTypes();
};
ok((@list > 0 and $@ eq ''), "ResamplingTypes, got $@");

# sub SerializeXMLTree ok

my $xml2 = Geo::GDAL::SerializeXMLTree($a);
$xml2 =~ s/\s//g;
$xml2 =~ s/key/ key/g;
ok($xml2 eq $xml, "SerializeXMLTree, got $xml2");

# sub SetCacheMax
# sub SetConfigOption above
# sub SieveFilter
# sub VersionInfo ok

my $VersionInfo = Geo::GDAL::VersionInfo('VERSION_NUM');
ok($VersionInfo, "VersionInfo, got $VersionInfo");

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

# package Geo::GDAL::AsyncReader
#
# sub GetNextUpdatedRegion
# sub LockBuffer
# sub UnlockBuffer


# package Geo::GDAL::GeoTransform
#
# sub Apply
# sub FromGCPs
# sub Inv
# sub new ok

my $dataset = Geo::GDAL::Driver('MEM')->Create();
my $transform = Geo::GDAL::GeoTransform->new;
$dataset->GeoTransform($transform);
$transform = $dataset->GeoTransform();

# package Geo::GDAL::VSIF
# sub Close ok
# sub MkDir ok
# sub Open ok
my $vsif = Geo::GDAL::VSIF::Open('/vsimem/test', 'w');
ok(blessed $vsif, "VSIF Open");
my $c = $vsif->Write('12345');
ok($c == 5, "VSIF Write $c");
$vsif->Close();
# sub Read ok
$vsif = Geo::GDAL::VSIF::Open('/vsimem/test', 'r');
my $r = $vsif->Read($c);
$vsif->Close();
ok($r == 12345, "VSIF Read $r");
# sub ReadDir ok

@list = Geo::GDAL::VSIF::ReadDir('/vsimem/');
ok($list[0] eq 'test', "ReadDir: @list");

eval {
    Geo::GDAL::VSIF::MkDir('/vsimem/test');
};
ok($@ ne '', "error is ok: $@");
Geo::GDAL::VSIF::MkDir('/vsimem/test-dir');
$vsif = Geo::GDAL::VSIF::Open('/vsimem/test-dir/x', 'w');
$vsif->Write('12345');
$vsif->Close();

# sub ReadDirRecursive ok
@list = Geo::GDAL::VSIF::ReadDirRecursive('/vsimem');
ok(@list == 3, "ReadDirRecursive: @list");

# sub Rename ok
Geo::GDAL::VSIF::Rename('/vsimem/test-dir/x', '/vsimem/test-dir/y');
@list = Geo::GDAL::VSIF::ReadDirRecursive('/vsimem');
ok($list[2] eq 'test-dir/y', "Rename: @list");

# sub RmDir
Geo::GDAL::VSIF::RmDir('/vsimem/test-dir', 1);
@list = Geo::GDAL::VSIF::ReadDirRecursive('/vsimem');
ok(@list == 1, "RmDir: @list");

# sub Seek
# sub Stat
# sub Tell
# sub Truncate
# sub Unlink
# sub Write ok
