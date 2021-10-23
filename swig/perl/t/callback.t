use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More tests => 2;
BEGIN { use_ok('Geo::GDAL') };

{
    package Output;
    use strict;
    use warnings;
    our @output;
    sub new {
        return bless {}, 'Output';
    }
    sub write {
        my $line = shift;
        push @output, $line;
    }
    sub close {
        push @output, "end";
    }
}

{   # create a small layer and copy it to vsistdout with redirection
    my $l = Geo::OGR::Driver('Memory')->Create()->CreateLayer(GeometryType => 'None');
    $l->CreateField(Name => 'value', Type => 'Integer');
    $l->CreateField(Name => 'geom', GeometryType => 'Point');
    $l->InsertFeature([12,{wkt=>'POINT(1 1)'}]);

    my $output = Output->new;
    my $l2 = Geo::OGR::Driver('GeoJSON')->Create($output)->CopyLayer($l, '');

    $output = join '', @Output::output;
    $output =~ s/\n//g;

    ok($output eq 
       '{"type": "FeatureCollection",'.
       '"features": '.
       '[{ "type": "Feature", "id": 0, "properties": '.
       '{ "value": 12 }, "geometry": { "type": "Point", '.
       '"coordinates": [ 1.0, 1.0 ] } }]}end', "Redirect stdout to write/close methods.");
}
