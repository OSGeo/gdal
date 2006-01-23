BEGIN { $| = 1; }
END {print "not ok 1\n" unless $loaded;}
use strict;
use gdal;
use gdalconst;
use osr;
use ogr;
use vars qw/$loaded $verbose @types %pack_types %types @fails/;

$loaded = 1;

$verbose = 0;

# tests:
#
# for explicitly specified raster types: 
#   Create dataset
#   Get/SetGeoTransform
#   Get/SetNoDataValue
#   Colortable operations
#   WriteRaster
#   Open dataset
#   ReadRaster
#   GCPs
#
# not yet tested:
#   Overviews
#
# for all available OGR drivers:
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

#system "rm -rf tmp_ds_*";

@types = ('GDT_Byte','GDT_UInt16','GDT_Int16','GDT_UInt32','GDT_Int32',
	  'GDT_Float32','GDT_Float64','GDT_CInt16','GDT_CInt32','GDT_CFloat32','GDT_CFloat64');

%pack_types = ('GDT_Byte'=>'c',
	       'GDT_Int16'=>'s',
	       'GDT_Int32'=>'i',
	       'GDT_Float32'=>'f',
	       'GDT_Float64'=>'d',
	       );

for (@types) {$types{$_} = eval "\$gdalconst::$_"};

my %no_colortable = map {$_=>1} ('ELAS','BMP','ILWIS','BT','RMF','NITF');

my %no_nodatavalue = map {$_=>1} ('NITF','HFA','ELAS','BMP','ILWIS','BT','IDA','RMF');

my %no_geotransform = map {$_=>1} ('NITF','PAux','PNM','MFF','ENVI','BMP','EHdr');

my %no_setgcp = map {$_=>1} ('HFA','ELAS','MEM','BMP','PCIDSK','ILWIS','PNM','ENVI',
			     'NITF','EHdr','MFF','MFF2','BT','IDA','RMF');

my %no_open = map {$_=>1} ('VRT','MEM','ILWIS','MFF2');

gdal_tests(gdal::GetDriverCount());

my $osr = new osr::SpatialReference;
$osr->SetWellKnownGeogCS('WGS84');

@types = ('wkbUnknown','wkbPoint','wkbLineString','wkbPolygon',
	  'wkbMultiPoint','wkbMultiLineString','wkbMultiPolygon','wkbGeometryCollection');

%types = ();

for (@types) {$types{$_} = eval "\$ogr::$_"};

ogr_tests(ogr::GetDriverCount(),$osr);

if (@fails) {
    print "unexpected failures: (shapefile integer type error is bug #933)\n",@fails;
    print "all other tests ok.\n";
} else {
    print "all tests ok.\n";
}

system "rm -rf tmp_ds_*";

###########################################
#
# only subs below
#
###########################################

