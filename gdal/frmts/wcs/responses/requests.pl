use strict;
use warnings;
use v5.10;
use XML::LibXML;

my %do = map {$_ => 1} @ARGV;

system "rm -rf /home/ajolma/.gdal/wcs_cache" unless $do{no_clear};

my $size = 60;

my $setup = {
    SimpleGeoServer => {
        URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
        Options => [
            "", 
            "-oo OuterExtents", 
            "-oo OuterExtents", 
            ""
            ],
        Projwin => "-projwin 145300 6737500 209680 6688700",
        Outsize => "-outsize $size 0",
        Coverage => ['smartsea:eusm2016', 'smartsea:eusm2016', 'smartsea:eusm2016', 'smartsea__eusm2016'],
        Versions => [100, 110, 111, 201]
    },
    GeoServer2 => {
        URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
        Options => [
            "", 
            "-oo OuterExtents -oo NoGridAxisSwap",
            "-oo OuterExtents -oo NoGridAxisSwap",
            "-oo NoGridAxisSwap -oo SubsetAxisSwap"
            ],
        Projwin => "-projwin 145300 6737500 209680 6688700",
        Outsize => "-outsize $size 0",
        Coverage => ['smartsea:south', 'smartsea:south', 'smartsea:south', 'smartsea__south'],
        Versions => [100, 110, 111, 201]
    },
    GeoServer => {
        URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
        Options => [
            "", 
            "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
            "-oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
            "-oo NoGridAxisSwap -oo SubsetAxisSwap"
            ],
        Projwin => "-projwin 3200000 6670000 3280000 6620000",
        Outsize => "-outsize $size 0",
        Coverage => ['smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393', 'smartsea__eusm2016-EPSG2393'],
        Versions => [100, 110, 111, 201]
    },
    MapServer => {
        URL => 'http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows',
        Options => [
            "-oo OriginAtBoundary -oo BandIdentifier=none",
            "-oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
            "-oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
            "-oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
            "-oo OriginAtBoundary",
            ],
        Projwin => "-projwin 10 45 15 35",
        Outsize => "-outsize $size 0",
        Coverage => 'BGS_EMODNET_CentralMed-MCol',
        Versions => [100, 110, 111, 112, 201]
    },
    Rasdaman => {
        URL => 'http://ows.rasdaman.org/rasdaman/ows',
        Options => "",
        Projwin => "-projwin 10 45 15 35",
        Outsize => "-outsize $size 0",
        Coverage => 'BlueMarbleCov',
        Versions => [201]
    },
    ArcGIS => {
        URL => 'http://paikkatieto.ymparisto.fi/arcgis/services/Testit/Velmu_wcs_testi/MapServer/WCSServer',
        Options => [
            "",
            "-oo NrOffsets=2",
            "-oo NrOffsets=2",
            "-oo NrOffsets=2",
            "-oo UseScaleFactor"
            ],
        Projwin => "-projwin 181000 7005000 200000 6980000",
        Outsize => "-outsize $size 0",
        Coverage => [2, 2, 2, 2, 'Coverage2'],
        Versions => [100, 110, 111, 112, 201]
    }
};

my $swapped = {
    2393 => 1,
    3047 => 1,
    4326 => 1
};

my $parser = XML::LibXML->new(no_blanks => 1);
my $coverage_param;

