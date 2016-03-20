use strict;
use warnings;
use bytes;
use v5.10;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# test measured geometries in shape driver

my @data = (
    Point => 'POINT (1 2)',
    LineString => 'LINESTRING (1 2)',
    Polygon => 'POLYGON ((1 2))',
    MultiPoint => 'MULTIPOINT ((1 2))',

    PointZ => 'POINT Z (1 2 3)',
    LineStringZ => 'LINESTRING Z (1 2 3)',
    PolygonZ => 'POLYGON Z ((1 2 3))',
    MultiPointZ => 'MULTIPOINT Z ((1 2 3))',

    PointM => 'POINT M (1 2 3)',
    LineStringM => 'LINESTRING M (1 2 3)',
    PolygonM => 'POLYGON M ((1 2 3))',
    MultiPointM => 'MULTIPOINT M ((1 2 3))',

    MultiLineStringM => 'MULTILINESTRING M ((1 2 3))',
    MultiPolygonM => 'MULTIPOLYGON M (((1 2 3)))',
#    CircularStringM => 'CIRCULARSTRING M (1 2 3,1 2 3,1 2 3)',
#    CompoundCurveM => 'COMPOUNDCURVE M ((0 1 2,2 3 4))',
#    CurvePolygonM => 'CURVEPOLYGON M ((0 0 1,0 1 1,1 1 1,1 0 1,0 0 1))',
#    MultiCurveM => 'MULTICURVE M (CIRCULARSTRING M (0 0 2,1 0 2,0 0 2),(0 0 2,1 1 2))',
#    MultiSurfaceM => 'MULTISURFACE M (((1 2 3)))',

    PointZM => 'POINT ZM (1 2 3 4)',
    LineStringZM => 'LINESTRING ZM (1 2 3 4)',
    PolygonZM => 'POLYGON ZM ((1 2 3 4))',
    MultiPointZM => 'MULTIPOINT ZM ((1 2 3 4))',

    MultiLineStringZM => 'MULTILINESTRING ZM ((1 2 3 4))',
    MultiPolygonZM => 'MULTIPOLYGON ZM (((1 2 3 4)))',
#    CircularStringZM => 'CIRCULARSTRING ZM (1 2 3 4,1 2 3 4,1 2 3 4)',
#    CompoundCurveZM => 'COMPOUNDCURVE ZM ((0 1 2 3,2 3 4 5))',
#    CurvePolygonZM => 'CURVEPOLYGON ZM ((0 0 1 2,0 1 1 2,1 1 1 2,1 0 1 2,0 0 1 2))',
#    MultiCurveZM => 'MULTICURVE ZM (CIRCULARSTRING ZM (0 0 2 3,1 0 2 3,0 0 2 3),(0 0 2 3,1 1 2 3))',
#    MultiSurfaceZM => 'MULTISURFACE ZM (((1 2 3 4)))',
);

my $driver = 'ESRI Shapefile';
my $dir = '/vsimem/';

if (1) {
for (my $i = 0; $i < @data; $i+=2) {
    my $type = $data[$i];
    my $l = Geo::OGR::Driver($driver)->Create($dir.$type.'.shp')->CreateLayer(GeometryType => $type);
    eval {
        $l->InsertFeature({Geometry => {WKT => $data[$i+1]}});
    };
    if ($@) {
        my @e = split /\n/, $@;
        $e[0] =~ s/\. at .*/./;
        ok(0, "$driver, insert feature: $type => $data[$i+1] ($e[0])");
        next;
    }
    
    # close and open
    undef $l;
    $l = Geo::OGR::Open($dir)->GetLayer($type);
    my $t = $l->GeometryType;
    $t =~ s/25D/Z/;
    my $exp = $type;
    $exp =~ s/^Multi// if $exp =~ /MultiLine|MultiPoly/;
    ok($t eq $exp, "$driver layer geom type: expected: $exp, got: $t");

    $l->ResetReading;
    while (my $f = $l->GetNextFeature()) {
        my $g = $f->GetGeometryRef;
        my $t = $g->GeometryType;
        $t =~ s/25D/Z/;
        my $exp = $type;
        $exp =~ s/^Multi// if $exp =~ /MultiLine|MultiPoly/;
        ok($t eq $exp, "$driver retrieve feature: expected: $exp, got: $t");
        my $wkt = $g->As(Format => 'ISO WKT');
        $exp = $data[$i+1];
        if ($exp =~ /MULTILINE|MULTIPOLY/) {
            $exp =~ s/^MULTI//;
            $exp =~ s/\(\(/\(/;
            $exp =~ s/\)\)/\)/;
        }
        ok($wkt eq $exp, "$driver retrieve feature: $type, expected: $exp got: $wkt");
    }
}
}

