use strict;
use warnings;
use v5.10;
use XML::LibXML;

# args: server name (or all_servers),
# version (or all_versions),
# test_scaled, test_non_scaled, or test_all
# clear_all: remove all XMLs and TIFFs
# say, say_all (optional debugging)
my %do = map {$_ => 1} @ARGV;
$do{say} = 1 if $do{say_all};

my $cache = 'wcs_cache';
my $size = 60;
my $first_call = $do{clear_cache} ? 1 : 0;

my @not_ok;

my $setup = get_setup();

my $swapped = {
    2393 => 1,
    3047 => 1,
    4326 => 1
};

my $parser = XML::LibXML->new(no_blanks => 1);
my $coverage_param;
my %urls; # to collect the URLs, WCSDataset::GetCoverage must print it out

for my $server (sort keys %$setup) {
    next unless $do{$server} || $do{all_servers};
    for my $i (0..$#{$setup->{$server}->{Versions}}) {

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
        my $dc = "DescribeCoverage-$server-$version.xml";

        if ($do{clear_all}) {
            unlink($gc, $dc);
        }

        # download GC and DC unless they exist
        my $url = $setup->{$server}->{URL};

        my $gc_request = "$url?SERVICE=WCS&VERSION=$version&REQUEST=GetCapabilities";
        unless (-e $gc) {
            system "wget \"$gc_request\" -O $gc"
        }

        my $dc_request = "$url?SERVICE=WCS&VERSION=$version&REQUEST=DescribeCoverage&$coverage_param=$coverage";
        unless (-e $dc) {
            system "wget \"$dc_request\" -O $dc"
        }

        my $options = $setup->{$server}{Options};
        $options = $options->[$i] if ref $options;

        $options .= " -oo CACHE=$cache";

        # test that the origin of the 2 x 2 non-scaled piece obtained with gdal_translate
        # is the top left boundary of the BBOX in DC
        # tests implicitly many things
        test_non_scaled($server, $version, $dc, $coverage, $options)
            if $do{test_non_scaled} || $do{test_all};

        # test that the width of the scaled piece obtained with gdal_translate
        # is what it was asked
        # tests implicitly many things
        test_scaled($server, $version, $coverage, $options)
            if $do{test_scaled} || $do{test_all};

        # test range subsetting with 2.0.1
        test_range_subsetting($server, $version, $coverage, $options)
            if $do{test_range_subsetting} || $do{test_all};

        # test dimension subsetting with 2.0.1
        test_dimension_subsetting($server, $version, $coverage, $options)
            if $do{test_dimension_subsetting} || $do{test_all};

    }
}

if ($do{urls}) {
    open(my $fh, ">", "urls");
    for my $key (sort keys %urls) {
        for my $test (sort keys %{$urls{$key}}) {
            say $fh $key,' ',$test,' ',$urls{$key}{$test};
        }
    }
    close $fh;
}

if (@not_ok) {
    say "Failed tests were:";
    for my $fail (@not_ok) {
        say $fail;
    }
} else {
    say "No failed tests.";
}

