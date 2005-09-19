BEGIN { $| = 1; }
END {print "not ok 1\n" unless $loaded;}
use gdalconst;
use gdal;
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
#   Open dataset
#
# for all OGR drivers:
#   Create datasource
#   Create layer
#   Schema test
#   Get/SetGeometry test
#
# if verbose = 1, all operations (skip,fail,ok) are printed out

gdal::UseExceptions();

system "rm -rf tmp_ds_*";

@types = ('GDT_Byte','GDT_UInt16','GDT_Int16','GDT_UInt32','GDT_Int32',
	  'GDT_Float32','GDT_Float64','GDT_CInt16','GDT_CInt32','GDT_CFloat32','GDT_CFloat64');

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

$n = gdal::GetDriverCount();

for $i (@tested_drivers) {
    my $driver = gdal::GetDriverByName($i);

#for $i (0..$n-1) {
#    $driver = gdal::GetDriver($i);
#    print "$driver->{ShortName}\n";
#    next;

    print "---->",$driver->{LongName}," ($driver->{ShortName})\n" if $verbose;
    
    $metadata = $driver->GetMetadata();

    unless ($metadata->{DCAP_CREATE} eq 'YES') {
	print "Create dataset skipped because no create capability announced.\n" if $verbose;
	next;
    }

    @create = split /\s+/,$metadata->{DMD_CREATIONDATATYPES};
    
    @create = ('Byte','Float32','UInt16',
	       'Int16','CInt16','CInt32','CFloat32') if $driver->{ShortName} eq 'MFF2';

    $ext = '.'.$metadata->{DMD_EXTENSION};

    unless (@create) {
	print "Create dataset skipped because no creation datatypes announced.\n" if $verbose;
	next;
    }

    if ($driver->{ShortName} eq 'PAux') {
	print "Create dataset skipped because of ?.\n" if $verbose;
	next;
    }

    for $type (@create) {

	if (($type eq 'CInt32') and ('MFF2' eq $driver->{ShortName})) {
	    print "Create dataset skipped because causes a fatal error." if $verbose;
	    next;
	}

	$typenr = $types{'GDT_'.$type};
	print "$type $typenr\n" if $verbose;

	$ext = '' if $driver->{ShortName} eq 'ILWIS';

	$filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
	$width = 100;
	$height = 50;
	$bands = 1;
	$options = undef;

	eval {
	    $dataset = $driver->Create($filename, $width, $height, $bands , $typenr, []);
	};
	$err = "without an error message.";
	if ($@) {
	    $@ =~ s/\n/ /g;
	    $@ =~ s/\s+$//;
	    $@ =~ s/\s+/ /g;
	    $@ =~ s/^\s+$//;
	    $err = $@ ? "with error:\n'$@'" : "without an error message.";
	}

	unless ($dataset) {
	    push @fails,"Creation of $type $driver->{LongName} raster dataset failed, $err\n";
	    print "Create dataset failed, error: $@\n" if $verbose;
	    next;
	} else {
	    print "Create dataset ok.\n" if $verbose;
	}

	my $band = $dataset->GetRasterBand(1);
	
	if ($driver->{ShortName} ne 'NITF' and
	    $driver->{ShortName} ne 'PAux' and
	    $driver->{ShortName} ne 'PNM' and
	    $driver->{ShortName} ne 'MFF' and
	    $driver->{ShortName} ne 'ENVI' and
	    $driver->{ShortName} ne 'BMP' and
	    $driver->{ShortName} ne 'EHdr') 
	{
	    $transform = $dataset->GetGeoTransform();
	    $transform->[5] = 12;
	    $dataset->SetGeoTransform($transform);
	    $transform2 = $dataset->GetGeoTransform();
	    if ($transform->[5] != $transform2->[5]) {
		$msg = "Get/SetGeoTransform for $type $driver->{LongName} failed.\n";
		push @fails,$msg;
	    } else {
		$msg = "Get/SetGeoTransform ok\n";
	    }
	} else {
	    $msg = "Get/SetGeoTransform skipped\n";
	}
	print $msg if $verbose;
	
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
	    $value = $band->GetNoDataValue;
	    if ($value != 5.5) {
		$msg = "Get/SetNoDataValue for $type $driver->{LongName} failed.\n";
		push @fails,$msg;
	    } else {
		$msg = "Get/SetNoDataValue ok\n";
	    }
	} else {
	    $msg = "Get/SetNoDataValue skipped.\n";
	}
	print $msg if $verbose;
	
	if (($driver->{ShortName} eq 'GTiff' and ($type ne 'Byte' or $type ne 'UInt16')) or
	    ($driver->{ShortName} eq 'ELAS') or
	    ($driver->{ShortName} eq 'BMP') or
	    ($driver->{ShortName} eq 'ILWIS') or
	    ($driver->{ShortName} eq 'BT') or
	    ($driver->{ShortName} eq 'RMF') or
	    ($driver->{ShortName} eq 'NITF'))
	{
	    $msg = "Colortable operations skipped.\n";
	} else {
	    my $colortable = new gdal::ColorTable();
	    @rgba = (255,0,0,255);
	    $colortable->SetColorEntry(0, \@rgba);
	    $band->SetRasterColorTable($colortable);
	    $colortable = $band->GetRasterColorTable;
	    @rgba2 = $colortable->GetColorEntry(0);
	    
	    if ($rgb[0] != $rgb2[0] or 
		$rgb[1] != $rgb2[1] or 
		$rgb[2] != $rgb2[2] or 
		$rgb[3] != $rgb2[3]) {
		$msg = "Colortable operation for $type $driver->{LongName} failed.\n";
		push @fails,$msg;
	    } else {
		$msg = "Colortable operations ok.\n";
	    }		
	}
	print $msg if $verbose;
	
	if ($driver->{ShortName} eq 'VRT' or 
	    $driver->{ShortName} eq 'MEM' or 
	    $driver->{ShortName} eq 'ILWIS' or
	    ($driver->{ShortName} eq 'MFF2' and 
	     ($type eq 'Int32' or $type eq 'Float64' or $type eq 'CFloat64')))
	{
	    $msg = "Open dataset skipped.\n";
	    
	} else {
	    
	    $ext = '.'.$metadata->{DMD_EXTENSION};
	    $filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
	    
	    eval {
		$dataset = gdal::Open($filename);
	    };
	    if ($@) {
		$@ =~ s/\n/ /g;
		$@ =~ s/\s+$//;
		$@ =~ s/\s+/ /g;
	    }
	    
	    $msg = $dataset ? "open dataset ok\n" : "open dataset failed, error: $@\n";
	    
	    undef $dataset;
	}
	print $msg if $verbose;
	
	undef $dataset;

    }
}