# test opening with requested type

@data = (
    Point => 'POINT (1 2)',
    LineString => 'LINESTRING (1 2)',
    Polygon => 'POLYGON ((1 2))',
    MultiPoint => 'MULTIPOINT ((1 2))',

    PointZ => 'POINT Z (1 2 3)',
    LineStringZ => 'LINESTRING Z (1 2 3)',
    PolygonZ => 'POLYGON Z ((1 2 3))',
    MultiPointZ => 'MULTIPOINT Z ((1 2 3))',

    PointM => 'POINT M (1 2 3)',
    LineStringM => 'LINESTRING M (1 2 3)',
    PolygonM => 'POLYGON M ((1 2 3))',
    MultiPointM => 'MULTIPOINT M ((1 2 3))',

    PointZM => 'POINT ZM (1 2 3 4)',
    LineStringZM => 'LINESTRING ZM (1 2 3 4)',
    PolygonZM => 'POLYGON ZM ((1 2 3 4))',
    MultiPointZM => 'MULTIPOINT ZM ((1 2 3 4))',
);
my %data = @data;

my @shpt = (POINT => 'Point', ARC => 'LineString', POLYGON => 'Polygon', MULTIPOINT => 'MultiPoint');
my %shpt = @shpt;

my %type2shpt;
for (my $j = 0; $j < @shpt; $j+=2) {
    $type2shpt{$shpt[$j+1]} = $shpt[$j];
}

if (1) {
for (my $i = 0; $i < @data; $i+=2) {
    my $type = $data[$i];
    my $basetype = $type;
    $basetype =~ s/(M|Z|ZM)$//;
    my $shpt = $type2shpt{$basetype};
    for my $dim ('','Z','M','ZM') {
        my $l = Geo::OGR::Driver($driver)->Create($dir.$type.'.shp')->CreateLayer(
            GeometryType => 'Unknown', 
            Options => { SHPT => $shpt.$dim });
        #my $wkt = $data{$basetype.$dim};
        #my $f0 = Geo::OGR::Feature->new(GeometryType => $basetype.$dim);
        #$f0->Geometry({WKT => $wkt});
        my $wkt = $data[$i+1];
        my $f0 = Geo::OGR::Feature->new(GeometryType => $type);
        $f0->Geometry({WKT => $wkt});
        eval {
            $l->CreateFeature($f0);
        };
        if ($@) {
            my @e = split /\n/, $@;
            $e[0] =~ s/\. at .*/./;
            ok(0, "$driver with option SHPT=$shpt$dim, insert feature: $type => $wkt ($e[0])");
            next;
        } else {
            ok(1, "$driver with option SHPT=$shpt$dim, insert feature: $type => $wkt");
        }
        
        # close and open
        undef $l;
        my $adjust = 'NO';
        if ($dim eq 'Z') { # M is added implicitly
            $adjust = 'ALL_SHAPES'; # but we strip it away with this
        }
        $l = Geo::GDAL::OpenEx(name => $dir, options => {ADJUST_GEOM_TYPE=>$adjust})->GetLayer($type);
        my $t = $l->GeometryType;
        $t =~ s/25D/Z/;
        my $exp = $shpt{$shpt}.$dim;
        ok($t eq $exp, "$driver with option SHPT=$shpt$dim layer geom type ($type): expected: $exp, got: $t");
        
        $l->ResetReading;
        while (my $f = $l->GetNextFeature()) {
            my $g = $f->GetGeometryRef;
            my $t = $g->GeometryType;
            $t =~ s/25D/Z/;
            my $exp = $basetype.$dim;
            ok($t eq $exp, "$driver with option SHPT=$shpt$dim retrieve feature: expected: $exp, got: $t");
            my $wkt = $g->As(Format => 'ISO WKT');
            $exp = $data{$basetype.$dim};
            if (!($type =~ /M$/) and $dim eq 'M') {
                $exp =~ s/3/-0/;
            } elsif (($type =~ /ZM$/) and $dim eq 'Z') {
            } elsif (!($type =~ /Z$/) and $dim eq 'Z') {
                $exp =~ s/3/0/;
            } elsif (($type =~ /ZM$/) and $dim eq 'M') {
                $exp =~ s/4 //;
                $exp =~ s/3/4/;
            } elsif (($type =~ /Z$/) and $dim eq 'ZM') {
                $exp =~ s/4/-0/;
            } elsif (($type =~ /ZM$/) and $dim eq 'ZM') {
            } elsif (($type =~ /ZM$/) and $dim eq 'ZM') {
                $exp =~ s/3 //;
            } elsif (($type =~ /M$/) and $dim eq 'ZM') {
                $exp =~ s/3/0/;
                $exp =~ s/4/3/;
            } elsif (!($type =~ /ZM$/) and $dim eq 'ZM') {
                $exp =~ s/3/0/;
                $exp =~ s/4/-0/;
            }
            $wkt =~ s/-[\d\.e\+]+/-0/; # "no data" M to "-0"
            ok($wkt eq $exp, "$driver with option SHPT=$shpt$dim retrieve feature: expected: $exp got: $wkt");
        }
    }
}
}

