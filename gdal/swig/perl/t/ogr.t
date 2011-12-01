use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

use strict;
use vars qw/%available_driver %test_driver $loaded $verbose @types %pack_types @fails @tested_drivers/;

$loaded = 1;

$verbose = $ENV{VERBOSE};

# tests:
#
# for pre-tested OGR drivers:
#   Create datasource
#   Create layer
#   Create field
#   Create geometry
#   Open layer
#   Open field
#   Open geom
#   Cmp points
#
# not yet tested
#   transactions
#   GEOS methods
#   osr
#   XML typemaps
#
# if verbose = 1, all operations (skip,fail,ok) are printed out

system "rm -rf tmp_ds_*" unless $^O eq 'MSWin32';

%test_driver = ('ESRI Shapefile' => 1,
		'MapInfo File' => 1,
		'Memory' => 1,
		);

{
    # test conversion methods
    my $g = Geo::OGR::Geometry->create(WKT=>'POINT (1 1)');
    my $x = Geo::OGR::Geometry->create(WKT=>'POINT (2 2)');
    my $g2 = $g->ForceToMultiPoint($x);
    ok($g2->AsText eq 'MULTIPOINT (1 1,2 2)', 'ForceToMultiPoint');
    $g = Geo::OGR::Geometry->create(WKT=>'LINESTRING (1 1,2 2)');
    $x = Geo::OGR::Geometry->create(WKT=>'LINESTRING (2 2,3 3)');
    $g2 = $g->ForceToMultiLineString($x);
    ok($g2->AsText eq 'MULTILINESTRING ((1 1,2 2),(2 2,3 3))', 'ForceToMultiLineString');
    $g = Geo::OGR::Geometry->create(WKT=>'POLYGON ((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5))');
    $x = Geo::OGR::Geometry->create(WKT=>'POLYGON ((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5))');
    $g2 = $g->ForceToMultiPolygon($x);
    ok($g2->AsText eq 'MULTIPOLYGON (((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5)),((0.49 0.5,0.83 0.5,0.83 0.77,0.49 0.77,0.49 0.5)))', 'ForceToMultiPolygon');
    $g = Geo::OGR::Geometry->create(WKT=>'POINT (1 1)');
    $x = Geo::OGR::Geometry->create(WKT=>'LINESTRING (2 2,3 3)');
    $g2 = $g->ForceToCollection($x);
    ok($g2->AsText eq 'GEOMETRYCOLLECTION (POINT (1 1),LINESTRING (2 2,3 3))', 'ForceToCollection');
    my @g = $g2->Dissolve;
    ok($g[0]->AsText eq 'POINT (1 1)', 'Dissolve point');
    ok($g[1]->AsText eq 'LINESTRING (2 2,3 3)', 'Dissolve line string');
}

{
    my $g = Geo::OGR::Geometry->create(wkt => "point(1 2)");
    $g->Point(2,3);
    my @p = $g->Point;
    ok($p[0] == 2, "Point");
    eval {
	my $wkb = "0102000000050000005555555524F3484100000000A0F05941555555552".
	    "4F34841000000E09CF05941000000C02EF34841ABAAAAAA97F05941ABAAAA2A39F3".
	    "4841000000E09CF05941ABAAAA2A39F3484100000000A0F05941";
	$g = Geo::OGR::Geometry->create(hexwkb => $wkb);
    };
    ok ($@ eq '', "create from WKb: $@");
    eval {
	my $gml = "<gml:Point><gml:coordinates>1,1</gml:coordinates></gml:Point>";
	$g = Geo::OGR::Geometry->create(gml => $gml);
    };
    ok ($@ eq '', "create from GML: $@");
    eval {
	$g = Geo::OGR::Geometry->create(GeoJSON => "abc");
    };
    ok ($@ =~ /GeoJSON parsing error/, "create from GeoJSON: $@");
}

