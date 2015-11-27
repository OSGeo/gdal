use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More tests => 12;
BEGIN { use_ok('Geo::GDAL') };

# test parenting, i.e., that dependent objects keep their parents alive
# child -> parent
# ---------------
# Dataset -> Dataset (virtual dataset) BANDS
# Band -> Dataset BANDS
# Layer -> Dataset LAYERS
# RAT -> Band RATS
# Defn -> Feature DEFNS
# Defn -> Layer DEFNS
# Feature -> Layer FEATURES
# Geometry -> Feature GEOMETRIES
#
# parent class has a CHILDREN hash, which contains
# keys tied(%$child) and $parent values
#
# child class has a method RELEASE_PARENTS, which deletes
# a value from a hash in its parent class
# the $self in RELEASE_PARENTS is already the scalar $self
#
# every time a parent object returns or gives in some other
# way a child object to user, it records the child into the hash

# Band, Layer, Dataset -> Dataset

{
    my $ds = Geo::GDAL::Driver('MEM')->Create();
    my $ds_str = "$ds";
    my $b = $ds->Band;
    undef $ds; # does not really delete the underlying dataset
    my $ds2 = $b->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "band keeps its parent dataset alive");
}

{
    my $ds = Geo::OGR::Driver('Memory')->Create();
    my $ds_str = "$ds";
    my $l = $ds->CreateLayer(Name => 'layer');
    undef $ds; # does not really delete the underlying dataset
    my $ds2 = $l->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "layer keeps its parent dataset alive");
}

{
    my $ds = Geo::GDAL::Driver('MEM')->Create();
    my $gt = $ds->GeoTransform(0,2,0,0,0,-1);
    my $ds_str = "$ds";
    my $d = $ds->Warped();
    undef $ds; # does not really delete the underlying dataset
    my $ds2 = $d->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "a virtual dataset keeps its parent dataset alive");
}

# RasterAttributeTable -> Band

{
    my $ds = Geo::GDAL::Driver('GTiff')->Create(Name => '/vsimem/tmp');
    my $ds_str = "$ds";
    my $rat = $ds->Band->RasterAttributeTable(Geo::GDAL::RasterAttributeTable->new);
    undef $ds; # does not really delete the underlying dataset and band
    my $ds2 = $rat->Band->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "a raster attribute table keeps its parent band alive");
}

# FeatureDefn -> Feature, Layer

{
    my $ds = Geo::OGR::Driver('Memory')->Create();
    my $ds_str = "$ds";
    my $fd = $ds->CreateLayer(Name => 'layer', Fields => [{Name => 'test', Type => 'Integer'}])
        ->InsertFeature({test => 13})
        ->GetDefn;
    undef $ds; # does not really delete the underlying dataset
    my $ds2 = $fd->Feature->Layer->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "FeatureDefn keeps its parent dataset alive");
}

{
    my $ds = Geo::OGR::Driver('Memory')->Create();
    my $ds_str = "$ds";
    my $l = $ds->CreateLayer(Name => 'layer', Fields => [{Name => 'test', Type => 'Integer'}]);
    $l->InsertFeature({test => 13});
    my $f;
    $l->ForFeatures(sub {$f = shift});
    ok($f->{test} == 13, "Got back the right feature with GetNextFeature.");
    undef $ds; # does not really delete the underlying dataset
    my $ds2 = $f->Layer->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "Feature keeps its parent dataset alive (1/2)");
}

{
    my $ds = Geo::OGR::Driver('Memory')->Create();
    my $ds_str = "$ds";
    my $l = $ds->CreateLayer(Name => 'layer', Fields => [{Name => 'test', Type => 'Integer'}]);
    $l->InsertFeature({test => 13});
    my $f = $l->GetFeature(0);
    ok($f->{test} == 13, "Got back the right feature with GetFeature.");
    undef $ds; # does not really delete the underlying dataset
    my $ds2 = $f->Layer->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "Feature keeps its parent dataset alive (2/2)");
}

# Geometry -> Feature

{
    my $ds = Geo::OGR::Driver('Memory')->Create();
    my $ds_str = "$ds";
    my $l = $ds->CreateLayer(Name => 'layer', 
                             Fields => [{Name => 'test', Type => 'Integer'}, 
                                        {Name => 'geom', Type => 'Point'}]);
    $l->InsertFeature({test => 13, geom => {WKT => 'POINT (10 20)'}});
    my $g = $l->GetFeature(0)->Geometry('geom');
    ok($g->AsText eq 'POINT (10 20)', "Got back the right geometry.");
    undef $ds; # does not really delete the underlying dataset
    undef $l;
    my $ds2 = $g->Feature->Layer->Dataset;
    my $ds2_str = "$ds2";
    ok($ds2_str eq $ds_str, "Geometry keeps its parent dataset alive");
}
