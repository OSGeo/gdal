use strict;
use warnings;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

use vars qw/%test_driver $loaded $verbose @types %pack_types @fails @tested_drivers/;

# this is a mixture or mostly ad hoc tests
# a good set of tests focusing on the Perl bindings is really needed

{
    my $l = Geo::OGR::Driver('Memory')->Create()->CreateLayer();
    $l->CreateField(Name => 'value', Type => 'Integer');
    my $s1 = $l->Schema;
    $l->CreateField(Name => 'v', Type => 'Integer');
    $l->DeleteField('v');
    my $s2 = $l->Schema;
    is_deeply($s1, $s2, "Layer schema test");
}

{
    my $l = Geo::OGR::Driver('Memory')->Create()->CreateLayer({GeometryType=>'Polygon'});
    eval {
	# this is an error because the layer is polygon and this is a point
	$l->InsertFeature([0,12.2,{wkt=>'POINT(1 1)'}]);
    };
    ok($@, 'an error because geometry type mismatch');
}

{
    our $warning;
    BEGIN { $SIG{'__WARN__'} = sub { $warning = $_[0] } }
    my $l = Geo::OGR::Driver('Memory')->Create()->CreateLayer(GeometryType => 'None');
    $l->CreateField(Name => 'value', Type => 'Integer');
    $l->CreateField(Name => 'geom', GeometryType => 'Point');
    $l->InsertFeature([0,12.2,{wkt=>'POINT(1 1)'}]);
    my $f = $l->GetFeature(0);
    my $r = $f->Row;
    ok($r->{geom}->AsText eq 'POINT (1 1)', 'The geometry of the inserted feature is ok');
    ok($r->{value} == 12, "The float is changed to integer because that's the type in the layer ($warning)");
    my $n = $f->Tuple;
    ok($n == 3, 'The extra field is scrapped because layer does not have a field for it');
}

{
    # test conversion methods
    my $g = Geo::OGR::Geometry->new(WKT=>'POINT (1 1)');
    my $x = Geo::OGR::Geometry->new(WKT=>'POINT (2 2)');
    my $g2 = $g->ForceToMultiPoint($x);
    ok($g2->AsText eq 'MULTIPOINT (1 1,2 2)', 'ForceToMultiPoint');
    $g = Geo::OGR::Geometry->new(WKT=>'LINESTRING (1 1,2 2)');
    $x = Geo::OGR::Geometry->new(WKT=>'LINESTRING (2 2,3 3)');
    $g2 = $g->ForceToMultiLineString($x);
    ok($g2->AsText eq 'MULTILINESTRING ((1 1,2 2),(2 2,3 3))', 'ForceToMultiLineString');
    $g = Geo::OGR::Geometry->new(WKT=>'POLYGON ((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5))');
    $x = Geo::OGR::Geometry->new(WKT=>'POLYGON ((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5))');
    $g2 = $g->ForceToMultiPolygon($x);
    ok($g2->AsText eq 'MULTIPOLYGON (((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5)),((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5)))', 'ForceToMultiPolygon');
    $g = Geo::OGR::Geometry->new(WKT=>'POINT (1 1)');
    $x = Geo::OGR::Geometry->new(WKT=>'LINESTRING (2 2,3 3)');
    $g2 = $g->ForceToCollection($x);
    ok($g2->AsText eq 'GEOMETRYCOLLECTION (POINT (1 1),LINESTRING (2 2,3 3))', 'ForceToCollection');
    my @g = $g2->Dissolve;
    ok($g[0]->AsText eq 'POINT (1 1)', 'Dissolve point');
    ok($g[1]->AsText eq 'LINESTRING (2 2,3 3)', 'Dissolve line string');
}

{
    my $g = Geo::OGR::Geometry->new(wkt => "point(1 2)");
    $g->Point(2,3);
    my @p = $g->Point;
    ok($p[0] == 2, "Point");
    eval {
	my $wkb = "0102000000050000005555555524F3484100000000A0F05941555555552".
	    "4F34841000000E09CF05941000000C02EF34841ABAAAAAA97F05941ABAAAA2A39F3".
	    "4841000000E09CF05941ABAAAA2A39F3484100000000A0F05941";
	$g = Geo::OGR::Geometry->new(hexwkb => $wkb);
    };
    ok ($@ eq '', "new from WKb: $@");
    eval {
	my $gml = "<gml:Point><gml:coordinates>1,1</gml:coordinates></gml:Point>";
	$g = Geo::OGR::Geometry->new(gml => $gml);
    };
    ok ($@ eq '', "new from GML: $@");
    eval {
	$g = Geo::OGR::Geometry->new(GeoJSON => "abc");
    };
    ok ($@ =~ /JSON parsing error/, "new from GeoJSON: $@");
}

