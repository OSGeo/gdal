use strict;
use warnings;
use bytes;
use v5.10;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# test layer construction and schema manipulation

my $ds = Geo::OGR::Driver('Memory')->Create('test');

my $l = $ds->CreateLayer(
        Fields => [ { Name => 'Integer', 
                      Type => 'Integer' },
                    { Name => 'Point', 
                      Type => 'Point' } ] );
my $s = $l->Schema;
my $f = $s->{Fields};

ok(@$f == 2, "Two fields");
ok($f->[0]->{Type} eq 'Integer', "First Integer");
ok($f->[1]->{Type} eq 'Point', "Second Point");

$l = $ds->CreateLayer(
    GeometryType => 'Point',
    Fields => [ { Name => 'Integer', 
                  Type => 'Integer' } ] );
$s = $l->Schema;
$f = $s->{Fields};

ok(@$f == 2, "Two fields");
ok($f->[0]->{Type} eq 'Integer', "First Integer");
ok($f->[1]->{Type} eq 'Point', "Second Point");
ok($f->[1]->{Name} eq '', "Second with no name");

$l->AlterFieldDefn(Integer => (Type => 'String', Name => 'String'));
$s = $l->Schema;
$f = $s->{Fields};
ok($f->[0]->{Type} eq 'String', "Alter field type");
ok($f->[0]->{Name} eq 'String', "Alter field name");

$l->CreateField(Type => 'Real', Name => 'Real');
$l->CreateField(Type => 'LineString', Name => 'LineString');
$s = $l->Schema;
$f = $s->{Fields};
ok(@$f == 4, "Add two fields.");
ok($f->[1]->{Type} eq 'Real', "New Real");
ok($f->[3]->{Type} eq 'LineString', "New LineString");

$l->DeleteField('String');
$s = $l->Schema;
$f = $s->{Fields};
ok(@$f == 3, "Delete one field.");

ok($l->GeometryType(0) eq 'Point', "Get GeometryType");
ok($l->GeometryType('LineString') eq 'LineString', "Get GeometryType");

# ReorderField
# ReorderFields
