use strict;
use warnings;
use utf8;
use v5.10;
use Scalar::Util 'blessed';
use Encode qw(encode decode);
use Test::More tests => 5;
BEGIN { use_ok('Geo::GDAL') };

my $b = Geo::GDAL::Driver('MEM')->Create()->Band;

{
    my $nodata = $b->NoDataValue;
    ok((not defined $nodata), "NoDataValue returns undef if there is no nodata value");
}

{
    my @nodata = $b->NoDataValue;
    ok((not $nodata[1]), "NoDataValue returns (x, untrue) if there is no nodata value");
}

$b->NoDataValue(13);

{
    my $nodata = $b->NoDataValue;
    ok($nodata == 13, "NoDataValue returns the nodata value if there is nodata value");
}

{
    my @nodata = $b->NoDataValue;
    ok(($nodata[0] == 13 and $nodata[1]), "NoDataValue returns (nodata, true) if there is nodata value");
}

