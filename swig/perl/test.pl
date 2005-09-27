BEGIN { $| = 1; }
END {print "not ok 1\n" unless $loaded;}
use gdal;
use gdalconst;
use osr;
use ogr;
$loaded = 1;

$verbose = 0;

# tests:
#
# for explicitly given raster types: 
#   Create dataset
#   Get/SetGeoTransform
#   Get/SetNoDataValue
#   Colortable operations
#   WriteRaster
#   Open dataset
#   ReadRaster
#
# for all OGR drivers:
#   Create datasource
#   Create layer
#   Create field
#   Create geometry
#   Open layer
#   Open field
#   Open geom
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

@fails;

@tested_drivers = ('VRT','GTiff','NITF','HFA','SAR_CEOS','CEOS','ELAS',
		   'AIG','AAIGrid','SDTS','DTED','PNG','JPEG','MEM','JDEM',
		   'GIF','ESAT','BSB','XPM','BMP','AirSAR','RS2','PCIDSK',
		   'ILWIS','RIK','PNM','DOQ1','DOQ2','ENVI','EHdr','PAux',
		   'MFF','MFF2','FujiBAS','GSC','FAST','BT','LAN','CPG','IDA',
		   'NDF','L1B','FIT','RMF','USGSDEM','GXF');

#@tested_drivers = ('GTiff');

$n = gdal::GetDriverCount();

for $i (@tested_drivers) {
    my $driver = gdal::GetDriverByName($i);

#for $i (0..$n-1) {
#    $driver = gdal::GetDriver($i);
#    print "$driver->{ShortName}\n";
#    next;

    $name = $driver->{ShortName};
    
    my $metadata = $driver->GetMetadata();
    
    unless ($metadata->{DCAP_CREATE} eq 'YES') {
	mytest('skipped: no capability',undef,$name,'dataset create');
	next;
    }
    
    @create = split /\s+/,$metadata->{DMD_CREATIONDATATYPES};
    
    @create = ('Byte','Float32','UInt16',
	       'Int16','CInt16','CInt32','CFloat32') if $driver->{ShortName} eq 'MFF2';
    
    $ext = '.'.$metadata->{DMD_EXTENSION};

    unless (@create) {
	mytest('skipped: no creation datatypes',undef,$name,'dataset create');
	next;
    }

    if ($driver->{ShortName} eq 'PAux') {
	mytest('skipped: PAux',undef,$name,'dataset create');
	next;
    }

    for $type (@create) {

	if (($type eq 'CInt32') and ('MFF2' eq $driver->{ShortName})) {
	    mytest('skipped: will cause a fatal error',undef,$name,$type,'dataset create');
	    next;
	}

	$typenr = $types{'GDT_'.$type};

	$ext = '' if $driver->{ShortName} eq 'ILWIS';

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
	
	if ($driver->{ShortName} ne 'NITF' and
	    $driver->{ShortName} ne 'PAux' and
	    $driver->{ShortName} ne 'PNM' and
	    $driver->{ShortName} ne 'MFF' and
	    $driver->{ShortName} ne 'ENVI' and
	    $driver->{ShortName} ne 'BMP' and
	    $driver->{ShortName} ne 'EHdr') 
	{
	    my $transform = $dataset->GetGeoTransform();
	    $transform->[5] = 12;
	    $dataset->SetGeoTransform($transform);
	    my $transform2 = $dataset->GetGeoTransform();
	    mytest($transform->[5] == $transform2->[5],
		   "$transform->[5] != $transform2->[5]",$name,$type,'Get/SetGeoTransform');
	} else {
	    mytest('skipped',undef,$name,$type,'Get/SetGeoTransform');
	}
	
	if ($driver->{ShortName} ne 'NITF' and 
	    $driver->{ShortName} ne 'HFA' and
	    $driver->{ShortName} ne 'ELAS' and
	    $driver->{ShortName} ne 'BMP' and
	    $driver->{ShortName} ne 'ILWIS' and
	    $driver->{ShortName} ne 'BT' and
	    $driver->{ShortName} ne 'IDA' and
	    $driver->{ShortName} ne 'RMF') 
	{
	    $band->SetNoDataValue(5.5);
	    my $value = $band->GetNoDataValue;
	    mytest($value == 5.5,"$value != 5.5",$name,$type,'Get/SetNoDataValue');
	} else {
	    mytest('skipped',undef,$name,$type,'Get/SetNoDataValue');
	}
	
	if (($driver->{ShortName} eq 'GTiff' and ($type ne 'Byte' or $type ne 'UInt16')) or
	    ($driver->{ShortName} eq 'ELAS') or
	    ($driver->{ShortName} eq 'BMP') or
	    ($driver->{ShortName} eq 'ILWIS') or
	    ($driver->{ShortName} eq 'BT') or
	    ($driver->{ShortName} eq 'RMF') or
	    ($driver->{ShortName} eq 'NITF'))
	{
	    mytest('skipped',undef,$name,$type,'Colortable');
	} else {
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
	if ($pc and $driver->{ShortName} ne 'VRT') {

	    $pc = "${pc}[$width]";
	    $scanline = pack($pc,(1..$width));

	    for my $yoff (0..$height-1) {
		$band->WriteRaster( 0, $yoff, $width, 1, $scanline );
	    }

	} else {
	    mytest('skipped',undef,$name,$type,'WriteRaster');
	}

	undef $band;
	undef $dataset;
	
	if ($driver->{ShortName} eq 'VRT' or 
	    $driver->{ShortName} eq 'MEM' or 
	    $driver->{ShortName} eq 'ILWIS' or
	    ($driver->{ShortName} eq 'MFF2' and 
	     ($type eq 'Int32' or $type eq 'Float64' or $type eq 'CFloat64')))
	{
	    mytest('skipped',undef,$name,$type,'open');
	} else {
	    
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
	    my $r;

	    if ($type eq 'wkbPoint' or $type eq 'wkbLineString') {
		$geom->AddPoint(1,1);
		$geom->AddPoint(2,2);
	    } elsif ($type eq 'wkbUnknown' or $type eq 'wkbPolygon') {
		$r = new ogr::Geometry($ogr::wkbLinearRing);
		$r->AddPoint(0,0);
		$r->AddPoint(0,1);
		$r->AddPoint(1,1);
		$r->AddPoint(1,0);
		$geom->AddGeometry($r);
		$geom->CloseRings;
	    } else {
#		print "what to add to $type\n";
		mytest('skipped',undef,$name,$type,'geom create');
	    }
	    
	    $feature->SetGeometry($geom);

	    $i = 0;
	    for $ft (@field_types) {
		$feature->SetField($i++,2);
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
