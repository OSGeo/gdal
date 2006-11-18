use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

use vars qw/%known_driver $loaded $verbose @types %pack_types %types @fails/;

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

%known_driver = ('ESRI Shapefile' => 1,
		 'MapInfo File' => 1,'UK .NTF' => 1,'SDTS' => 1,'TIGER' => 1,
		 'S57' => 1,'DGN' => 1,'VRT' => 1,'AVCBin' => 1,'REC' => 1,
		 'Memory' => 1,'CSV' => 1,'GML' => 1,'OGDI' => 1,'PostgreSQL' => 1);

my $osr = new Geo::OSR::SpatialReference;
$osr->SetWellKnownGeogCS('WGS84');

@types = ('wkbUnknown','wkbPoint','wkbLineString','wkbPolygon',
	  'wkbMultiPoint','wkbMultiLineString','wkbMultiPolygon','wkbGeometryCollection');

%types = ();

for (@types) {$types{$_} = eval "\$Geo::OGR::$_"};

ogr_tests(Geo::OGR::GetDriverCount(),$osr);

if (@fails) {
    print STDERR "unexpected failures: (shapefile integer type error is bug #933)\n",@fails;
    print STDERR "all other tests ok.\n";
} else {
    print STDERR "all tests ok.\n";
}

system "rm -rf tmp_ds_*" unless $^O eq 'MSWin32';

###########################################
#
# only subs below
#
###########################################

