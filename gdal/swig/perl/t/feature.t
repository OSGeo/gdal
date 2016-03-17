use strict;
use warnings;
use bytes;
use v5.10;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# test features and their fields

my $f = Geo::OGR::Feature->new(
    GeometryType => 'Point', 
    Fields => [
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
    ]);

{
    my $l = $f->Layer();
    ok(!(defined $l), "Layer of an orphan feature is undefined.");
}

{
    my $i = $f->GetFieldIndex('Binary');
    ok($i == 0, "Get field index");
}

{
    my $b = 'åäöAJ';
    my $c = $f->Field(String => $b);
    ok(is_deeply($b, $c), "Set and get string field.");
}

{
    my $b = ['åäöAJ','åäöAJx'];
    my $c = $f->Field(StringList => $b);
    ok(is_deeply($b, $c), "Set and get string list field.");
}

{
    my $b = 123.456;
    my $c = $f->Field(Real => $b);
    ok(is_deeply($b, $c), "Set and get real field.");
}

{
    my $b = [123.456,2123.4567];
    my $c = $f->Field(RealList => $b);
    ok(is_deeply($b, $c), "Set and get real list field.");
}

{
    my $b = 123;
    my $c = $f->Field(Integer => $b);
    ok(is_deeply($b, $c), "Set and get integer field.");
}

{
    my $b = [123,12];
    my $c = $f->Field(IntegerList => $b);
    ok(is_deeply($b, $c), "Set and get integer list field.");
}

{
    my $b = 9223372036854775806;
    my $c = $f->Field(Integer64 => $b);
    ok(is_deeply($b, $c), "Set and get integer64 field.");
}

{
    my $b = [9223372036854775806,12];
    my $c = $f->Field(Integer64List => $b);
    ok(is_deeply($b, $c), "Set and get integer64 list field.");
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
    ok(is_deeply($b, $c), "Set and get date field.");
}

{
    my $b = [2008,3,23,2,3,4,1];
    my $c = $f->Field(DateTime => $b);
    ok(is_deeply($b, $c), "Set and get datetime field.");
}

{
    my $b = [2,3,4,1];
    my $c = $f->Field(Time => $b);
    ok(is_deeply($b, $c), "Set and get time field.");
}

