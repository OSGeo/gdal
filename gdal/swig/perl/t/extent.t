use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

my $e = Geo::GDAL::Extent->new;

ok(blessed($e), "Can create new extent objects");
ok(ref $e eq 'Geo::GDAL::Extent', "Can create new extent objects (2)");

ok($e->IsEmpty, "New, without arguments created extent is empty");

my @s = $e->Size;

is_deeply(\@s, [0,0], "The size of and empty extent is (0,0)");

$e = Geo::GDAL::Extent->new([0,0,1,1]);

ok(!$e->IsEmpty, "New, with arguments created extent is not empty");

@s = $e->Size;

is_deeply(\@s, [1,1], "The size of 0,0,1,1 extent is 1,1");

$e = Geo::GDAL::Extent->new(0,0,1,1);

ok(!$e->IsEmpty, "New, with arguments created extent is not empty (2)");

@s = $e->Size;

is_deeply(\@s, [1,1], "The size of 0,0,1,1 extent is 1,1 (2)");

my $f = Geo::GDAL::Extent->new(1,1,2,2);
ok(!$e->Overlaps($f), "Touching extents do not overlap");
$f = Geo::GDAL::Extent->new(0.5,0.5,1.5,1.5);
ok($e->Overlaps($f), "Overlapping extents overlap");

$f = Geo::GDAL::Extent->new(1,-1,2,0);
ok(!$e->Overlaps($f), "Touching extents do not overlap");
$f = Geo::GDAL::Extent->new(0.5,-0.5,1.5,0.5);
ok($e->Overlaps($f), "Overlapping extents overlap");

$f = Geo::GDAL::Extent->new(-1,-1,0,0);
ok(!$e->Overlaps($f), "Touching extents do not overlap");
$f = Geo::GDAL::Extent->new(-0.5,-0.5,0.5,0.5);
ok($e->Overlaps($f), "Overlapping extents overlap");

$f = Geo::GDAL::Extent->new(-1,1,0,2);
ok(!$e->Overlaps($f), "Touching extents do not overlap");
$f = Geo::GDAL::Extent->new(-0.5,0.5,0.5,1.5);
ok($e->Overlaps($f), "Overlapping extents overlap");

$f = Geo::GDAL::Extent->new(0.5,0.5,1.5,1.5);
my $g = $e->Overlap($f);
is_deeply($g, [0.5,0.5,1,1], "Overlap is ok");

$f = Geo::GDAL::Extent->new(1,1,2,2);
$g = $e->Overlap($f);
ok($g->IsEmpty, "Overlap of not overlapping extents is empty");

$f = Geo::GDAL::Extent->new(1,1,2,2);
$f->ExpandToInclude($e);
is_deeply($f, [0,0,2,2], "Expand to NE");

$f = Geo::GDAL::Extent->new(1,-1,2,0);
$f->ExpandToInclude($e);
is_deeply($f, [0,-1,2,1], "Expand to SE");

$f = Geo::GDAL::Extent->new(-1,-1,0,0);
$f->ExpandToInclude($e);
is_deeply($f, [-1,-1,1,1], "Expand to SW");

$f = Geo::GDAL::Extent->new(-1,1,0,2);
$f->ExpandToInclude($e);
is_deeply($f, [-1,0,1,2], "Expand to NW");

$f = Geo::GDAL::Extent->new();
$e->ExpandToInclude($f);
is_deeply($e, [0,0,1,1], "Expand with empty");

$f->ExpandToInclude($e);
is_deeply($f, [0,0,1,1], "Expand empty");
