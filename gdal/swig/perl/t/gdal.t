use strict;
use warnings;
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
    my $dataset = $driver->Create('tmp', 10, 10, 3 , 'Int32', {});
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

    my @histogram;
    eval {
	@histogram = $b->GetHistogram();
    };
    ok($#histogram == 255, "Histogram");
    eval {
	$b->SetDefaultHistogram(1,10,[0..255]);
    };
    my ($min, $max, $histogram);
    eval {
	($min,$max,$histogram) = $b->GetDefaultHistogram();
    };
    ok(($#$histogram == 255), "Default Histogram $#histogram == 255");
    eval {
	@histogram = $b->GetHistogram(Min=>0, Max=>100, Buckets=>20);
    };
    ok($#histogram == 19, "Histogram with parameters");
 
    Geo::GDAL::RegenerateOverview($r, $b, 'GAUSS');
    
    my $band = $r;

    my $colors = $band->ColorTable(Geo::GDAL::ColorTable->new);
    my @table = $colors->ColorTable([10,20,30,40],[20,20,30,40]);
    for (@table) {
	@$_ = (1,2,3,4) if $_->[0] == 10;
    }
    my @table2 = $colors->ColorTable(@table);
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
    my $DOWARN = 0;
    BEGIN { $SIG{'__WARN__'} = sub { warn $_[0] if $DOWARN } }
    my $r = Geo::GDAL::RasterAttributeTable->new;
    my @t = $r->FieldTypes;
    my @u = $r->FieldUsages;
    my %colors = (Red=>1, Green=>1, Blue=>1, Alpha=>1);
    my @types;
    my @usages;
    for my $u (@u) {
	for my $t (@t) {
	    $r->CreateColumn("$t $u", $t, $u); # do not warn about RAT column types
            push @types, $t;
            push @usages, $u;
	}
    }
    $DOWARN = 1;
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

SKIP: {
    my $src;
    eval {
        $src = Geo::OSR::SpatialReference->new(EPSG => 2392);
    };
    
    skip "GDAL support files not found. Please set GDAL_DATA.", 1 if $@;

    my $xml = $src->ExportToXML();
    $a = Geo::GDAL::ParseXMLString($xml);
    $xml = Geo::GDAL::SerializeXMLTree($a);
    $b = Geo::GDAL::ParseXMLString($xml);
    ok(is_deeply($a, $b), "xml parsing");
}

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

    my $name = 'MEM';

    my $driver = Geo::GDAL::Driver($name);
	
    my @create = (qw/Byte Float32 UInt16 Int16 CInt16 CInt32 CFloat32/);

    push @tested_drivers, $name;

    for my $type (@create) {

        my $filename = "tmp_ds_".$driver->{ShortName}."_$type";
        my $width = 100;
        my $height = 50;
        my $bands = 1;
        my $options = undef;
        
        my $dataset;
        
        eval {
            $dataset = $driver->Create($filename, $width, $height, $bands, $type, {});
        };
        
        mytest($dataset, @$, $name, $type, "$name $type dataset create");
        next unless $dataset;
        
        mytest($dataset->{RasterXSize} == $width,'RasterXSize',$name,$type,'RasterXSize');
        mytest($dataset->{RasterYSize} == $height,'RasterYSize',$name,$type,'RasterYSize');
        
        my $band = $dataset->GetRasterBand(1);
        
        my $transform = $dataset->GetGeoTransform();
        $transform->[5] = 12;
        $dataset->SetGeoTransform($transform);
        my $transform2 = $dataset->GetGeoTransform();
        mytest($transform->[5] == $transform2->[5],
               "$transform->[5] != $transform2->[5]",$name,$type,'Get/SetGeoTransform');
        
        $band->ColorInterpretation('GreenBand');
        my $value = $band->ColorInterpretation;
        mytest($value eq 'GreenBand',"$value ne GreenBand",$name,$type,'ColorInterpretation');
            
        $band->SetNoDataValue(5);
        my $value = $band->GetNoDataValue;
        mytest($value == 5,"$value != 5",$name,$type,'Get/SetNoDataValue');
        
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
        
        my $pc;
        eval {
            $pc = Geo::GDAL::PackCharacter($band->{DataType});
        };
        
        if ($pc) {
            $pc = "${pc}[$width]";
            my $scanline = pack($pc,(1..$width));
        
            for my $yoff (0..$height-1) {
                $band->WriteRaster( 0, $yoff, $width, 1, $scanline );
            }
        }
        
        my @gcps = ();
        push @gcps,new Geo::GDAL::GCP(1.1,2.2);
        push @gcps,new Geo::GDAL::GCP(2.1,3.2);
        my $po = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Latitude\",NORTH],AXIS[\"Longitude\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]";
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
        
        undef $band;
        undef $dataset;
        
        unless ($name eq 'MEM') {
            eval {
                $dataset = Geo::GDAL::Open($filename);
            };
            mytest($dataset,'no message',$name,$type,"open $filename");
        }
        
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