{
    # test the Points method
    my $g = Geo::OGR::Geometry->create( WKT => 'POINT(1.1 2.2)');
    my $p = $g->Points;
    $g->Points($p);
    my $q = $g->Points;
    is_deeply($p, $q, "Points with a point");
    $g = Geo::OGR::Geometry->create(wkt => "linestring(1 1, 1 2, 2 2)");
    $p = $g->Points;
    $g->Points($p);
    $q = $g->Points;
    is_deeply($p, $q, "Points with a linestring");
}

{
    my $g = Geo::OGR::Geometry->create(wkt => "linestring(1 1, 1 2, 2 2)");
    ok ($g->Length == 2, "Length 1");
}

{
    # test list valued fields
    my $d = Geo::OGR::FeatureDefn->new;
    $d->Schema(Fields=>[
			{ Name => 'ilist',
			  Type => 'IntegerList',
		      },
			{ Name => 'rlist',
			  Type => 'RealList',
		      },
			{ Name => 'slist',
			  Type => 'StringList',
		      },
			{ Name => 'date',
			  Type => 'Date',
		      },
			{ Name => 'time',
			  Type => 'Time',
		      },
			{ Name => 'datetime',
			  Type => 'DateTime',
		      },
			]
	       );
    my $f = Geo::OGR::Feature->new($d);
    ok($f->Schema->{Fields}->[1]->{Index} == 1, "Index in field in schema");
    
    $f->Row( ilist => [1,2,3],
	     rlist => [1.1,2.2,3.3],
	     slist => ['a','b','c'],
	     date => [2008,3,23],
	     time => [12,55,15],
	     datetime => [2008,3,23,12,55,20],
	     );
    my @test;
    @test = $f->GetField('ilist');
    ok(is_deeply(\@test, [1,2,3]), 'integer list');
    @test = $f->GetField('rlist');
    ok(is_deeply(\@test, [1.1,2.2,3.3]), 'double list');
    @test = $f->GetField('slist');
    ok(is_deeply(\@test, ['a','b','c']), 'string list');
    @test = $f->GetField('date');
    ok(is_deeply(\@test, [2008,3,23]), 'date');
    @test = $f->GetField('time');
    ok(is_deeply(\@test, [12,55,15,0]), 'time');
    @test = $f->Field('datetime');
    ok(is_deeply(\@test, [2008,3,23,12,55,20,0]), 'datetime');
}

{
    my $g2;
    {
	my $d = Geo::OGR::FeatureDefn->new;
	$d->Schema(Fields=>[Geo::OGR::FieldDefn->create(Name=>'Foo')]);
	my $f = Geo::OGR::Feature->new($d);
	my $g = Geo::OGR::Geometry->create('Point');
	$f->SetGeometry($g);
	my $fd = $f->GetDefnRef;
	my $s = $fd->Schema;
	my $s2 = $s->{Fields}[0];
	ok($s->{GeometryType} eq 'Unknown', 'Feature defn schema 0');
	ok($s2->{Name} eq 'Foo', 'Feature defn schema 1');
	ok($s2->{Type} eq 'String', 'Feature defn schema 2');
	$g2 = $f->GetGeometry;
    }
    $g2->GetDimension; # does not cause a kaboom
}
{
    my $f = Geo::OGR::FieldDefn->create();
    my $s = $f->Schema(Width => 10);
    ok($s->{Width} == 10, 'fieldefn schema 1');
    ok($s->{Type} eq 'String', 'fieldefn schema 2');
}
{
    my $driver = Geo::OGR::Driver('Memory');
    my @cap = $driver->Capabilities;
    ok(is_deeply(\@cap, ['CreateDataSource']), "driver capabilities");
    my $datasource = $driver->CreateDataSource('test');
    @cap = $datasource->Capabilities;
    ok(is_deeply(\@cap, ['CreateLayer','DeleteLayer']), "data source capabilities");
    
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
    
    $layer = $datasource->CreateLayer('test', undef, 'Point');
    $layer->Schema(Fields => 
		   [{Name => 'test1', Type => 'Integer'},
		    {Name => 'test2', Type => 'String'},
		    {Name => 'test3', Type => 'Real'}
		    ], ApproxOK => 1);
    $layer->InsertFeature({ test1 => 13, 
			    Geometry => { Points => [1,2,3] } });
    $layer->InsertFeature({ test2 => '31a', 
			    Geometry => { Points => [3,2] } });
    $layer->ResetReading;
    my $i = 0;
    while (my $f = $layer->GetNextFeature) {
	my @a = $f->Tuple;
	$a[1] = $a[1]->AsText;
	my $h = $f->Row;
	$h->{Geometry} = $h->{Geometry}->AsText;
	if ($i == 0) {
	    my @t = (0,'POINT (1 2)',13,undef,undef);
	    ok(is_deeply(\@a, \@t), "layer create test 1");
	} else {
	    my %t = (FID => 1, Geometry => 'POINT (3 2)', test1 => undef, test2 => '31a', test3 => undef);
	    ok(is_deeply($h, \%t), "layer create test 2");
	}
	$i++;
    }
    $layer->Row(FID=>0, Geometry=>{ Points => [5,6] }, test3 => 6.5);
    my @t = $layer->Tuple(0);
    ok($t[4] == 6.5, "layer row and tuple");
    ok($t[1]->ExportToWkt eq 'POINT (5 6)', "layer row and tuple");
}

