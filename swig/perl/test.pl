BEGIN { $| = 1; }
END {print "not ok 1\n" unless $loaded;}
use gdal;
use gdalconst;
use osr;
use ogr;
$loaded = 1;

#$datasource = ogr::Open(undef);

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

%types;

for (@types) {$types{$_} = eval "\$gdalconst::$_"};

sub dumphash {
    my $h = shift;
    for (keys %$h) {
	print "$_ $h->{$_}\n";
    }
}

%no_colortable = map {$_=>1} ('ELAS','BMP','ILWIS','BT','RMF','NITF');

%no_nodatavalue = map {$_=>1} ('NITF','HFA','ELAS','BMP','ILWIS','BT','IDA','RMF');

%no_geotransform = map {$_=>1} ('NITF','PAux','PNM','MFF','ENVI','BMP','EHdr');

%no_setgcp = map {$_=>1} ('HFA','ELAS','MEM','BMP','PCIDSK','ILWIS','PNM','ENVI',
			  'NITF','EHdr','MFF','MFF2','BT','IDA','RMF');

%no_open = map {$_=>1} ('VRT','MEM','ILWIS','MFF2');

@fails;

@tested_drivers = ('VRT','GTiff','NITF','HFA','SAR_CEOS','CEOS','ELAS',
		   'AIG','AAIGrid','SDTS','DTED','PNG','JPEG','MEM','JDEM',
		   'GIF','ESAT','BSB','XPM','BMP','AirSAR','RS2','PCIDSK',
		   'ILWIS','RIK','PNM','DOQ1','DOQ2','ENVI','EHdr','PAux',
		   'MFF','MFF2','FujiBAS','GSC','FAST','BT','LAN','CPG','IDA',
		   'NDF','L1B','FIT','RMF','USGSDEM','GXF');

#@tested_drivers = ('VRT');

for $i (@tested_drivers) {

    my $driver = gdal::GetDriverByName($i);

    $name = $driver->{ShortName};
    
    my $metadata = $driver->GetMetadata();
    
    unless ($metadata->{DCAP_CREATE} eq 'YES') {
	mytest('skipped: no capability',undef,$name,'dataset create');
	next;
    }
    
    @create = split /\s+/,$metadata->{DMD_CREATIONDATATYPES};
    
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

    for $type (@create) {

	if (($driver->{ShortName} eq 'MFF2') and ($type eq 'CInt32')) {
	    mytest('skipped: does not work?',undef,$name,$type,'dataset create');
	    next;
	}

	$typenr = $types{'GDT_'.$type};

	$filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
	$width = 100;
	$height = 50;
	$bands = 1;
	$options = undef;

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
	    $band->SetNoDataValue(5.5);
	    my $value = $band->GetNoDataValue;
	    mytest($value == 5.5,"$value != 5.5",$name,$type,'Get/SetNoDataValue');
	}
	
	if ($no_nodatavalue{$driver->{ShortName}} or
	    ($driver->{ShortName} eq 'GTiff' and ($type ne 'Byte' or $type ne 'UInt16')))
	{
	    mytest('skipped',undef,$name,$type,'Colortable');

	} else 
	{
	    my $colortable = new gdal::ColorTable();
	    @rgba = (255,0,0,255);
	    $colortable->SetColorEntry(0, \@rgba);
	    $band->SetRasterColorTable($colortable);
	    $colortable = $band->GetRasterColorTable;
	    @rgba2 = $colortable->GetColorEntry(0);

	    mytest($rgb[0] == $rgb2[0] and
		    $rgb[1] == $rgb2[1] and
		    $rgb[2] == $rgb2[2] and
		    $rgb[3] == $rgb2[3],"colors do not match",$name,$type,'Colortable');
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
	    $scanline = pack($pc,(1..$width));

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

		    $scanline = $band->ReadRaster( 0, 0, $width, 1);
		    my @data = unpack($pc, $scanline);
		    mytest($data[49] == 50,'',$name,$type,'ReadRaster');

		}
		
	    }
	    
	    undef $dataset;
	}

    }
}

@types = ('wkbUnknown','wkbPoint','wkbLineString','wkbPolygon',
	  'wkbMultiPoint','wkbMultiLineString','wkbMultiPolygon','wkbGeometryCollection');

#@types = ();

%types = ();

for (@types) {$types{$_} = eval "\$ogr::$_"};