sub gdal_tests {
    my $nr_drivers_tested = shift;

    for my $i (0..$nr_drivers_tested-1) {

	my $driver = gdal::GetDriver($i);
	unless ($driver) {
	    mytest('',undef,"gdal::GetDriver($i)");
	    next;
	}

	my $name = $driver->{ShortName};

        next if $name eq 'MFF2'; # does not work probably because of changes in hkvdataset.cpp
	
	my $metadata = $driver->GetMetadata();
	
	unless ($metadata->{DCAP_CREATE} eq 'YES') {
	    mytest('skipped: no capability',undef,$name,'dataset create');
	    next;
	}
	
	my @create = split /\s+/,$metadata->{DMD_CREATIONDATATYPES};
	
	@create = ('Byte','Float32','UInt16','Int16','CInt16','CInt32','CFloat32') 
	    if $driver->{ShortName} eq 'MFF2';
	
	unless (@create) {
	    mytest('skipped: no creation datatypes',undef,$name,'dataset create');
	    next;
	}
	
	if ($driver->{ShortName} eq 'PAux') {
	    mytest('skipped: does not work?',undef,$name,'dataset create');
	    next;
	}
	
	my $ext = '.'.$metadata->{DMD_EXTENSION};
	$ext = '' if $driver->{ShortName} eq 'ILWIS';
	
	for my $type (@create) {
	    
	    if (($driver->{ShortName} eq 'MFF2') and ($type eq 'CInt32')) {
		mytest('skipped: does not work?',undef,$name,$type,'dataset create');
		next;
	    }
	    
	    my $typenr = $types{'GDT_'.$type};
	    
	    my $filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
	    my $width = 100;
	    my $height = 50;
	    my $bands = 1;
	    my $options = undef;
	    
	    my $dataset;
	    eval {
		$dataset = $driver->Create($filename, $width, $height, $bands , $typenr, []);
	    };
	    mytest($dataset,'no error message',$name,$type,'dataset create');
	    next unless $dataset;
	    
	    mytest($dataset->{RasterXSize} == $width,'RasterXSize',$name,$type,'RasterXSize');
	    mytest($dataset->{RasterYSize} == $height,'RasterYSize',$name,$type,'RasterYSize');
	    
	    my $band = $dataset->GetRasterBand(1);
	    
	    if ($no_geotransform{$driver->{ShortName}}) 
	    {
		mytest('skipped',undef,$name,$type,'Get/SetGeoTransform');

	    } else
	    {
		my $transform = $dataset->GetGeoTransform();
		$transform->[5] = 12;
		$dataset->SetGeoTransform($transform);
		my $transform2 = $dataset->GetGeoTransform();
		mytest($transform->[5] == $transform2->[5],
		       "$transform->[5] != $transform2->[5]",$name,$type,'Get/SetGeoTransform');
	    }
	    
	    if ($no_nodatavalue{$driver->{ShortName}}) 
	    {
		mytest('skipped',undef,$name,$type,'Get/SetNoDataValue');
		
	    } else
	    {
		$band->SetNoDataValue(5);
		my $value = $band->GetNoDataValue;
		mytest($value == 5,"$value != 5",$name,$type,'Get/SetNoDataValue');
	    }
	    
	    if ($no_nodatavalue{$driver->{ShortName}} or
		($driver->{ShortName} eq 'GTiff' and ($type ne 'Byte' or $type ne 'UInt16')))
	    {
		mytest('skipped',undef,$name,$type,'Colortable');
		
	    } else 
	    {
		my $colortable = new gdal::ColorTable();
		my @rgba = (255,0,0,255);
		$colortable->SetColorEntry(0, \@rgba);
		$band->SetRasterColorTable($colortable);
		$colortable = $band->GetRasterColorTable;
		my @rgba2 = $colortable->GetColorEntry(0);
		
		mytest($rgba[0] == $rgba2[0] and
		       $rgba[1] == $rgba2[1] and
		       $rgba[2] == $rgba2[2] and
		       $rgba[3] == $rgba2[3],"colors do not match",$name,$type,'Colortable');
	    }
	    
	    my $pc = $pack_types{"GDT_$type"};
	    
	    if ($driver->{ShortName} eq 'VRT') 
	    {
		mytest('skipped',"",$name,$type,'WriteRaster');
		
	    } elsif (!$pc) 
	    {
		mytest('skipped',"no packtype defined yet",$name,$type,'WriteRaster');
		
	    } else 
	    {
		$pc = "${pc}[$width]";
		my $scanline = pack($pc,(1..$width));
		
		for my $yoff (0..$height-1) {
		    $band->WriteRaster( 0, $yoff, $width, 1, $scanline );
		}
	    }
	    
	    if ($no_setgcp{$driver->{ShortName}})
	    {
		mytest('skipped',undef,$name,$type,'Set/GetGCPs');
		
	    } else 
	    {
		my @gcps = ();
		push @gcps,new gdal::GCP(1.1,2.2);
		push @gcps,new gdal::GCP(2.1,3.2);
		my $po = "ho ho ho";
		$dataset->SetGCPs(\@gcps,$po);
		my $c = $dataset->GetGCPCount();
		my $p = $dataset->GetGCPProjection();
		my $gcps = $dataset->GetGCPs();
		my $y1 = $gcps->[0]->{GCPY};
		my $y2 = $gcps->[1]->{GCPY};
		my $y1o = $gcps[0]->{GCPY};
		my $y2o = $gcps[1]->{GCPY};
		mytest(($c == 2 and $p eq $po and $y1 == $y1o and $y2 == $y2o),
		       "$c != 2 or $p ne $po or $y1 != $y1o or $y2 != $y2o",$name,$type,'Set/GetGCPs');
	    }

	    undef $band;
	    undef $dataset;
	    
	    if ($no_open{$driver->{ShortName}} or 
		($driver->{ShortName} eq 'MFF2' and 
		 ($type eq 'Int32' or $type eq 'Float64' or $type eq 'CFloat64')))
	    {
		mytest('skipped',undef,$name,$type,'open');
		
	    } else 
	    {    
		$ext = '.'.$metadata->{DMD_EXTENSION};
		$filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
		
		eval {
		    $dataset = gdal::Open($filename);
		};
		mytest($dataset,'no message',$name,$type,'open');
		
		if ($dataset) {
		    mytest($dataset->{RasterXSize} == $width,'RasterXSize',$name,$type,'RasterXSize');
		    mytest($dataset->{RasterYSize} == $height,'RasterYSize',$name,$type,'RasterYSize');
		    
		    my $band = $dataset->GetRasterBand(1);
		    
		    if ($pc) {
			
			my $scanline = $band->ReadRaster( 0, 0, $width, 1);
			my @data = unpack($pc, $scanline);
			mytest($data[49] == 50,'',$name,$type,'ReadRaster');
			
		    }
		    
		}
		undef $dataset;
	    }    
	}
    }
}

