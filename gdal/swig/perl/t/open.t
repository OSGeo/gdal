use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# Drivers, DriverNames, Driver
#
# Geo::GDAL
# Geo::OGR

for my $method (qw/Drivers DriverNames Driver/) {
    ok(Geo::GDAL->can($method), "$method exists");
    ok(Geo::OGR->can($method), "$method exists");
}

for my $driver (Geo::GDAL::Drivers()) {
    ok($driver->TestCapability('RASTER'), $driver->Name." is a raster driver");
}

for my $driver (Geo::OGR::Drivers()) {
    ok($driver->TestCapability('VECTOR'), $driver->Name." is a vector driver");
}

# Open, OpenShared, OpenEx 
#
# Geo::GDAL: Open, OpenShared, OpenEx
# Geo::OGR: Open, OpenShared
# Geo::GDAL::Driver: Open
# Geo::OGR::Driver: Open
# Geo::GDAL::Dataset: Open, OpenShared
# Geo::OGR::DataSource: Open, OpenShared

for my $method (qw/Open OpenShared OpenEx/) {
    ok(Geo::GDAL->can($method), "$method exists");
 }

for my $method (qw/Open OpenShared/) {
    ok(Geo::OGR->can($method), "$method exists");
    ok(Geo::GDAL::Dataset->can($method), "$method exists");
    ok(Geo::OGR::DataSource->can($method), "$method exists");
}

for my $method (qw/Open/) {
    ok(Geo::GDAL::Driver->can($method), "$method exists");
    ok(Geo::OGR::Driver->can($method), "$method exists");
}

# Create, Copy
#
# Geo::GDAL::Driver
# Geo::OGR::Driver

for my $method (qw/Create Copy/) {
    ok(Geo::GDAL::Driver->can($method), "$method exists");
    ok(Geo::OGR::Driver->can($method), "$method exists");
}

{
    my $ds = Geo::GDAL::Driver('MEM')->Create();
    ok($ds, "MEM Create works.");
}

{
    my $ds = Geo::OGR::Driver('Memory')->Create();
    ok($ds, "Memory Create works.");
}

{
    my $ds = Geo::GDAL::Driver('MEM')->Create();
    my $progress;
    Geo::GDAL::Driver('MEM')->Copy( Name => '', src => $ds, progress => sub {$progress = "Me progress!";1});
    ok($progress eq "Me progress!", "MEM Copy works.");
}