for $i (0..ogr::GetDriverCount()-1) {
    my $driver = ogr::GetDriver($i);
    $name = $driver->{name};

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

    @field_types = ('OFTInteger','OFTIntegerList','OFTReal','OFTRealList','OFTString',
		    'OFTStringList','OFTWideString','OFTWideStringList','OFTBinary');
    
    if ($name eq 'ESRI Shapefile') {
	@field_types = ('OFTInteger','OFTReal','OFTString','OFTInteger');
    } elsif ($name eq 'MapInfo File') {
	@field_types = ('OFTInteger','OFTReal','OFTString');
    }
    
    %field_types;
    for (@field_types) {$field_types{$_} = eval "\$ogr::$_"};

    $dir0 = $name;
    $dir0 =~ s/ //g;
    $dir = "tmp_ds_$dir0";
    system "mkdir $dir" unless $name eq 'Memory';
    
    eval {
	$datasource = $driver->CreateDataSource($dir);
    };
    $msg = mytest($datasource,'no message',$name,'datasource create');
    
    next unless $datasource;
    
    for $type (@types) {
	
	if ($name eq 'ESRI Shapefile' and $type eq 'wkbGeometryCollection') {
	    mytest("skipped, will fail",undef,$name,$type,'layer create');
	    next;
	}

	if ($type eq 'wkbMultiPolygon') {
	    mytest("skipped, no test yet",undef,$name,$type,'layer create');
	    next;
	}

	eval {
	    $layer = $datasource->CreateLayer($type, undef, $types{$type});
	};
	$msg = mytest($layer,'no message',$name,$type,'layer create');
	
	next unless $layer;

	# create one field of each type

	for $ft (@field_types) {

	    my $column = new ogr::FieldDefn($ft, $field_types{$ft});

	    $layer->CreateField($column);

	}

	{
	    my $schema = $layer->GetLayerDefn();

	    $i = 0;
	    for $ft (@field_types) {

		my $column = $schema->GetFieldDefn($i++);
		my $n = $column->GetName;
		mytest($n eq $ft,"$n ne $ft",$name,$type,$ft,'field create');

	    }

	    my $feature = new ogr::Feature($schema);
	    
	    $t = $type eq 'wkbUnknown' ? $ogr::wkbPolygon : $types{$type};
	    
	    my $geom = new ogr::Geometry($t);

	    test_geom($geom,$name,$type,'create');
	    
	    $feature->SetGeometry($geom);

	    $i = 0;
	    for $ft (@field_types) {
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

	    $msg = mytest($layer,'no message',$name,$type,"layer $type open");
	    next unless $layer;

	    # check to see if the fields exist and the types are the same

	    my $schema = $layer->GetLayerDefn();

	    $i = 0;
	    for $ft (@field_types) {
		my $column = $schema->GetFieldDefn($i++);
		my $n = $column->GetName;
		mytest($n eq $ft,"$n ne $ft",$name,$type,$ft,'field open (name)');
		my $t = $column->GetType;
		my $t2 = $field_types{$ft};
		mytest($t == $t2,"$t != $t2",$name,$type,$ft,'field open (type)');
	    }

	    if ($type eq 'wkbPoint' or $type eq 'wkbLineString' or $type eq 'wkbPolygon') {

		$layer->ResetReading;
#		my $feature = $layer->GetFeature(0);
		my $feature = $layer->GetNextFeature;
		
		mytest($feature,'getFeature failed',$name,$type,'feature open');
		
		if ($feature) {
		    
		    my $geom = $feature->GetGeometryRef();
		    
		    if ($type eq 'wkbPointlll') {
			mytest('skipped',undef,$name,$type,'geom open');
		    } else {
			$t = $type eq 'wkbUnknown' ? $ogr::wkbPolygon : $types{$type};
			$t2 = $geom->GetGeometryType;
			mytest($t == $t2,"$t != $t2",$name,$type,'geom open');
			test_geom($geom,$name,$type,'open');
		    }

		    $i = 0;
		    for $ft (@field_types) {
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
    
    undef $datasource;

#    $driver->DeleteDataSource($dir); #  <- Delete the data source 

}

if (@fails) {
    print "unexpected failures:\n",@fails;
    print "all other tests ok.\n";
} else {
    print "all tests ok.\n";
}

system "rm -rf tmp_ds_*";

sub test_geom {
    my($geom,$name,$type,$mode) = @_;
#    my($pc,$gn);

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

	my @pts = ([1.1,1],[1.11,0],[0,0.2],[0,2.1],[1,1]);

	if ($mode eq 'create') {
	    my $r = new ogr::Geometry($ogr::wkbLinearRing);
	    pop @pts;
	    for $pt (@pts) {
		$r->AddPoint(@$pt);
	    }
	    $geom->AddGeometry($r);
	    $geom->CloseRings;
	} else {
	    mytest($gn == 1,"$gn != 1",$name,$type,'geom count');
	    my $r = $geom->GetGeometryRef(0);
	    $pc = $r->GetPointCount;
	    mytest($pc == 5,"$pc != 5",$name,$type,'point count');
#	    @pts = reverse @pts if ($name eq 'MapInfo File');
	    for my $cxy (@pts) {
		my @xy = ($r->GetX($i),$r->GetY($i)); $i++;
		mytest(cmp_ar(2,\@xy,$cxy),"(@xy) != (@$cxy)",$name,$type,"get point");
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
