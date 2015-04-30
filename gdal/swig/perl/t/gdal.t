use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

use vars qw/%available_driver %test_driver $loaded $verbose @types @fails @tested_drivers/;

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

%test_driver = ('GTiff' => 1,
		'MEM' => 1,
		'EHdr' => 1,
		);

@types = (qw/Byte UInt16 Int16 UInt32 Int32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64/);

my %no_colortable = map {$_=>1} ('NITF','ELAS','BMP','ILWIS','BT','RMF','RST');

my %no_nodatavalue = map {$_=>1} ('NITF','HFA','ELAS','BMP','ILWIS','BT','IDA','RMF');

my %no_geotransform = map {$_=>1} ('NITF','PAux','PNM','MFF','ENVI','BMP','EHdr');

my %no_setgcp = map {$_=>1} ('HFA','ELAS','MEM','BMP','PCIDSK','ILWIS','PNM','ENVI',
			     'NITF','EHdr','MFF','MFF2','BT','IDA','RMF','RST');

my %no_open = map {$_=>1} ('VRT','MEM','ILWIS','MFF2');

if (0) {
    {
	my $ds = Geo::GDAL::Dataset::Open('/data/Corine/lceugr100_00/lceugr100_00_pct.tif');
	$b = $ds->GetRasterBand(1);
    }
    $b->GetDefaultRAT();
    exit;
}

{
    # test memory files
    my $fp = Geo::GDAL::VSIFOpenL('/vsimem/x', 'w');
    my $c = Geo::GDAL::VSIFWriteL("hello world!\n", $fp);
    ok($c == 13, 'Wrote 13 characters to a memory file.');
    Geo::GDAL::VSIFCloseL($fp);
    $fp = Geo::GDAL::VSIFOpenL('/vsimem/x', 'r');
    my $b = Geo::GDAL::VSIFReadL(40, $fp);
    ok($b eq "hello world!\n", 'Read back what I Wrote to a memory file.');
    Geo::GDAL::VSIFCloseL($fp);
    Geo::GDAL::Unlink('/vsimem/x');
}