# test using no data M values 

@data = (
    PointM => 'POINT M (1 2 -2E38)',
    LineStringM => 'LINESTRING M (1 2 -2E38)',
    PolygonM => 'POLYGON M ((1 2 -2E38))',
    MultiPointM => 'MULTIPOINT M ((1 2 -2E38))',

    PointZM => 'POINT ZM (1 2 3 -2E38)',
    LineStringZM => 'LINESTRING ZM (1 2 3 -2E38)',
    PolygonZM => 'POLYGON ZM ((1 2 3 -2E38))',
    MultiPointZM => 'MULTIPOINT ZM ((1 2 3 -2E38))',
);

my $test = "$driver with no data M";

if (0) {
for (my $i = 0; $i < @data; $i+=2) {
    my $type = $data[$i];
    my $basetype = $type;
    $basetype =~ s/(M|Z|ZM)$//;
    my $fallback_type = $basetype;
    $fallback_type .= $type =~ /ZM$/ ? 'Z' : '';
    my $l = Geo::OGR::Driver($driver)->Create($dir.$type.'.shp')->CreateLayer(GeometryType => $type);
    eval {
        $l->InsertFeature({Geometry => { WKT => $data[$i+1] }});
    };
    if ($@) {
        my @e = split /\n/, $@;
        $e[0] =~ s/\. at .*/./;
        ok(0, "$test, insert feature: $type => $data[$i+1] ($e[0])");
        next;
    } else {
        ok(1, "$test, insert feature: $type => $data[$i+1]");
    }
    
    # close and open
    undef $l;
    $l = Geo::OGR::Open($dir)->GetLayer($type);
    my $t = $l->GeometryType;
    $t =~ s/25D/Z/;
    my $exp = $fallback_type;
    ok($t eq $exp, "$test layer geom type: expected: $exp, got: $t");
    
    $l->ResetReading;
    while (my $f = $l->GetNextFeature()) {
        my $g = $f->GetGeometryRef;
        my $t = $g->GeometryType;
        $t =~ s/25D/Z/;
        my $exp = $fallback_type;
        ok($t eq $exp, "$test retrieve feature: expected: $exp, got: $t");
        my $wkt = $g->As(Format => 'ISO WKT');
        $exp = $data[$i+1];
        $exp =~ s/ -2E38//;
        $exp =~ s/M //;
        ok($wkt eq $exp, "$test retrieve feature: $type, expected: $exp got: $wkt");
    }
}
}
