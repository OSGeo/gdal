use strict;
use v5.10;

system "rm -rf /home/ajolma/.gdal/wcs_cache";

my $size = 60;

my $setup = {
    SimpleGeoServer => {
        URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
        Options => [
            "", 
            "-oo OuterExtents=TRUE", 
            "-oo OuterExtents=TRUE", 
            ""
            ],
        Projwin => "-projwin 145300 6737500 209680 6688700",
        Outsize => "-outsize $size 0",
        Coverage => ['smartsea:eusm2016', 'smartsea:eusm2016', 'smartsea:eusm2016', 'smartsea__eusm2016'],
        Versions => [100, 110, 111, 201]
    },
    GeoServer => {
        URL => 'https://msp.smartsea.fmi.fi/geoserver/wcs',
        Options => [
            "", 
            "-oo OuterExtents=TRUE -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap=TRUE",
            "-oo OuterExtents=TRUE -oo BufSizeAdjust=0.5 -oo NoGridAxisSwap=TRUE",
            "-oo NoGridAxisSwap=TRUE -oo SubsetAxisSwap=TRUE"
            ],
        Projwin => "-projwin 3200000 6670000 3280000 6620000",
        Outsize => "-outsize $size 0",
        Coverage => ['smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393', 'smartsea:eusm2016-EPSG2393', 'smartsea__eusm2016-EPSG2393'],
        Versions => [100, 110, 111, 201]
    },
    MapServer => {
        URL => 'http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows',
        Options => [
            "-oo OriginNotCenter100=TRUE",
            "-oo OffsetsPositive=TRUE -oo NrOffsets=2 -oo NoGridAxisSwap=TRUE",
            "-oo OffsetsPositive=TRUE -oo NrOffsets=2 -oo NoGridAxisSwap=TRUE",
            "-oo OffsetsPositive=TRUE -oo NrOffsets=2 -oo NoGridAxisSwap=TRUE",
            "-oo GridAxisLabelSwap=TRUE",
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
            "-oo UseScaleFactor=TRUE"
            ],
        Projwin => "-projwin 181000 7005000 200000 6980000",
        Outsize => "-outsize $size 0",
        Coverage => [2, 2, 2, 2, 'Coverage2'],
        Versions => [100, 110, 111, 112, 201]
    }
};

my %do = map {$_ => 1} @ARGV;

for my $server (sort keys %$setup) {
    next unless $do{$server} || $do{all_servers};
    for my $i (0..$#{$setup->{$server}->{Versions}}) {
        my $url = $setup->{$server}->{URL};
        my $v = $setup->{$server}{Versions}[$i];
        my $version = int($v / 100) . '.' . int($v % 100 / 10) . '.' . ($v % 10);
        next unless $do{$version} || $do{all_versions};
        #say $server.'-'.$version;
        my $coverage = $setup->{$server}{Coverage};
        $coverage = $coverage->[$i] if ref $coverage;
        my $options = $setup->{$server}{Options};
        $options = $options->[$i] if ref $options;
        
        #system "gdalinfo $options -oo filename=$server-$version \"WCS:$url?version=$version&coverage=$coverage\"";
        #next;
        
        my $result = "$server-$version-scaled.tiff";
        my $o = "$options $setup->{$server}->{Projwin} $setup->{$server}->{Outsize}";
        say "gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result" if $do{show};
        my $output = qx(gdal_translate $o \"WCS:$url?version=$version&coverage=$coverage\" $result 2>&1);
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
        #exit;
    }
}