sub get_setup {
    return {
        SimpleGeoServer => {
            URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            Options => [
                "-oo INTERLEAVE=PIXEL ",
                "-oo INTERLEAVE=PIXEL -oo OuterExtents",
                "-oo INTERLEAVE=PIXEL -oo OuterExtents",
                ""
                ],
            Projwin => "-projwin 145300 6737500 209680 6688700",
            Outsize => "-outsize $size 0",
            Coverage => [
                'smartsea:eusm2016', 'smartsea:eusm2016',
                'smartsea:eusm2016', 'smartsea__eusm2016'],
            Versions => [100, 110, 111, 201],
        },
        GeoServer2 => {
            URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            Options => [
                "-oo INTERLEAVE=PIXEL ",
                "-oo INTERLEAVE=PIXEL -oo OuterExtents -oo NoGridAxisSwap",
                "-oo INTERLEAVE=PIXEL -oo OuterExtents -oo NoGridAxisSwap",
                "-oo INTERLEAVE=PIXEL -oo NoGridAxisSwap -oo SubsetAxisSwap"
                ],
            Projwin => "-projwin 145300 6737500 209680 6688700",
            Outsize => "-outsize $size 0",
            Coverage => ['smartsea:south', 'smartsea:south', 'smartsea:south', 'smartsea__south'],
            Versions => [100, 110, 111, 201],
            Range => [qw/GREEN_BAND BLUE_BAND/]
        },
        GeoServer => {
            URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
            Options => [
                "-oo INTERLEAVE=PIXEL",
                "-oo INTERLEAVE=PIXEL -oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo INTERLEAVE=PIXEL -oo OuterExtents -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap",
                "-oo INTERLEAVE=PIXEL -oo NoGridAxisSwap -oo SubsetAxisSwap",
                ],
            Projwin => "-projwin 3200000 6670000 3280000 6620000",
            Outsize => "-outsize $size 0",
            Coverage => [
                'smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393',
                'smartsea:eusm2016-EPSG2393', 'smartsea__eusm2016-EPSG2393'],
            Versions => [100, 110, 111, 201]
        },
        MapServer => {
            URL => 'http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows',
            Options => [
                "-oo INTERLEAVE=PIXEL -oo OriginAtBoundary -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
                "-oo INTERLEAVE=PIXEL -oo OffsetsPositive -oo NrOffsets=2 -oo NoGridAxisSwap -oo BandIdentifier=none",
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
        Rasdaman2 => {
            URL => 'http://ows.rasdaman.org/rasdaman/ows',
            Options => "-oo \"subset=unix(\"2008-01-05T01:58:30.000Z\")\"",
            Projwin => "-projwin 100000 5400000 150000 5100000",
            Outsize => "-outsize $size 0",
            Coverage => 'test_irr_cube_2',
            Versions => [201],
            Dimension => "unix(\"2008-01-05T01:58:30.000Z\")"
        },
        ArcGIS => {
            URL => 'http://paikkatieto.ymparisto.fi/arcgis/services/Testit/Velmu_wcs_testi/MapServer/WCSServer',
            Options => [
                "-oo INTERLEAVE=PIXEL ",
                "-oo INTERLEAVE=PIXEL -oo NrOffsets=2",
                "-oo INTERLEAVE=PIXEL -oo NrOffsets=2",
                "-oo INTERLEAVE=PIXEL -oo NrOffsets=2",
                "-oo UseScaleFactor"
                ],
            Projwin => "-projwin 181000 7005000 200000 6980000",
            Outsize => "-outsize $size 0",
            Coverage => [2, 2, 2, 2, 'Coverage2'],
            Versions => [100, 110, 111, 112, 201]
        },
        beta_karttakuva => {
            URL => 'https://beta-karttakuva.maanmittauslaitos.fi/wcs/service/ows',
            slice => 'time(1985-01-01T00:00:00.000Z)',
            Versions => [201],
            Coverage => 'ortokuva__ortokuva',
            Outsize => "-outsize $size 0",
            Projwin => "-projwin 377418 6683393.87938218 377717.879386966 6683094",
            Options => [
                "-oo INTERLEAVE=PIXEL",
                ],
        }
    };
}

sub test_dimension_subsetting {
    my ($server, $version, $coverage, $options) = @_;
    return unless $setup->{$server}->{Dimension};
    return unless $version eq '2.0.1';
    my $o = "$options -srcwin 0 0 2 2";
    if ($first_call) {
        $o .= " -oo CLEAR_CACHE";
        $first_call = 0;
    }
    my $url = $setup->{$server}->{URL};
    $url .= "?version=$version&coverage=$coverage";
    $url .= "&subset=$setup->{$server}->{Dimension}";
    my $result = 'x.tiff';
    my $cmd = "gdal_translate $o \"WCS:$url\" $result 2>&1";
    say $cmd if $do{say};
    my $output = `$cmd`;
    say $output if $do{say_all};
    foreach my $line (split /[\r\n]+/, $output) {
        if ($line =~ /URL=(.*)/) {
            my $key = 'dimension_subsetting';
            $key .= '2' if $urls{$server.'-'.$version}{$key};
            $urls{$server.'-'.$version}{$key} = $1;
        }
    }
}

sub test_range_subsetting {
    my ($server, $version, $coverage, $options) = @_;
    return unless $setup->{$server}->{Range};
    return unless $version eq '2.0.1';
    my $o = "$options -srcwin 0 0 2 2";
    if ($first_call) {
        $o .= " -oo CLEAR_CACHE";
        $first_call = 0;
    }
    my $url = $setup->{$server}->{URL};
    $url .= "?version=$version&coverage=$coverage";
    my $range = join(',', @{$setup->{$server}->{Range}});
    $url .= "&rangesubset=$range";
    my $result = 'x.tiff';
    my $cmd = "gdal_translate $o \"WCS:$url\" $result 2>&1";
    say $cmd if $do{say};
    my $output = `$cmd`;
    say $output if $do{say_all};
    foreach my $line (split /[\r\n]+/, $output) {
        if ($line =~ /URL=(.*)/) {
            my $key = 'range_subsetting';
            $key .= '2' if $urls{$server.'-'.$version}{$key};
            $urls{$server.'-'.$version}{$key} = $1;
        }
    }
}

sub test_non_scaled {
    my ($server, $version, $dc, $coverage, $options) = @_;

    # load the DC
    my $dom = $parser->load_xml(location => $dc);
    my $doc = $dom->documentElement;
    my $xc = XML::LibXML::XPathContext->new($doc);
    my $wcs;
    if ($version eq '1.0.0') {
        $wcs = $doc->lookupNamespacePrefix('http://www.opengis.net/wcs');
    } elsif ($version =~ /^1.1/) {
        $wcs = $doc->lookupNamespacePrefix('http://www.opengis.net/wcs/1.1');
        $wcs //= $doc->lookupNamespacePrefix('http://www.opengis.net/wcs/1.1.1');
    } else {
        $wcs = $doc->lookupNamespacePrefix('http://www.opengis.net/wcs/2.0');
    }
    unless ($wcs) {
        $wcs = 'wcs';
        my $ns;
        for my $n ($doc->getNamespaces) {
            $ns = $n->value unless $n->name =~ /:/;
        }
        $xc->registerNs($wcs, $ns);
    }
    die "WCS namespace undefined." unless defined $wcs;
    my @low;
    my @high;
    my $crs;
    # grab the CRS and BBOX from the DC
    if ($version eq '1.0.0') {
        $crs = $xc->find("//$wcs:nativeCRSs")->get_node(1);
        $crs //= $xc->find('//gml:Envelope/@srsName')->get_node(1);
        $crs = $crs->textContent;
        my @o = $doc->findnodes('//gml:Envelope/gml:pos');
        @low = split(/\s/, $o[0]->textContent);
        @high = split(/\s/, $o[1]->textContent);
    } elsif ($version =~ /^1.1/) {
        $crs = $xc->find("//$wcs:GridBaseCRS")->get_node(1)->textContent;
        my $o = $doc->find("//ows:BoundingBox[\@crs='".$crs."']")->get_node(1);
        @low = split(/\s/, $o->find('ows:LowerCorner')->get_node(1)->textContent);
        @high = split(/\s/, $o->find('ows:UpperCorner')->get_node(1)->textContent);
    } else {
        my $srs = $xc->find('//gml:Envelope/@srsName')->get_node(1) //
            $xc->find('//gml:EnvelopeWithTimePeriod/@srsName')->get_node(1);
        $crs = $srs->textContent;
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

    my $result = "$server-$version.tiff";
    if ($do{clear_all}) {
        unlink($result);
    }
    my $o = "$options -oo \"filename=$server-$version-non_scaled.tiff\" -srcwin 0 0 2 2";
    if ($first_call) {
        $o .= " -oo CLEAR_CACHE";
        $first_call = 0;
    }
    my $url = $setup->{$server}->{URL};
    my $cmd = "gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result 2>&1";
    say $cmd if $do{say};
    my $output = `$cmd`;
    say $output if $do{say_all};
    foreach my $line (split /[\r\n]+/, $output) {
        if ($line =~ /URL=(.*)/) {
            my $key = 'non_scaled';
            $key .= '2' if $urls{$server.'-'.$version}{$key};
            $urls{$server.'-'.$version}{$key} = $1;
        }
    }

    # test
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
    if ($do{say_all}) {
        say $swap ? 'swap' : 'no swap';
        say "bbox = @low, @high, $crs" ;
        say "origin = @origin";
        say "pixel_size = @pixel_size";
    }

    # origin is x,y; low and high are OGC
    print $server.'-'.$version.' non-scaled ';
    my $eps = 0.000001;
    my $dx = $origin[0] - $low[0];
    my $dy = $origin[1] - $high[1];
    if (abs($dx) < $eps and abs($dy) < $eps) {
        say " ok";
    } else {
        say " not ok ($dx, $dy)";
        push @not_ok, $server.'-'.$version.' non-scaled '." not ok ($dx, $dy)";
    }
}

sub test_scaled {
    my ($server, $version, $coverage, $options) = @_;
    my $url = $setup->{$server}->{URL};
    my $result = "$server-$version-scaled.tiff";
    if ($do{clear_all}) {
        unlink($result);
    }
    my $o = "$options $setup->{$server}->{Projwin} $setup->{$server}->{Outsize}";
    if ($first_call) {
        $o .= " -oo CLEAR_CACHE";
        $first_call = 0;
    }
    my $slice = '';
    if ($setup->{$server}->{slice}) {
        $slice = "-oo Subset=\"$setup->{$server}->{slice}\"";
    }
    my $cmd = "gdal_translate $o $slice \"WCS:$url?version=$version&coverage=$coverage\" $result 2>&1";
    say $cmd if $do{say};
    my $output = `$cmd`;
    my @full_output;
    foreach my $line (split /[\r\n]+/, $output) {
        if ($line =~ /URL=(.*)/) {
            my $key = 'scaled';
            $key .= '2' if $urls{$server.'-'.$version}{$key};
            $urls{$server.'-'.$version}{$key} = $1;
        }
        push @full_output, $line;
    }
    $output = qx(gdalinfo $result 2>&1);
    my $ok = 'not ok';
    foreach my $line (split /[\r\n]+/, $output) {
        if ($line =~ /Size is (\d+), (\d)/) {
            if ($1 == $size) {
                $ok = "ok";
            }
        }
        push @full_output, $line;
    }
    say $server.'-'.$version.' scaled '.$ok;
    push @not_ok, $server.'-'.$version.' scaled '." not ok" if $ok eq 'not ok';
    if ($ok eq 'not ok' or $do{say}) {
        say "===";
        for (@full_output) {
            say "$_" if $do{say_all} || /URL/;
        }
        say "==="
    }
}