for my $server (sort keys %$setup) {
    next unless $do{$server} || $do{all_servers};
    for my $i (0..$#{$setup->{$server}->{Versions}}) {

        my $url = $setup->{$server}->{URL};
        my $v = $setup->{$server}{Versions}[$i];
        my $version = int($v / 100) . '.' . int($v % 100 / 10) . '.' . ($v % 10);
        next unless $do{$version} || $do{all_versions};
        my $coverage = $setup->{$server}{Coverage};
        $coverage = $coverage->[$i] if ref $coverage;

        if ($version eq '1.0.0') {
            $coverage_param = 'coverage';
        } elsif ($version =~ /^1.1/) {
            $coverage_param = 'identifiers';
        } else {
            $coverage_param = 'coverageid';
        }

        my $gc = "GetCapabilities-$server-$version.xml";
        my $gc_request = "$url?SERVICE=WCS&VERSION=$version&REQUEST=GetCapabilities";
        unless (-e $gc) {
            system "wget \"$gc_request\" -O $gc"
        }
        
        my $dc = "DescribeCoverage-$server-$version.xml";
        my $dc_request = "$url?SERVICE=WCS&VERSION=$version&REQUEST=DescribeCoverage&$coverage_param=$coverage";
        unless (-e $dc) {
            system "wget \"$dc_request\" -O $dc"
        }
        
        #say $server.'-'.$version;
        my $options = $setup->{$server}{Options};
        $options = $options->[$i] if ref $options;
        
        #system "gdalinfo $options -oo filename=$server-$version \"WCS:$url?version=$version&coverage=$coverage\"";
        #next;       

        my ($result, $o, $output);

        if (0) {
            $result = "$server-$version-scaled.tiff";
            $o = "$options $setup->{$server}->{Projwin} $setup->{$server}->{Outsize}";
            say "gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result" if $do{show};
            $output = qx(gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result 2>&1);
            output($server, $version, $result, $output);
        }
        
        # non-scaled from the boundary

        # load the DC
        #say $dc;
        my $dom = $parser->load_xml(location => $dc);
        my $doc = $dom->documentElement;
        my $xc = XML::LibXML::XPathContext->new($doc);
        my $wcs;
        if ($version eq '1.0.0') {
            $wcs = $doc->lookupNamespacePrefix('http://www.opengis.net/wcs');
            unless ($wcs) {
                $wcs = 'wcs';
                $xc->registerNs($wcs, 'http://www.opengis.net/wcs');
            }
        } elsif ($version =~ /^1.1/) {
            $wcs = $doc->lookupNamespacePrefix('http://www.opengis.net/wcs/1.1');
            $wcs //= $doc->lookupNamespacePrefix('http://www.opengis.net/wcs/1.1.1');
            unless ($wcs) {
                $wcs = 'wcs';
                my $ns;
                for my $n ($doc->getNamespaces) {
                    $ns = $n->value unless $n->name =~ /:/;
                }
                $xc->registerNs($wcs, $ns);
            }
        } else {
            $wcs = $doc->lookupNamespacePrefix('http://www.opengis.net/wcs/2.0');
            unless ($wcs) {
                $wcs = 'wcs';
                $xc->registerNs($wcs, 'http://www.opengis.net/wcs/2.0');
            }
        }
        die "WCS namespace undefined." unless defined $wcs;
        my @low;
        my @high;
        my $crs;
        if ($version eq '1.0.0') {
            $crs = $xc->find("//$wcs:nativeCRSs")->get_node(1);
            $crs //= $xc->find('//gml:Envelope/@srsName')->get_node(1);
            $crs = $crs->textContent;
            my @o = $doc->findnodes('//gml:Envelope/gml:pos');
            @low = split(/\s/, $o[0]->textContent);
            @high = split(/\s/, $o[1]->textContent);
        } elsif ($version =~ /^1.1/) {
            $crs = $xc->find("//$wcs:GridBaseCRS")->get_node(1)->textContent;
            $o = $doc->find("//ows:BoundingBox[\@crs='".$crs."']")->get_node(1);
            @low = split(/\s/, $o->find('ows:LowerCorner')->get_node(1)->textContent);
            @high = split(/\s/, $o->find('ows:UpperCorner')->get_node(1)->textContent);
        } else {
            $crs = $xc->find('//gml:Envelope/@srsName')->get_node(1)->textContent;
            my $o = $doc->find('//gml:lowerCorner')->get_node(1);
            @low = split(/\s/, $o->textContent);
            $o = $dom->documentElement->find('//gml:upperCorner')->get_node(1);
            @high = split(/\s/, $o->textContent);
        }
        my ($epsg) = $crs =~ /EPSG.*?(\d\d+)/;
        my $swap = $version ne '1.0.0' && $swapped->{$epsg};
        if ($swap) {
            unshift @low, $low[1];
            delete $low[2];
            unshift @high, $high[1];
            delete $high[2];
        }
        
        $result = "$server-$version.tiff";
        $o = "$options -srcwin 0 0 2 2";
        say "gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result" if $do{show};
        $output = qx(gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result 2>&1);
        say $output if $do{all};
        my $ok = 'not ok';
        $output = qx(gdalinfo $result 2>&1);
        my @origin;
        my @pixel_size;
        foreach my $line (split /[\r\n]+/, $output) {
            if ($line =~ /Origin = \((.*?)\)/) {
                @origin = split /,/, $1;
            }
            if ($line =~ /Pixel Size = \((.*?)\)/) {
                @pixel_size = split /,/, $1;
            }
        }
        if ($do{all}) {
            say $swap ? 'swap' : 'no swap';
            say "bbox = @low, @high, $crs" ;
            say "origin = @origin";
            say "pixel_size = @pixel_size";
        }
 
        # origin is x,y; low and high are OGC
        print $server.'-'.$version;
        my $eps = 0.000001;
        my $dx = $origin[0] - $low[0];
        my $dy = $origin[1] - $high[1];
        if (abs($dx) < $eps and abs($dy) < $eps) {
            say " ok";
        } else {
            say " not ok ($dx, $dy)";
        }
        
        #exit;
    }
}

sub output {
    my ($server, $version, $result, $output) = @_;
    my @full_output;
    foreach my $line (split /[\r\n]+/, $output) {
        say $line if $line =~ /URL=/;
        $line =~ s/^.+?URL=/URL=/;
        push @full_output, $line;
    }
    $output = qx(gdalinfo $result 2>&1);
    my $ok = 'not ok';
    foreach my $line (split /[\r\n]+/, $output) {
        #say $line;
        if ($line =~ /Size is (\d+), (\d)/) {
            if ($1 == $size) {
                $ok = "ok";
            }
        }
        push @full_output, $line;
    }
    say $server.'-'.$version.' '.$ok;
    if ($ok eq 'not ok' or $do{show}) {
        say "===";
        for (@full_output) {
            say "$_" if $do{all} || /URL/;
        }
        say "==="
    }
}
