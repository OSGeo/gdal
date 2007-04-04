use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

use vars qw/%available_driver %test_driver $loaded $verbose @types %types @fails @tested_drivers/;

$loaded = 1;

$verbose = $ENV{VERBOSE};

# tests:
#
# for pre-tested GDAL drivers: 
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
# if verbose = 1, all operations (skip,fail,ok) are printed out

system "rm -rf tmp_ds_*" unless $^O eq 'MSWin32';

%test_driver = ('VRT' => 0,
		'GTiff' => 1,
		'NITF' => 0,
		'HFA' => 0,
		'SAR_CEOS' => 0,
		'CEOS' => 0,
		'ELAS' => 0,
		'AIG' => 0,
		'AAIGrid' => 0,
		'SDTS' => 0,
		'OGDI' => 0,
		'DTED' => 0,
		'PNG' => 0,
		'JPEG' => 0,
		'MEM' => 1,
		'JDEM' => 0,
		'GIF' => 0,
		'ESAT' => 0,
		'BSB' => 0,
		'XPM' => 0,
		'BMP' => 0,
		'AirSAR' => 0,
		'RS2' => 0,
		'PCIDSK' => 0,
		'PCRaster' => 0,
		'ILWIS' => 0,
		'RIK' => 0,
		'SGI' => 0,
		'Leveller' => 0,
		'GMT' => 0,
		'netCDF' => 0,
		'PNM' => 0,
		'DOQ1' => 0,
		'DOQ2' => 0,
		'ENVI' => 0,
		'EHdr' => 1,
		'PAux' => 0,
		'MFF' => 0,
		'MFF2' => 0,
		'FujiBAS' => 0,
		'GSC' => 0,
		'FAST' => 0,
		'BT' => 0,
		'LAN' => 0,
		'CPG' => 0,
		'IDA' => 0,
		'NDF' => 0,
		'DIPEx' => 0,
		'ISIS2' => 0,
		'L1B' => 0,
		'FIT' => 0,
		'RMF' => 0,
		'RST' => 0,
		'USGSDEM' => 0,
		'GXF' => 0);

@types = ('GDT_Byte','GDT_UInt16','GDT_Int16','GDT_UInt32','GDT_Int32',
	  'GDT_Float32','GDT_Float64','GDT_CInt16','GDT_CInt32','GDT_CFloat32','GDT_CFloat64');

for (@types) {$types{$_} = eval "\$Geo::GDAL::Const::$_"};

my %no_colortable = map {$_=>1} ('NITF','ELAS','BMP','ILWIS','BT','RMF','RST');

my %no_nodatavalue = map {$_=>1} ('NITF','HFA','ELAS','BMP','ILWIS','BT','IDA','RMF');

my %no_geotransform = map {$_=>1} ('NITF','PAux','PNM','MFF','ENVI','BMP','EHdr');

my %no_setgcp = map {$_=>1} ('HFA','ELAS','MEM','BMP','PCIDSK','ILWIS','PNM','ENVI',
			     'NITF','EHdr','MFF','MFF2','BT','IDA','RMF','RST');

my %no_open = map {$_=>1} ('VRT','MEM','ILWIS','MFF2');

gdal_tests(Geo::GDAL::GetDriverCount());

my @tmp = sort keys %available_driver;

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

sub gdal_tests {
    my $nr_drivers_tested = shift;

    for my $i (0..$nr_drivers_tested-1) {

	my $driver = Geo::GDAL::GetDriver($i);
	unless ($driver) {
	    mytest('',undef,"Geo::GDAL::GetDriver($i)");
	    next;
	}

	my $name = $driver->{ShortName};
	$available_driver{$name} = 1;
	mytest('skipped: not tested',undef,$name,'test'),next unless $test_driver{$name};

        next if $name eq 'MFF2'; # does not work probably because of changes in hkvdataset.cpp
	
	my $metadata = $driver->GetMetadata();
	
	unless ($metadata->{DCAP_CREATE} and $metadata->{DCAP_CREATE} eq 'YES') {
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

	push @tested_drivers,$name;
	
	my $ext = $metadata->{DMD_EXTENSION} ? '.'.$metadata->{DMD_EXTENSION} : '';
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
	    
	    if ($no_colortable{$driver->{ShortName}} 
		or ($driver->{ShortName} eq 'GTiff' and ($type ne 'Byte' or $type ne 'UInt16'))
		)
	    {
		mytest('skipped',undef,$name,$type,'Colortable');
		
	    } else 
	    {
		my $colortable = new Geo::GDAL::ColorTable();
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
	    
	    my $pc;
	    eval {
		$pc = Geo::GDAL::PackCharacter($band->{DataType});
	    };
	    
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
		push @gcps,new Geo::GDAL::GCP(1.1,2.2);
		push @gcps,new Geo::GDAL::GCP(2.1,3.2);
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
		$ext = $metadata->{DMD_EXTENSION} ? '.'.$metadata->{DMD_EXTENSION} : '';
		$filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
		
		eval {
		    $dataset = Geo::GDAL::Open($filename);
		};
		mytest($dataset,'no message',$name,$type,"open $filename");
		
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