@types = ('wkbUnknown','wkbPoint','wkbLineString','wkbPolygon',
	  'wkbMultiPoint','wkbMultiLineString','wkbMultiPolygon','wkbGeometryCollection');

%types = ();

for (@types) {$types{$_} = eval "\$ogr::$_"};

for $i (0..ogr::GetDriverCount()-1) {
    my $driver = ogr::GetDriver($i);
    $name = $driver->{name};
    print "---->","$name\n" if $verbose;

    if (!$driver->TestCapability($ogr::ODrCCreateDataSource)) {
	print "Create datasource skipped because no capability.\n" if $verbose;
	next;
    }

    if ($driver->{name} eq 'S57' or $driver->{name} eq 'CSV' or $driver->{name} eq 'GML') {
	print "Create datasource skipped because really no capability?\n" if $verbose;
	next;
    }

    if ($driver->{name} eq 'TIGER' or $driver->{name} eq 'DGN') {
	print "Create datasource skipped because layer creations will fail.\n" if $verbose;
	next;
    }

    $dir = "tmp_ds_$i";
    system "mkdir $dir";

    eval {
	$datasource = $driver->CreateDataSource($dir);
    };
    $err = "without an error message.";
    if ($@) {
	$@ =~ s/\n/ /g;
	$@ =~ s/\s+$//;
	$@ =~ s/\s+/ /g;
	$@ =~ s/^\s+$//;
	$err = $@ ? "with error:\n'$@'" : "without an error message.";
    }

    unless ($datasource) {
	push @fails,"Creation of datasource for $driver->{name} failed $err\n";
	print "Create datasource failed, $err\n" if $verbose;
	next;
    } else {
	print "Create datasource ok.\n" if $verbose;
    }
    
    for $type (keys %types) {
	print "$type\n" if $verbose;

	if ($driver->{name} eq 'ESRI Shapefile' and $type eq 'wkbGeometryCollection') {
	    print "Create layer skipped because it will fail.\n" if $verbose;
	    next;
	}

	eval {
	    $layer = $datasource->CreateLayer($type, undef, $types{$type});
	};
	$err = "without an error message.";
	if ($@) {
	    $@ =~ s/\n/ /g;
	    $@ =~ s/\s+$//;
	    $@ =~ s/\s+/ /g;
	    $@ =~ s/^\s+$//;
	    $err = $@ ? "with error:\n'$@'" : "without an error message.";   
	}
	unless ($layer) {
	    push @fails,"Creation of $type layer for $driver->{name} failed $err\n";
	    print "Create layer failed, $err\n" if $verbose;
	    next;
	} else {
	    print "Create layer ok.\n" if $verbose;
	}

	my $column = new ogr::FieldDefn('testing', $ogr::OFTInteger);

	$layer->CreateField($column);

	my $schema = $layer->GetLayerDefn();

	my $column2 = $schema->GetFieldDefn(0);

	if ($column2->GetName ne 'testing') {
	    $msg = "Schema test $type $driver->{name} failed.\n";
	    push @fails,$msg;
	} else {
	    $msg = "Schema test ok.\n";
	}
	print $msg if $verbose;

	$t = $type eq 'wkbUnknown' ? $ogr::wkbPolygon : $types{$type};

	my $geom = new ogr::Geometry($t);

#	$geom->AddPoint(1,1);

	my $feature = new ogr::Feature($schema);
	$feature->SetGeometry($geom);

	my $geom2 = $feature->GetGeometryRef();

	if ($type eq 'wkbPoint') {
	    $msg = "Get/SetGeometry test skipped for $type.\n";
	} else {
	    $t2 = $geom2->GetGeometryType;
	    if ($t2 != $t) {
		$msg = "Get/SetGeometry test $type $driver->{name} failed ($t vs. $t2).\n";
		push @fails,$msg;
	    } else {
		$msg = "Get/SetGeometry test ok.\n";
	    }
	}
	print $msg if $verbose;

    }

    undef $layer;
    undef $datasource;
}

if (@fails) {
    print "unexpected failures:\n",@fails;
} else {
    print "all tests ok.\n";
}

system "rm -rf tmp_ds_*";