my $osr = new Geo::OSR::SpatialReference;
$osr->SetWellKnownGeogCS('WGS84');

@types = Geo::OGR::GeometryType();

ok(@types == 17, "number of geometry types is 17");

my @tmp = @types;
@types = ();
for (@tmp) {
    my $a = Geo::OGR::GeometryType($_);
    my $b = Geo::OGR::GeometryType($a);
    ok($_ eq $b, "geometry type back and forth");
    next if /25/;
    next if /Ring/;
    next if /None/;
    push @types, $_;
}

ogr_tests($osr);

my $methods = Geo::OSR::GetProjectionMethods;

for my $method (@$methods) {
    my($params, $name) = Geo::OSR::GetProjectionMethodParameterList($method);
    ok(ref($params) eq 'ARRAY', "$method: GetProjectionMethodParameterList params, out=($params, $name)");
    ok($name ne '', "$method: GetProjectionMethodParameterList name");
    next if $method =~ /^International_Map_of_the_World/; # there is a bug in there...
    for my $parameter (@$params) {
	my($usrname, $type, $defaultval) = Geo::OSR::GetProjectionMethodParamInfo($method, $parameter);
	ok($usrname ne '', "$method $parameter: GetProjectionMethodParamInfo username");
	ok($type ne '', "$method $parameter: GetProjectionMethodParamInfo type");
	ok($defaultval ne '', "$method $parameter: GetProjectionMethodParamInfo defval");
    }
}

@tmp = sort keys %available_driver;

if (@fails) {
    print STDERR "\nUnexpected failures:\n",@fails;
    print STDERR "\nAvailable drivers were ",join(', ',@tmp),"\n";
    print STDERR "Drivers used in tests were: ",join(', ',@tested_drivers),"\n";
} else {
    print STDERR "\nAvailable drivers were ",join(', ',@tmp),"\n";
    print STDERR "Drivers used in tests were: ",join(', ',@tested_drivers),"\n";
}

system "rm -rf tmp_ds_*" unless $^O eq 'MSWin32';

###########################################
#
# only subs below
#
###########################################