{
    my $driver = Geo::GDAL::GetDriver('MEM');
    my $dataset = $driver->Create('tmp', 10, 10, 3 , 'Int32', []);
    ok($dataset->isa('Geo::GDAL::Dataset'), 'Geo::GDAL::Dataset');
    ok($dataset->{RasterXSize} == 10, "Geo::GDAL::Dataset::RasterXSize $dataset->{RasterXSize}");
    ok($dataset->{RasterCount} == 3, "Geo::GDAL::Dataset::RasterCount $dataset->{RasterCount}");
    my $drv = $dataset->GetDriver;
    ok($drv->isa('Geo::GDAL::Driver'), 'Geo::GDAL::Dataset::GetDriver');
    my @size = $dataset->Size();
    ok(is_deeply([10,10], \@size), "Geo::GDAL::Dataset::Size @size");
    my $r = $dataset->GetRasterBand(1);
    my $g = $dataset->GetRasterBand(2);
    my $b = $dataset->GetRasterBand(3);

    $b->WriteTile([
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10]
    ]);
    $r->WriteTile([
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,0,0,0,0,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10],
    [1,2,3,4,5,6,7,8,9,10]
    ]);
    $g->WriteTile($b->ReadTile);
    $b->FillNodata($r);
    #print STDERR "@$_\n" for (@{$b->ReadTile()});

    my $histogram;
    eval {
	@histogram = $b->GetHistogram();
    };
    ok($#histogram == 255, "Histogram: @histogram");
    eval {
	$b->SetDefaultHistogram(1,10,[0..255]);
    };
    my ($min, $max);
    eval {
	($min,$max,$histogram) = $b->GetDefaultHistogram();
    };
    ok(($#$histogram == 255), "Default Histogram $#histogram == 255");
    eval {
	@histogram = $b->GetHistogram(Min=>0, Max=>100, Buckets=>20);
    };
    ok($#histogram == 19, "Histogram with parameters: @histogram");

    my @o;
    for (0..5) {
	my $a = 0.1*$_;
	push @o, "$a Generating Histogram 1";
    }
    my @out;
    $callback = sub {
    	      push @out, "@_";	
    	      return $_[0] < 0.5 ? 1 : 0 };	
    eval {
	Geo::GDAL::ComputeMedianCutPCT($r,$g,$b,5,
				       Geo::GDAL::ColorTable->new,
				       $callback
				       );
    };
    ok(is_deeply(\@out, \@o), "callback without callback_data");
    @o = ();
    for (0..5) {
	my $a = 0.1*$_;
	push @o, "$a Generating Histogram 6";
    }
    @out = ();
    eval {
	Geo::GDAL::ComputeMedianCutPCT($r,$g,$b,5,
				       Geo::GDAL::ColorTable->new,
				       $callback,6);
    };
    ok(is_deeply(\@out, \@o), "callback with callback_data");

    # without callback only implicit test:
    Geo::GDAL::ComputeMedianCutPCT($r,$g,$b,5,Geo::GDAL::ColorTable->new);

    Geo::GDAL::RegenerateOverview($r, $b, 'GAUSS');
    
    my $band = $r;

    $colors = $band->ColorTable(Geo::GDAL::ColorTable->new);
    @table = $colors->ColorTable([10,20,30,40],[20,20,30,40]);
    for (@table) {
	@$_ = (1,2,3,4) if $_->[0] == 10;
    }
    @table2 = $colors->ColorTable(@table);
    ok($table[1]->[1] == 20, "colortable 1");
    ok($table2[0]->[2] == 3, "colortable 2");

    my @data;
    for my $yoff (0..9) {
	push @data, [$yoff..9+$yoff];
    }
    $band->WriteTile(\@data);
    for my $yoff (4..6) {
	for my $xoff (3..4) {
	    $data[$yoff][$xoff] = 0;
	}
    }
    my $data = $band->ReadTile(3,4,2,3);
    for my $y (@$data) {
	for (@$y) {
	    $_ = 0;
	}
    }
    $band->WriteTile($data,3,4);
    $data = $band->ReadTile();
    ok(is_deeply(\@data,$data), "write/read tile");
}

{
    BEGIN { $SIG{'__WARN__'} = sub { warn $_[0] if $DOWARN } }
    my $r = Geo::GDAL::RasterAttributeTable->new;
    my @t = $r->FieldTypes;
    my @u = $r->FieldUsages;
    my %colors = (Red=>1, Green=>1, Blue=>1, Alpha=>1);
    my @types;
    my @usages;
    for my $u (@u) {
	for my $t (@t) {
	    $r->CreateColumn("$t $u", $t, $u);
            push @types, $t;
            push @usages, $u;
	}
    }
    my $n = $r->GetColumnCount;
    my $n2 = @t * @u;
    ok($n == $n2, "create rat column");
    $r->SetRowCount(1);
    my $i = 0;
    for my $c (0..$n-1) {
        my $usage = $r->GetUsageOfCol($c);
        ok($usage eq $usages[$c], "usage $usage eq $usages[$c]");
        my $type = $r->GetTypeOfCol($c);
        if ($colors{$usage}) {
            ok($type eq 'Integer', "type $type eq 'Integer'");
        } else {
            ok($type eq $types[$c], "type $type eq $types[$c]");
        }
        for ($type) {
            if (/Integer/) {
                my $v = $r->Value($i, $c, 12);
                ok($v == 12, "rat int ($i,$c): $v vs 12");
            } elsif (/Real/) {
                my $v = $r->Value($i, $c, 1.23);
                ok($v == 1.23, "rat real ($i,$c): $v vs 1.23");
            } elsif (/String/) {
                my $v = $r->Value($i, $c, "abc");
                ok($v eq 'abc', "rat str ($i,$c): $v vs 'abc'");
            }
        }
    }
}

gdal_tests();

$src = Geo::OSR::SpatialReference->new(EPSG => 2392);

$xml = $src->ExportToXML();
$a = Geo::GDAL::ParseXMLString($xml);
$xml = Geo::GDAL::SerializeXMLTree($a);
$b = Geo::GDAL::ParseXMLString($xml);
ok(is_deeply($a, $b), "xml parsing");

my @tmp = sort keys %available_driver;

#print STDERR "\nGDAL version: ",Geo::GDAL::VersionInfo,"\n";
#print STDERR "Unexpected failures:\n",@fails,"\n" if @fails;
#print STDERR "Available drivers were ",join(', ',@tmp),"\n";
#print STDERR "Drivers used in tests were: ",join(', ',@tested_drivers),"\n";

system "rm -rf tmp_ds_*" unless $^O eq 'MSWin32';

###########################################
#
# only subs below
#
###########################################

sub gdal_tests {

    for my $driver (Geo::GDAL::Drivers) {

	my $name = $driver->{ShortName};
	
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

        next if $name eq 'MFF2'; # does not work probably because of changes in hkvdataset.cpp
	
	my $metadata = $driver->GetMetadata();
	
	unless ($metadata->{DCAP_CREATE} and $metadata->{DCAP_CREATE} eq 'YES') {
	    mytest('skipped: no capability',undef,$name,'dataset create');
	    next;
	}
	
	my @create = split /\s+/,$metadata->{DMD_CREATIONDATATYPES};
	
	@create = (qw/Byte Float32 UInt16 Int16 CInt16 CInt32 CFloat32/)
	    if $driver->{ShortName} eq 'MFF2';
	
	unless (@create) {
	    mytest('skipped: no creation datatypes',undef,$name,'dataset create');
	    next;
	}
	
	if ($driver->{ShortName} eq 'PAux') {
	    mytest('skipped: does not work?',undef,$name,'dataset create');
	    next;
	}

        next unless $driver->{ShortName} eq 'MEM';

	push @tested_drivers,$name;
	
	my $ext = $metadata->{DMD_EXTENSION} ? '.'.$metadata->{DMD_EXTENSION} : '';
	$ext = '' if $driver->{ShortName} eq 'ILWIS';
	
	for my $type (@create) {
	    
	    if (($driver->{ShortName} eq 'MFF2') and ($type eq 'CInt32')) {
		mytest('skipped: does not work?',undef,$name,$type,'dataset create');
		next;
	    }

	    my $filename = "tmp_ds_".$driver->{ShortName}."_$type$ext";
	    my $width = 100;
	    my $height = 50;
	    my $bands = 1;
	    my $options = undef;

	    my $dataset;

	    eval {
		$dataset = $driver->Create($filename, $width, $height, $bands , $type, []);
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
		if ($name ne 'GTiff') {
		    $band->ColorInterpretation('GreenBand');
		    my $value = $band->ColorInterpretation;
		    mytest($value eq 'GreenBand',"$value ne GreenBand",$name,$type,'ColorInterpretation');
		}

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
		my $colortable = Geo::GDAL::ColorTable->new('Gray');
		my @rgba = (255,0,0,255);
		$colortable->SetColorEntry(0, \@rgba);
		$band->ColorTable($colortable);
		$colortable = $band->ColorTable;
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
		my $y1 = $gcps->[0]->{Y};
		my $y2 = $gcps->[1]->{Y};
		my $y1o = $gcps[0]->{Y};
		my $y2o = $gcps[1]->{Y};
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

		    {
			my @a = ('abc','def');
			my @b = $band->CategoryNames(@a);
			ok(is_deeply(\@a, \@b,"$name,$type,CategoryNames"));
		    }
		    
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