sub ogr_tests {
    my($nr_drivers_tested,$osr) = @_;
    
    for my $i (0..$nr_drivers_tested-1) {
	
	my $driver = Geo::OGR::GetDriver($i);
	unless ($driver) {
	    mytest('',undef,"Geo::OGR::GetDriver($i)");
	    next;
	}
	my $name = $driver->{name};
#	print "$name\n";
	mytest('skipped: not tested',undef,$name,'test') unless $known_driver{$name};
	
	if (!$driver->TestCapability($Geo::OGR::ODrCCreateDataSource)) {
	    mytest('skipped: no capability',undef,$name,'datasource create');
	    next;
	}
	
	if ($name eq 'S57' or $name eq 'CSV' or $name eq 'GML' or $name eq 'PostgreSQL') {
	    mytest('skipped: apparently no capability',undef,$name,'datasource create');
	    next;
	}
	
	if ($name eq 'TIGER' or $name eq 'DGN') {
	    mytest("skipped: can't create layers afterwards.",undef,$name,'datasource create');
	    next;
	}

	my @field_types = ('OFTInteger','OFTIntegerList','OFTReal','OFTRealList','OFTString',
			   'OFTStringList','OFTWideString','OFTWideStringList','OFTBinary');
	
	if ($name eq 'ESRI Shapefile') {
	    @field_types = ('OFTInteger','OFTReal','OFTString','OFTInteger');
	} elsif ($name eq 'MapInfo File') {
	    @field_types = ('OFTInteger','OFTReal','OFTString');
	}
	
	my %field_types;
	for (@field_types) {$field_types{$_} = eval "\$Geo::OGR::$_"};
	
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
	    
	    if ($name eq 'ESRI Shapefile' and $type eq 'wkbGeometryCollection') {
		mytest("skipped, will fail",undef,$name,$type,'layer create');
		next;
	    }
	    
	    if ($type eq 'wkbMultiPolygon') {
		mytest("skipped, no test yet",undef,$name,$type,'layer create');
		next;
	    }

	    if ($name eq 'MapInfo File' and $type eq 'wkbMultiLineString') {
		mytest("skipped, no test",undef,$name,$type,'layer create');
		next;
	    }
	    
	    my $layer;
	    eval {
		$layer = $datasource->CreateLayer($type, $osr, $types{$type});
	    };
	    mytest($layer,'no message',$name,$type,'layer create');
	    
	    next unless $layer;
	    
	    # create one field of each type
	    
	    for my $ft (@field_types) {
		
		my $column = new Geo::OGR::FieldDefn($ft, $field_types{$ft});
		
		$layer->CreateField($column);
		
	    }
	    
	    {
		my $schema = $layer->GetLayerDefn();
		
		$i = 0;
		for my $ft (@field_types) {
		    
		    my $column = $schema->GetFieldDefn($i++);
		    my $n = $column->GetName;
		    mytest($n eq $ft,"$n ne $ft",$name,$type,$ft,'field create');
		    
		}
		
		my $feature = new Geo::OGR::Feature($schema);
		
		my $t = $type eq 'wkbUnknown' ? $Geo::OGR::wkbPolygon : $types{$type};

		my $geom = new Geo::OGR::Geometry($t);

		if ($type eq 'wkbMultiPoint') {

		    for (0..1) {
			my $g = new Geo::OGR::Geometry($Geo::OGR::wkbPoint);
			test_geom($g,$name,'wkbPoint','create');
			$geom->AddGeometry($g);
		    }

		} elsif ($type eq 'wkbMultiLineString') {

		    for (0..1) {
			my $g = new Geo::OGR::Geometry($Geo::OGR::wkbLineString);
			test_geom($g,$name,'wkbLineString','create');
			$geom->AddGeometry($g);
		    }

		} else {
		
		    test_geom($geom,$name,$type,'create');

		}
		
		$feature->SetGeometry($geom);
		
		$i = 0;
		for my $ft (@field_types) {
		    my $v = 2;
		    $v = 'kaksi' if $ft eq 'OFTString';
		    $feature->SetField($i++,$v);
		}
		
		$layer->CreateFeature($feature);
		$layer->SyncToDisk;
		
	    }
	    
	    undef $layer;
	    
	    # now open
	    
	    if ($name eq 'Memory')
	    {
		mytest('skipped',undef,$name,$type,'layer open');
		
	    } else {
		
		undef $datasource;
		
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
		
		# check to see if the fields exist and the types are the same
		
		my $schema = $layer->GetLayerDefn();
		
		$i = 0;
		for my $ft (@field_types) {
		    my $column = $schema->GetFieldDefn($i++);
		    my $n = $column->GetName;
		    mytest($n eq $ft,"$n ne $ft",$name,$type,$ft,'GetName');
		    my $t = $column->GetType;
		    my $t2 = $field_types{$ft};
		    mytest($t == $t2,"$t != $t2",$name,$type,$ft,'GetType');
		}
		
		if ($type eq 'wkbPoint' or $type eq 'wkbLineString' or $type eq 'wkbPolygon') {
		    
		    $layer->ResetReading;
		    my $feature = $layer->GetNextFeature;
		    
		    mytest($feature,'GetFeature failed',$name,$type,'GetNextFeature');
		    
		    if ($feature) {
			
			my $geom = $feature->GetGeometryRef();
			
			if ($type eq 'wkbPointlll') {
			    mytest('skipped',undef,$name,$type,'geom open');
			} else {
			    my $t = $type eq 'wkbUnknown' ? $Geo::OGR::wkbPolygon : $types{$type};
			    my $t2 = $geom->GetGeometryType;
			    mytest($t == $t2,"$t != $t2",$name,$type,'geom open');

			    if ($type eq 'wkbMultiPoint') {

				my $gn = $geom->GetGeometryCount;
				mytest($gn == 2,"$gn != 2",$name,$type,'geom count');

				for my $i (0..1) {
				    my $g = $geom->GetGeometryRef($i);
				    test_geom($g,$name,'wkbPoint','open');
				}

			    } elsif ($type eq 'wkbMultiLineString') {

				my $gn = $geom->GetGeometryCount;
				mytest($gn == 2,"$gn != 2",$name,$type,'geom count');

				for my $i (0..1) {
				    my $g = $geom->GetGeometryRef($i);
				    test_geom($g,$name,'wkbLineString','open');
				}
				
			    } else {
				test_geom($geom,$name,$type,'open');
			    }
			}
			
			$i = 0;
			for my $ft (@field_types) {
			    #$feature->SetField($i++,2);
			    my $f;
			    if ($ft eq 'OFTString') {
				$f = $feature->GetFieldAsString($i);
				mytest($f eq 'kaksi',"$f ne 'kaksi'",$name,$type,'GetFieldAsString');
			    } else {
				$f = $feature->GetFieldAsInteger($i);
				mytest($f == 2,"$f != 2",$name,$type,'GetFieldAsInteger');
				$f = $feature->GetFieldAsDouble($i);
				mytest($f == 2,"$f != 2",$name,$type,'GetFieldAsDouble');
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
}

sub test_geom {
    my($geom,$name,$type,$mode) = @_;

    my $pc = $geom->GetPointCount;
    my $gn = $geom->GetGeometryCount;
    my $i = 0;

    if ($type eq 'wkbPoint') {

	if ($mode eq 'create') {
	    $geom->AddPoint(1,1);
	} else {
	    mytest($pc == 1,"$pc != 1",$name,$type,'point count');
	    mytest($gn == 0,"$gn != 0",$name,$type,'geom count');
	    my @xy = ($geom->GetX($i),$geom->GetY($i));
	    mytest(cmp_ar(2,\@xy,[1,1]),"(@xy) != (1,1)",$name,$type,"get point");
	}

    } elsif ($type eq 'wkbLineString') {

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

    } elsif ($type eq 'wkbUnknown' or $type eq 'wkbPolygon') {

	my @pts = ([1.1,1],[1.11,0],[0,0.2],[0,2.1],[1,1.23],[1.1,1]);

	if ($mode eq 'create') {
	    my $r = new Geo::OGR::Geometry($Geo::OGR::wkbLinearRing);
	    pop @pts;
	    for my $pt (@pts) {
		$r->AddPoint(@$pt);
	    }
	    $geom->AddGeometry($r);
	    $geom->CloseRings; # this overwrites the last point
	} else {
	    mytest($gn == 1,"$gn != 1",$name,$type,'geom count');
	    my $r = $geom->GetGeometryRef(0);
	    $pc = $r->GetPointCount;
	    mytest($pc == 6,"$pc != 6",$name,$type,'point count');
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