sub ogr_tests {
    my($osr) = @_;
    for my $driver (Geo::OGR::Drivers) {
	my $name = $driver->{name};
	
	unless (defined $name) {
	    $name = 'unnamed';
	    my $i = 1;
	    while ($available_driver{$name}) {
		$name = 'unnamed '.$i;
		$i++;
	    }
	}

	$available_driver{$name} = 1;
	mytest('skipped: not tested',undef,$name,'test'),next unless $test_driver{$name};
	
	if (!$driver->TestCapability('CreateDataSource')) {
	    mytest('skipped: no capability',undef,$name,'datasource create');
	    next;
	}
	
	if ($name eq 'KML' or 
	    $name eq 'S57' or 
	    $name eq 'CSV' or 
	    $name eq 'GML' or 
	    $name eq 'PostgreSQL' or 
	    $name =~ /^Interlis/ or 
	    $name eq 'SQLite' or 
	    $name eq 'MySQL') 
	{
	    mytest('skipped: apparently no capability',undef,$name,'datasource create');
	    next;
	}
	
	if ($name eq 'TIGER' or $name eq 'DGN') {
	    mytest("skipped: can't create layers afterwards.",undef,$name,'datasource create');
	    next;
	}

	push @tested_drivers,$name;

	my @field_types = (qw/Integer IntegerList Real RealList String 
			   StringList Binary/);
	
	if ($name eq 'ESRI Shapefile') {
	    @field_types = (qw/Integer Real String/);
	} elsif ($name eq 'MapInfo File') {
	    @field_types = (qw/Integer Real String/);
	}
	
	my $dir0 = $name;
	$dir0 =~ s/ //g;
	my $dir = "tmp_ds_$dir0";
	system "mkdir $dir" unless $name eq 'Memory';
	
	my $datasource;
	eval {
	    $datasource = $driver->CreateDataSource($dir);
	};
	mytest($datasource,'no message',$name,'datasource create');
	
	next unless $datasource;
	
	for my $type (@types) {
	    
	    if ($name eq 'ESRI Shapefile' and $type eq 'GeometryCollection') {
		mytest("skipped, will fail",undef,$name,$type,'layer create');
		next;
	    }
	    
	    if ($type eq 'MultiPolygon') {
		mytest("skipped, no test yet",undef,$name,$type,'layer create');
		next;
	    }

	    if ($name eq 'MapInfo File' and $type eq 'MultiLineString') {
		mytest("skipped, no test",undef,$name,$type,'layer create');
		next;
	    }
	    
	    my $layer;
	    eval {
		$layer = $datasource->CreateLayer($type, $osr, $type);
	    };
	    mytest($layer,'no message',$name,$type,'layer create');
	    
	    next unless $layer;
	    
	    # create one field of each type
	    
	    for my $ft (@field_types) {
		
		my $column = Geo::OGR::FieldDefn->create($ft, $ft);
		$column->SetWidth(5) if $ft eq 'Integer';
		$layer->CreateField($column);
		
	    }
	    
	    {
		my $schema = $layer->GetLayerDefn();
		
		my $i = 0;
		for my $ft (@field_types) {
		    
		    my $column = $schema->GetFieldDefn($i++);
		    my $n = $column->GetName;
		    mytest($n eq $ft,"$n ne $ft",$name,$type,$ft,'field create');
		    
		}
		
		my $feature = new Geo::OGR::Feature($schema);
		
		my $t = $type eq 'Unknown' ? 'Polygon' : $type;

		my $geom = Geo::OGR::Geometry->create($t);
		
		if ($type eq 'MultiPoint') {

		    for (0..1) {
			my $g = Geo::OGR::Geometry->create('Point');
			test_geom($g,$name,'Point','create');
			$geom->AddGeometry($g);
		    }

		} elsif ($type eq 'MultiLineString') {

		    for (0..1) {
			my $g = Geo::OGR::Geometry->create('LineString');
			test_geom($g,$name,'LineString','create');
			$geom->AddGeometry($g);
		    }

		} else {
		
		    test_geom($geom,$name,$type,'create');

		}
		
		$feature->SetGeometry($geom);
		
		$i = 0;
		for my $ft (@field_types) {
		    my $v = 2;
		    $v = 'kaksi' if $ft eq 'String';
		    $feature->SetField($i++,$v);
		}
		
		$layer->CreateFeature($feature);
		#$layer->SyncToDisk;
		
	    }
	    
	    undef $layer if $name ne 'Memory';
	    
	    # now open
	    
	    if ($name eq 'MemoryXX')
	    {
		mytest('skipped',undef,$name,$type,'layer open');
		
	    } else {
		
		undef $datasource if $name ne 'Memory';
		
		if ($name ne 'Memory') {
		    eval {
			if ($name eq 'MapInfo File') {
			    $datasource = Geo::OGR::Open("$dir/$type.tab");
			    $layer = $datasource->GetLayerByIndex;
			} else {
			    $datasource = $driver->CreateDataSource($dir);
			    $layer = $datasource->GetLayerByName($type);
			}
		    };
		    
		    mytest($layer,'no message',$name,$type,"layer $type open");
		    next unless $layer;
		}
		
		# check to see if the fields exist and the types are the same
		
		my $schema = $layer->GetLayerDefn();
		
		my $i = 0;
		for my $ft (@field_types) {
		    my $column = $schema->GetFieldDefn($i++);
		    my $n = $column->GetName;
		    mytest($n eq $ft,"$n ne $ft",$name,$type,$ft,'GetName');
		    my $t2 = $column->Type;
		    mytest($ft eq $t2,"$ft ne $t2",$name,$type,$ft,'Type');
		}
		
		if ($type eq 'Point' or $type eq 'LineString' or $type eq 'Polygon') {
		    
		    $layer->ResetReading;
		    my $feature = $layer->GetNextFeature;
		    
		    mytest($feature,'GetFeature failed',$name,$type,'GetNextFeature');
		    
		    if ($feature) {
			
			my $geom = $feature->GetGeometryRef();
			
			if ($type eq 'Pointxx') {
			    mytest('skipped',undef,$name,$type,'geom open');
			} else {
			    my $t = $type eq 'Unknown' ? 'Polygon' : $type;
			    my $t2 = $geom->GeometryType;
			    mytest($t eq $t2,"$t ne $t2",$name,$type,'geom open');

			    if ($type eq 'MultiPoint') {

				my $gn = $geom->GetGeometryCount;
				mytest($gn == 2,"$gn != 2",$name,$type,'geom count');

				for my $i (0..1) {
				    my $g = $geom->GetGeometryRef($i);
				    test_geom($g,$name,'Point','open');
				}

			    } elsif ($type eq 'MultiLineString') {

				my $gn = $geom->GetGeometryCount;
				mytest($gn == 2,"$gn != 2",$name,$type,'geom count');

				for $i (0..1) {
				    my $g = $geom->GetGeometryRef($i);
				    test_geom($g,$name,'LineString','open');
				}
				
			    } else {
				test_geom($geom,$name,$type,'open');
			    }
			}
			
			my $i = 0;
			for my $ft (@field_types) {
			    next if $name eq 'Memory' and $ft ne 'Integer'; 
			    #$feature->SetField($i++,2);
			    my $f;
			    if ($ft eq 'String') {
				$f = $feature->GetField($i);
				mytest($f eq 'kaksi',"$f ne 'kaksi'",$name,$type,"$ft GetField");
			    } else {
				$f = $feature->GetField($i);
				mytest($f == 2,"$f != 2",$name,$type,"$ft GetField");
				#$f = $feature->GetField($i);
				#mytest($f == 2,"$f != 2",$name,$type,'GetField');
			    }
			    $i++;
			}
			
		    }
		} else {
		    mytest('skipped',undef,$name,$type,'feature open');
		}
		
		undef $layer;
	    }
	    
	}
		
    }

    # specific tests:

    my $geom = Geo::OGR::Geometry->create('Point');
    $geom->AddPoint(1,1);
    ok($geom->GeometryType eq 'Point', "Add 2D Point");
    $geom->AddPoint(1,1,1);
    ok($geom->GeometryType eq 'Point25D', "Add 3D Point upgrades geom type");
    
}

