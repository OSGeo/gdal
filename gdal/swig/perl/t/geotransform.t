use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

my $t = Geo::GDAL::GeoTransform->new;

is_deeply($t, [0,1,0,0,0,1], "Default geotransform is 0,1,0, 0,0,1");

$t = Geo::GDAL::GeoTransform->new([0,1,0,0,0,1]);

is_deeply($t, [0,1,0,0,0,1], "Create from array ref");

$t = Geo::GDAL::GeoTransform->new(Geo::GDAL::GeoTransform->new);

is_deeply($t, [0,1,0,0,0,1], "Create from another geotransform");

$t = Geo::GDAL::GeoTransform->new(0,1,0,0,0,1);

is_deeply($t, [0,1,0,0,0,1], "Create from array");

ok($t->NorthUp, "Default is north up");

my @gcps;
{
    my @gcp_data = (
        [0,6,0, 0,0],
        [4,1,0, 4,5],
        [0,3,0, 0,3]);
    
    for my $gcp (@gcp_data) {
        push @gcps, Geo::GDAL::GCP->new(@$gcp);
    }
}

$t = Geo::GDAL::GeoTransform->new(GCPs => \@gcps);
@$t = round(@$t);
is_deeply($t, [0,1,0,6,0,-1], "Create from GCPs");

$t = Geo::GDAL::GeoTransform->new(Extent => [0,0,20,20], CellSize => 1);
is_deeply($t, [0,1,0,20,0,-1], "Create from extent and cell size");

# from Math::Round
sub round {
    my $x;
    my $half = 0.50000000000008;
    my @res  = map {
        if ($_ >= 0) { POSIX::floor($_ + $half); }
        else { POSIX::ceil($_ - $half); }
    } @_;
    return (wantarray) ? @res : $res[0];
}
