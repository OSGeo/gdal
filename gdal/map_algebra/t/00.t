use strict;
use warnings;
use v5.10;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
use Geo::GDAL;
BEGIN { use_ok('gma') };

my $band = Geo::GDAL::Driver('MEM')->Create(Width => 16, Height => 16)->Band();
my $b = gma::gma_new_band($band);
say $b;
$b->rand;
$b->print;

my $min = $b->get_min;
say $min;
say $b->get_max;

my @bins = (50,100,200);
my $h = $b->histogram(\@bins);
for my $a (@$h) {
    say "@$a";
}