sub test_geom {
    my($geom,$name,$type,$mode) = @_;

    my $pc = $geom->GetPointCount;
    my $gn = $geom->GetGeometryCount;
    my $i = 0;

    if ($type eq 'Point') {

	if ($mode eq 'create') {
	    $geom->AddPoint(1,1);
	    $geom->SetPoint_2D(0,1,1);
	    my @p = $geom->GetPoint;
	    ok(is_deeply(\@p, [1,1]), "GetPoint");
	} else {
	    mytest($pc == 1,"$pc != 1",$name,$type,'point count');
	    mytest($gn == 0,"$gn != 0",$name,$type,'geom count');
	    my @xy = ($geom->GetX($i),$geom->GetY($i));
	    mytest(cmp_ar(2,\@xy,[1,1]),"(@xy) != (1,1)",$name,$type,"get point");
	}
	
    } elsif ($type eq 'LineString') {
	
	if ($mode eq 'create') {
	    $geom->AddPoint(1,1);
	    $geom->AddPoint(2,2);
	} else {
	    mytest($pc == 2,"$pc != 2",$name,$type,'point count');
	    mytest($gn == 0,"$gn != 0",$name,$type,'geom count');
	    my @xy = ($geom->GetX($i),$geom->GetY($i)); $i++;
	    mytest(cmp_ar(2,\@xy,[1,1]),"(@xy) != (1,1)",$name,$type,"get point");
	    @xy = ($geom->GetX($i),$geom->GetY($i));
	    mytest(cmp_ar(2,\@xy,[2,2]),"(@xy) != (2,2)",$name,$type,"get point");
	}

    } elsif ($type eq 'Unknown' or $type eq 'Polygon') {

	my @pts = ([1.1,1],[1.11,0],[0,0.2],[0,2.1],[1,1.23],[1.1,1]);

	if ($mode eq 'create') {
	    my $r = Geo::OGR::Geometry->create('LinearRing');
	    pop @pts;
	    my $n = @pts;
	    for my $pt (@pts) {
		$r->AddPoint(@$pt);
	    }
	    $pc = $r->GetPointCount;
	    mytest($pc == $n,"$pc != $n",$name,$type,'point count pre');
	    $geom->AddGeometry($r);
	    $geom->CloseRings; # this adds the point we popped out
	    $n++;
	    $r = $geom->GetGeometryRef(0);
	    $pc = $r->GetPointCount;
	    for (0..$pc-1) {
		my @p = $r->GetPoint($_);
	    }
	    mytest($pc == $n,"$pc != $n",$name,$type,'point count post 1');
	} else {	       
	    mytest($gn == 1,"$gn != 1",$name,$type,'geom count');
	    my $r = $geom->GetGeometryRef(0);
	    $pc = $r->GetPointCount;
	    my $n = @pts;
	    mytest($pc == $n,"$pc != $n",$name,$type,'point count post  2');
	    for my $cxy (@pts) {
		my @xy = ($r->GetX($i),$r->GetY($i)); $i++;
		mytest(cmp_ar(2,\@xy,$cxy),"(@xy) != (@$cxy)",$name,$type,"get point $i");
	    }
	}

    } else {
	mytest('skipped',undef,$name,$type,'geom create/open');
    }
}

sub cmp_ar {
    my($n,$a1,$a2) = @_;
    return 0 unless $n == @$a1;
    return 0 unless $#$a1 == $#$a2;
    for my $i (0..$#$a1) {
	return 0 unless abs($a1->[$i] - $a2->[$i]) < 0.001;
    }
    return 1;
}

sub mytest {
    my $test = shift;
    my $msg = shift;
    my $context = join(': ',@_);
    ok($test, $context);
    unless ($test) {
	my $err = $msg;
	if ($@) {
	    $@ =~ s/\n/ /g;
	    $@ =~ s/\s+$//;
	    $@ =~ s/\s+/ /g;
	    $@ =~ s/^\s+$//;
	    $err = $@ ? "'$@'" : $msg;
	}
	$msg = "$context: $err: not ok\n";
	push @fails,$msg;
    } elsif ($test =~ /^skip/) {
	$msg = "$context: $test.\n";
    } else {
	$msg = "$context: ok.\n";
    }
    print $msg if $verbose;
    return $msg;
}

sub dumphash {
    my $h = shift;
    for (keys %$h) {
	print "$_ $h->{$_}\n";
    }
}