sub ogr_tests {
    my($nr_drivers_tested,$osr) = @_;
    
    for my $i (0..$nr_drivers_tested-1) {
	
	my $driver = ogr::GetDriver($i);
	unless ($driver) {
	    mytest('',undef,"ogr::GetDriver($i)");
	    next;
	}
	my $name = $driver->{name};
	
	if (!$driver->TestCapability($ogr::ODrCCreateDataSource)) {
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
	for (@field_types) {$field_types{$_} = eval "\$ogr::$_"};
	
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
	    
	    my $layer;
	    eval {
		$layer = $datasource->CreateLayer($type, $osr, $types{$type});
	    };
	    mytest($layer,'no message',$name,$type,'layer create');
	    
	    next unless $layer;
	    
	    # create one field of each type
	    
	    for my $ft (@field_types) {
		
		my $column = new ogr::FieldDefn($ft, $field_types{$ft});
		
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
		
		my $feature = new ogr::Feature($schema);
		
		my $t = $type eq 'wkbUnknown' ? $ogr::wkbPolygon : $types{$type};
		
		my $geom = new ogr::Geometry($t);
		
		test_geom($geom,$name,$type,'create');
		
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
			$datasource = ogr::Open("$dir/$type.tab");
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
			    my $t = $type eq 'wkbUnknown' ? $ogr::wkbPolygon : $types{$type};
			    my $t2 = $geom->GetGeometryType;
			    mytest($t == $t2,"$t != $t2",$name,$type,'geom open');
			    test_geom($geom,$name,$type,'open');
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
	    my $r = new ogr::Geometry($ogr::wkbLinearRing);
	    pop @pts;
	    for my $pt (@pts) {
		$r->AddPoint(@$pt);
	    }
	    $geom->AddGeometry($r);
	    
#	    $r->DISOWN;
#	    $geom->AddGeometryDirectly($r);

	    $geom->CloseRings; # this overwrites the last point
	} else {
	    mytest($gn == 1,"$gn != 1",$name,$type,'geom count');
	    my $r = $geom->GetGeometryRef(0);
	    $pc = $r->GetPointCount;
	    mytest($pc == 6,"$pc != 6",$name,$type,'point count');
#	    @pts = reverse @pts if ($name eq 'MapInfo File');
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

