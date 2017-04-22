use strict;
use warnings;
use Scalar::Util 'blessed';
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

for my $datatype (qw/Byte Int16 Int32 UInt16 UInt32/) {
    my $band = Geo::GDAL::Driver('MEM')->Create(Type => $datatype, Width => 5, Height => 5)->Band;
    my $tile = $band->ReadTile;
    my $v = 0; # 3 of 0 to 7 and one 8
    my $c = 0;
    my %counts;
    for my $y (0..$#$tile) {
        for my $x (0..$#{$tile->[$y]}) {
            $tile->[$y][$x] = $v;
            $counts{$v} = $counts{$v} ? $counts{$v}+1 : 1;
            if (++$c > 2) {
                ++$v;
                $c = 0;
            }
        }
    }
    $band->WriteTile($tile);
    
    $c = $band->ClassCounts;
    is_deeply($c, \%counts, "$datatype: ClassCounts");

    eval {
        $band->ClassCounts(sub {return 1});
    };
    ok(!$@, "ClassCounts overload test $datatype: $@");
    
    $band->Reclassify({'*' => 10, 1 => 2, 2 => 3});
    $c = $band->ClassCounts;
    is_deeply($c, {2 => 3, 3 => 3, 10 => 19}, "Reclassify with default");
    
    $band->WriteTile($tile);
    $band->Reclassify({1 => 2, 2 => 3});
    $c = $band->ClassCounts;
    delete $counts{1};
    $counts{3} = 6;
    is_deeply($c, \%counts, "$datatype: Reclassify without default");
    
    $band->WriteTile($tile);
    $band->NoDataValue(5);
    $band->Reclassify({'*' => 0});
    $c = $band->ClassCounts;
    #for my $key (keys %$c) {
    #    print "$key $c->{$key}\n";
    #}
    is_deeply($c, {0 => 22, 5 => 3}, "Reclassify with default does not affect cells with NoData.");

}

for my $datatype (qw/Float32 Float64/) {
    my $band = Geo::GDAL::Driver('MEM')->Create(Type => $datatype, Width => 2, Height => 2)->Band;
    my $tile = $band->ReadTile;
    $tile->[0] = [2,3];
    $tile->[1] = [5,6];
    $band->WriteTile($tile);

    # 1   2   3
    #   3   5
    my $classifier = ['<', [5.0, [3.0, 1.0, 2.0], 3.0]];
    my $counts = $band->ClassCounts($classifier);
    #say STDERR $counts;
    my @counts;
    for my $key (sort {$a<=>$b} keys %$counts) {
        #say STDERR "$key => $counts->{$key}";
        push @counts, $key => $counts->{$key};
    }
    is_deeply(\@counts, [0=>1,1=>1,2=>2], "Class counts $datatype");
    $band->Reclassify($classifier);
    $tile = $band->ReadTile;
    #for my $y (0..$#$tile) {
    #    say STDERR "@{$tile->[$y]}";
    #}
    is_deeply($tile, [[1,2],[3,3]], "Reclassify $datatype");

    eval {
        $band->Reclassify($classifier, sub {return 1});
    };
    ok(!$@, "Reclassify overload test $datatype: $@");
}
    
my $band = Geo::GDAL::Driver('MEM')->Create(Type => 'CFloat32', Width => 5, Height => 5)->Band;
eval {
    my $c = $band->ClassCounts;
};
ok($@, "ClassCounts works only on integer bands.");

eval {
    $band->Reclassify({1 => 2, 2 => 3});
};
ok($@, "Reclassify works only on integer bands.");

eval {
    my $c = $band->ClassCounts(sub {return 0});
};
ok($@, "Terminating ClassCounts raises an error.");

eval {
    $band->Reclassify({1 => 2, 2 => 3}, sub {return 0});
};
ok($@, "Terminating Reclassify raises an error.");   
