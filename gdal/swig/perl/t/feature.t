use utf8;
use strict;
use warnings;
use bytes;
use v5.10;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# test features and their fields

my @fields = (
    {Name => 'Binary', Type => 'Binary'},
    {Name => 'Time', Type => 'Time'},
    {Name => 'Date', Type => 'Date'}, 
    {Name => 'DateTime', Type => 'DateTime'}, 
    {Name => 'Integer', Type => 'Integer'}, 
    {Name => 'Integer64', Type => 'Integer64'}, 
    {Name => 'IntegerList', Type => 'IntegerList'}, 
    {Name => 'Integer64List', Type => 'Integer64List'},
    {Name => 'Real', Type => 'Real'}, 
    {Name => 'RealList', Type => 'RealList'},
    {Name => 'String', Type => 'String'},
    {Name => 'StringList', Type => 'StringList'},
    #{Name => 'WideString', Type => 'WideString'},
    #{Name => 'WideStringList', Type => 'WideStringList'}
   );

my $f = Geo::OGR::Feature->new(
    GeometryType => 'Point', 
    Fields => \@fields
    );

{
    my $l = $f->Layer();
    ok(!(defined $l), "Layer of an orphan feature is undefined.");
}

{
    my @n = $f->GetFieldNames();
    ok(@n == @fields + 1, "Get field names (count).");
}

{
    my $i = $f->GetFieldIndex('Binary');
    ok($i == 0, "Get field index: valid field name");
    $i = $f->GetFieldIndex(0);
    ok($i == 0, "Get field index: valid field index");
    eval {
        $i = $f->GetFieldIndex('No');
    };
    ok($@ ne '', "Get field index: invalid field name");
    eval {
        $i = $f->GetFieldIndex(28);
    };
    ok($@ ne '', "Get field index: invalid index");
}

{
    $f->{String} = 'x';
    ok ($f->{String} eq 'x', "Set and get field using hashref syntax works for a non-spatial field.");
    eval {
        $f->{10} = 'x';
    };
    ok ($@ ne '', "Set field using hashref syntax and field index does not work.");
    eval {
        $f->{No} = 'x';
    };
    ok ($@ ne '', "Set using hashref syntax for a non-existing field is an error.");
    eval {
        my $test = $f->{No};
    };
    ok ($@ ne '', "Get using hashref syntax for a non-existing field is an error.");
    my $wkt = "POINT (1 2)";
    $f->{''} = {WKT => $wkt}; # the name of a single geometry field is ''
    ok($f->Geometry('')->AsText eq $wkt, "Setting geometry using hashref syntax works");
    eval {
        my $test = $f->{''};
    };
    ok ($@ ne '', "Getting geometry field using hashref syntax is an error because it can't be done safely.");
}

{
    my $v = $f->Field('Integer');
    ok(!(defined $v), "Unset field returns undef.");
    $f->Field(Integer => 1);
    $v = $f->Field('Integer');
    ok($v == 1, "Set field returns its value.");
    $f->Field(Integer => undef);
    $v = $f->Field('Integer');
    ok(!(defined $v), "Setting field to undef unsets it.");
}

{
    my $b = 'åäöAJ';
    my $c = $f->Field(String => $b);
    ok($b eq $c, "Set and get string field.");
}

{
    my $b = ['åäöAJ','åäöAJx'];
    my $c = $f->Field(StringList => $b);
    ok($b->[0] eq $c->[0] && $b->[1] eq $c->[1], "Set and get string list field.");
}

{
    my $b = 123.456;
    my $c = $f->Field(Real => $b);
    ok("$b" eq "$c", "Set and get real field.");
}

{
    my $b = [123.456,2123.4567];
    my $c = $f->Field(RealList => $b);
    ok("@$b" eq "@$c", "Set and get real list field.");
}

{
    my $b = 123;
    my $c = $f->Field(Integer => $b);
    ok("$b" eq "$c", "Set and get integer field.");
}

{
    my $b = [123,12];
    my $c = $f->Field(IntegerList => $b);
    ok("@$b" eq "@$c", "Set and get integer list field.");
}

{
    my $b = 9223372036854775806;
    my $c = $f->Field(Integer64 => $b);
    ok($b eq $c, "Set and get integer64 field.");
}

{
    my $b = [9223372036854775806,12];
    my $c = $f->Field(Integer64List => $b);
    ok("@$b" eq "@$c", "Set and get integer64 list field.");
}

{
    my $b_hex = '4100204d414e204120504c414e20412043414e414c2050414e414d41';
    my $b = pack('H*', $b_hex);
    $f->Field(Binary => $b);
    my $c = $f->Field('Binary');
    my $c_hex = unpack('H*', $c);
    ok($c_hex eq $b_hex, "Set and get a binary field.");
    $c = $f->Field(0);
    $c_hex = unpack('H*', $c);
    ok($c_hex eq $b_hex, "Set and get a binary field.");
}

{
    my $b = [2008,3,23];
    my $c = $f->Field(Date => $b);
    ok("@$b" eq "@$c", "Set and get date field.");
}

{
    my $b = [2008,3,23,2,3,4,1];
    my $c = $f->Field(DateTime => $b);
    ok("@$b" eq "@$c", "Set and get datetime field.");
}

{
    my $b = [2,3,4,1];
    my $c = $f->Field(Time => $b);
    ok("@$b" eq "@$c", "Set and get time field.");
}

{
    my $p = [1,2];
    my $b = Geo::OGR::Geometry->new(GeometryType => 'Point', points => $p);
    my $c = $f->Geometry($b);
    ok($b->As(Format => 'ISO WKT') eq $c->As(Format => 'ISO WKT'), "Set and get the geometry field.");
    $c = $f->Geometry(0 => $b);
    ok($b->As(Format => 'ISO WKT') eq $c->As(Format => 'ISO WKT'), "Set and get the geometry field.");
}

eval {
    $f = Geo::OGR::Feature->new(
        GeometryType => 'Point', 
        Fields => [
            {Name => 'Integer', Type => 'Integer'},
            {Name => 'Point', Type => 'Point'},
            {Name => 'LineString', Type => 'LineString'},
        ]);
};
ok($@, "Mixing error.");

@fields = ({Name => 'Integer', Type => 'Integer'},
           {Name => 'Point', Type => 'Point'},
           {Name => 'LineString', Type => 'LineString'});

$f = Geo::OGR::Feature->new(Fields => \@fields);

{
    my @n = $f->GetFieldNames();
    ok(@n == @fields, "Get field names (count).");
}

{
    my $p = [1,2];
    my $b = Geo::OGR::Geometry->new(GeometryType => 'Point', points => $p);
    my $c = $f->Geometry(Point => $b);
    ok($b->As(Format => 'ISO WKT') eq $c->As(Format => 'ISO WKT'), "Set and get the point field.");
}
