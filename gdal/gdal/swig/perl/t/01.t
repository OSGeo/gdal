use strict;
use warnings;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# package Geo::GDAL::Driver
#
# sub Capabilities ok

my %cap = map {$_=>1} Geo::GDAL::Driver::Capabilities;
my @cap = Geo::GDAL::Driver('GTiff')->Capabilities;
for my $cap (@cap) {
    ok($cap{$cap}, "Capability $cap");
    my $t = Geo::GDAL::Driver('GTiff')->TestCapability($cap);
    ok($t, "Test capability $cap");
}

# sub Copy
# sub CopyFiles
# sub Create ok

my $dataset = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Width => 123, Height => 45);
ok($dataset, "Create a geotiff into vsimem");
my @list = $dataset->GetFileList();
ok($list[0] eq '/vsimem/test.gtiff', "GetFileList");

my $driver = Geo::GDAL::IdentifyDriver('/vsimem/test.gtiff');
ok($driver->Name eq Geo::GDAL::Driver('GTiff')->Name, "IdentifyDriver");

# sub CreationDataTypes
# sub CreationOptionList
# sub Delete
# sub Domains
# sub Extension
# sub MIMEType
# sub Name
# sub Rename
# sub TestCapability above


my $dataset2 = Geo::GDAL::Driver('MEM')->Copy('', $dataset);
my @size1 = $dataset->Size;
my @size2 = $dataset2->Size;
is_deeply(\@size1, \@size2, "Size, Copy, got @size1 and @size2");

$dataset->GetDriver->CopyFiles('/vsimem/new.gtiff', '/vsimem/test.gtiff');
my %files = map {$_=>1} Geo::GDAL::VSIF::ReadDir('/vsimem/');
ok(($files{'new.gtiff'} and $files{'test.gtiff'}), "CopyFiles");

my %dt = map {$_=>1} Geo::GDAL::DataTypes();
for my $dt (Geo::GDAL::Driver('GTiff')->CreationDataTypes()) {
    ok($dt{$dt}, "CreationDataTypes: $dt");
}

for my $co (Geo::GDAL::Driver('GTiff')->CreationOptionList()) {
    ok(ref($co) eq 'HASH', "Creation option");
    ok(ref($co->{Value}) eq 'ARRAY', "Value in creation option") if $co->{Value};
    #use Data::Dumper;
    #print Dumper $co;
}

$dataset->GetDriver->Delete('/vsimem/new.gtiff');
@list = Geo::GDAL::VSIF::ReadDir('/vsimem/');
ok((@list == 1 and $list[0] eq 'test.gtiff'), "Delete");

%dt = map {$_=>1} Geo::GDAL::Driver::Domains();
for my $dt (Geo::GDAL::Driver('GTiff')->Domains()) {
    ok($dt{$dt}, "Driver domain: $dt");
}

my $ext = Geo::GDAL::Driver('GTiff')->Extension;
ok($ext eq 'tif', "Extension, got $ext");

$ext = Geo::GDAL::Driver('GTiff')->MIMEType;
ok($ext eq 'image/tiff', "MIMEType, got $ext");

$dataset->GetDriver->CopyFiles('/vsimem/new.gtiff', '/vsimem/test.gtiff');
$dataset->GetDriver->Rename('/vsimem/new2.gtiff', '/vsimem/new.gtiff');
%files = map {$_=>1} Geo::GDAL::VSIF::ReadDir('/vsimem/');
ok(($files{'new2.gtiff'} and $files{'test.gtiff'}), "Rename");
