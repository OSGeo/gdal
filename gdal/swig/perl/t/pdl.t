use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More tests => 4;
BEGIN { use_ok('Geo::GDAL') };
use PDL;

my $band = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/test.gtiff', Width => 23, Height => 45)->Band();
my $t = $band->ReadTile;
$t->[5][3] = 1;
$band->WriteTile($t);

my $pdl;

$pdl = $band->Piddle;
my @s = $pdl->dims;
ok($s[0] == 23 && $s[1] == 45, "Get right size piddle.");

$pdl = $band->Piddle(1,2,4,4);
ok($pdl->at(2,3) == 1, "Data in piddle.");

$pdl += 1;
$band->Piddle($pdl,1,2);
$t = $band->ReadTile;
print "$t->[5][3]\n";
ok($t->[5][3] == 2, "Data from piddle into band.");