{
    # test the Points method
    my $g = Geo::OGR::Geometry->new( WKT => 'POINT(1.1 2.2)');
    my $p = $g->Points;
    ok (sprintf("%.1f", $p->[0]) eq '1.1' && sprintf("%.1f", $p->[1]) eq '2.2', "Points from a point is a simple anonymous array");
    $g->Points($p);
    my $q = $g->Points;
    is_deeply($p, $q, "Points with a point");
    $g = Geo::OGR::Geometry->new(wkt => "linestring(1 1, 1 2, 2 2)");
    $p = $g->Points;
    ok ($p->[0]->[0] == 1 && $p->[1]->[1] == 2, "Points from a linestring is an anonymous array of points");
    $g->Points($p);
    $q = $g->Points;
    is_deeply($p, $q, "Points with a linestring");
}

{
    my $g = Geo::OGR::Geometry->new(wkt => "linestring(1 1, 1 2, 2 2)");
    ok ($g->Length == 2, "Length 1");
}

{
    my $g2;
    {
	my $d = Geo::OGR::FeatureDefn->new(Fields=>[Geo::OGR::FieldDefn->new(Name => 'Foo', Index => 1)]);
        $d->AddField(Type => 'Point');
	my $f = Geo::OGR::Feature->new($d);
	my $g = Geo::OGR::Geometry->new('Point');
	$f->SetGeometry($g);
	my $fd = $f->GetDefnRef;
	my $s = $fd->Schema;
	my $s2 = $s->{Fields}[0];
	ok($s->{Fields}[1]{Type} eq 'Point', 'Feature defn schema 0');
	ok($s2->{Name} eq 'Foo', 'Feature defn schema 1');
	ok($s2->{Type} eq 'String', 'Feature defn schema 2');
	$g2 = $f->GetGeometry;
    }
    $g2->GetDimension; # does not cause a kaboom
}
{
    my $f = Geo::OGR::FieldDefn->new();
    my $s = $f->Schema(Width => 10);
    ok($s->{Width} == 10, 'fieldefn schema 1');
    ok($s->{Type} eq 'String', 'fieldefn schema 2');
}
{
    my $driver = Geo::OGR::Driver('Memory');
    my %cap = map {$_=>1} $driver->Capabilities;
    ok($cap{CREATE}, "driver capabilities");
    my $datasource = $driver->Create('test');
    #%cap = map {$_=>1} $datasource->Capabilities;
    #ok($cap{CreateLayer} and $cap{DeleteLayer}, "data source capabilities");
    
    my $layer = $datasource->CreateLayer('a', undef, 'Point');
    my %cap = map { $_ => 1 } $layer->Capabilities;
    for (qw/RandomRead SequentialWrite RandomWrite	
	  FastFeatureCount CreateField DeleteFeature FastSetNextByIndex/) {
	ok($cap{$_}, "layer has capability: $_");
    }
    $datasource->CreateLayer('b', undef, 'Point');
    $datasource->CreateLayer('c', undef, 'Point');
    my @layers = $datasource->Layers;
    ok(is_deeply(\@layers, ['a','b','c'], "layers"));
    if ($datasource->TestCapability('DeleteLayer')) {
        $datasource->DeleteLayer('b');
	@layers = $datasource->Layers;
	ok(is_deeply(\@layers, ['a','c'], "delete layer"));
    }
    
    $layer = $datasource->CreateLayer
        ( Name => 'test', 
          GeometryType => 'Point',
          Fields => [ {Name => 'test1', Type => 'Integer'},
                      {Name => 'test2', Type => 'String'},
                      {Name => 'test3', Type => 'Real'} ], 
          ApproxOK => 1 );
    $layer->InsertFeature({ test1 => 13, 
			    Geometry => { Points => [1,2,3] } });
    $layer->InsertFeature({ test2 => '31a', 
			    Geometry => { Points => [3,2] } });
    $layer->ResetReading;
    my $i = 0;
    while (my $f = $layer->GetNextFeature) {
	my @a = $f->Tuple;
	$a[4] = $a[4]->AsText;
	my $h = $f->Row;
	$h->{Geometry} = $h->{Geometry}->AsText;
	if ($i == 0) {
	    my @t = (0,13,undef,undef,'POINT (1 2)');
	    ok(is_deeply(\@a, \@t), "layer create test 1");
	} else {
	    my %t = (FID => 1, Geometry => 'POINT (3 2)', test1 => undef, test2 => '31a', test3 => undef);
	    ok(is_deeply($h, \%t), "layer create test 2");
	}
	$i++;
    }
    $layer->Row(FID=>0, Geometry=>{ Points => [5,6] }, test3 => 6.5);
    my @t = $layer->Tuple(0);
    ok($t[3] == 6.5, "layer row and tuple");
    ok($t[4]->ExportToWkt eq 'POINT (5 6)', "layer row and tuple");
}

my $osr = Geo::OSR::SpatialReference->new(WGS => 84);

@types = Geo::OGR::GeometryType();

my @tmp = @types;
@types = ();
for (@tmp) {
    my $a = Geo::OGR::GeometryType($_);
    my $b = Geo::OGR::GeometryType($a);
    #ok($_ eq $b, "geometry type back and forth");
    next if /25/;
    next if /Ring/;
    next if /None/;
    push @types, $_;
}
