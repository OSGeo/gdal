use strict;
use warnings;
use bytes;
use v5.10;
use Test::More qw(no_plan);
BEGIN { use_ok('Geo::GDAL') };

# input => ISO, old
my @wkt = (
    'POINT EMPTY' => ['POINT EMPTY', 'POINT EMPTY'],
    'POINT Z EMPTY' => ['POINT Z EMPTY', 'POINT EMPTY'],
    'POINT M EMPTY' => ['POINT M EMPTY', 'POINT EMPTY'],
    'POINT ZM EMPTY' => ['POINT ZM EMPTY', 'POINT EMPTY'],
    'POINT (EMPTY)' => ['POINT EMPTY', 'POINT EMPTY'],
    'POINT EMPTY (1 2)' => ['error', 'error'],
    'POINT (1)' => ['error', 'error'],
    'POINT (1 2)' => ['POINT (1 2)', 'POINT (1 2)'],
    'POINT (1 2 3)' => ['POINT Z (1 2 3)', 'POINT (1 2 3)'],
    'POINT (1 2 3 4)' => ['POINT ZM (1 2 3 4)', 'POINT (1 2 3)'],
    'POINT (1 2 3 4 5)' => ['error', 'error'],
    'POINT Z (1 2 3)' => ['POINT Z (1 2 3)', 'POINT (1 2 3)'],
    'POINT Z (1 2)' => ['POINT Z (1 2 0)', 'POINT (1 2 0)'],
    'POINT Z (1 2 3 4)' => ['POINT Z (1 2 3)', 'POINT (1 2 3)'], # backwards compatibility
    'POINT M (1 2 3)' => ['POINT M (1 2 3)', 'POINT (1 2)'],
    'POINT M (1 2)' => ['POINT M (1 2 0)', 'POINT (1 2)'],
    'POINT M (1 2 3 4)' => ['error', 'error'],
    'POINT ZM (1 2 3 4)' => ['POINT ZM (1 2 3 4)', 'POINT (1 2 3)'],
    'POINT ZM (1 2 3)' => ['POINT ZM (1 2 3 0)', 'POINT (1 2 3)'],
    'POINT ZM (1 2 3 4 5)' => ['error', 'error'],

    'LINESTRING EMPTY' => ['LINESTRING EMPTY', 'LINESTRING EMPTY'],
    'LINESTRING Z EMPTY' => ['LINESTRING Z EMPTY', 'LINESTRING EMPTY'],
    'LINESTRING M EMPTY' => ['LINESTRING M EMPTY', 'LINESTRING EMPTY'],
    'LINESTRING ZM EMPTY' => ['LINESTRING ZM EMPTY', 'LINESTRING EMPTY'],
    'LINESTRING (EMPTY)' => ['LINESTRING EMPTY', 'LINESTRING EMPTY'],
    'LINESTRING EMPTY (1 2, 3 4)' => ['error', 'error'],
    'LINESTRING (1, 3)' => ['error', 'error'],
    'LINESTRING (1 2, 3 4)' => ['LINESTRING (1 2,3 4)', 'LINESTRING (1 2,3 4)'],
    'LINESTRING (1 2, 3 4 5)' => ['LINESTRING Z (1 2 0,3 4 5)', 'LINESTRING (1 2 0,3 4 5)'],
    'LINESTRING (1 2 3, 3 4 5)' => ['LINESTRING Z (1 2 3,3 4 5)', 'LINESTRING (1 2 3,3 4 5)'],
    'LINESTRING (1 2 3 4, 5 6 7 8)' => ['LINESTRING ZM (1 2 3 4,5 6 7 8)', 'LINESTRING (1 2 3,5 6 7)'],
    'LINESTRING (1 2 3 4 5, 6 7 8 9)' => ['error', 'error'],
    'LINESTRING Z (1 2 3, 6 7 8)' => ['LINESTRING Z (1 2 3,6 7 8)', 'LINESTRING (1 2 3,6 7 8)'],
    'LINESTRING Z (1 2, 6 7)' => ['LINESTRING Z (1 2 0,6 7 0)', 'LINESTRING (1 2 0,6 7 0)'],
    'LINESTRING Z (1 2 3 4, 6 7 8 9)' => ['LINESTRING Z (1 2 3,6 7 8)', 'LINESTRING (1 2 3,6 7 8)'], # backwards compatibility
    'LINESTRING M (1 2 3, 6 7 8)' => ['LINESTRING M (1 2 3,6 7 8)', 'LINESTRING (1 2,6 7)'],
    'LINESTRING M (1 2, 6 7)' => ['LINESTRING M (1 2 0,6 7 0)', 'LINESTRING (1 2,6 7)'],
    'LINESTRING M (1 2 3 4, 6 7 8 9)' => ['LINESTRING ZM (1 2 3 4,6 7 8 9)', 'LINESTRING (1 2 3,6 7 8)'],
    'LINESTRING ZM (1 2 3 4, 6 7 8 9)' => ['LINESTRING ZM (1 2 3 4,6 7 8 9)', 'LINESTRING (1 2 3,6 7 8)'],
    'LINESTRING ZM (1 2 3, 6 7 8)' => ['LINESTRING ZM (1 2 3 0,6 7 8 0)', 'LINESTRING (1 2 3,6 7 8)'],
    'LINESTRING ZM (1 2 3 4 5, 6 7 8 9 10)' => ['error', 'error'],

    'POLYGON (EMPTY)' => ['POLYGON EMPTY', 'POLYGON EMPTY'],
    'POLYGON Z (EMPTY)' => ['POLYGON Z EMPTY', 'POLYGON EMPTY'],
    'POLYGON M (EMPTY)' => ['POLYGON M EMPTY', 'POLYGON EMPTY'],
    'POLYGON ZM (EMPTY)' => ['POLYGON ZM EMPTY', 'POLYGON EMPTY'],

    'MULTIPOINT EMPTY' => ['MULTIPOINT EMPTY', 'MULTIPOINT EMPTY'],
    'MULTILINESTRING EMPTY' => ['MULTILINESTRING EMPTY', 'MULTILINESTRING EMPTY'],
    'MULTIPOLYGON EMPTY' => ['MULTIPOLYGON EMPTY', 'MULTIPOLYGON EMPTY'],
    'GEOMETRYCOLLECTION EMPTY' => ['GEOMETRYCOLLECTION EMPTY', 'GEOMETRYCOLLECTION EMPTY'],
    'CIRCULARSTRING EMPTY' => ['CIRCULARSTRING EMPTY', 'CIRCULARSTRING EMPTY'],
    'COMPOUNDCURVE EMPTY' => ['COMPOUNDCURVE EMPTY', 'COMPOUNDCURVE EMPTY'],
    'CURVEPOLYGON EMPTY' => ['CURVEPOLYGON EMPTY', 'CURVEPOLYGON EMPTY'],
    'MULTICURVE EMPTY' => ['MULTICURVE EMPTY', 'MULTICURVE EMPTY'],
    'MULTISURFACE EMPTY' => ['MULTISURFACE EMPTY', 'MULTISURFACE EMPTY'],

    'MULTIPOINT Z EMPTY' => ['MULTIPOINT Z EMPTY', 'MULTIPOINT EMPTY'],
    'MULTILINESTRING Z EMPTY' => ['MULTILINESTRING Z EMPTY', 'MULTILINESTRING EMPTY'],
    'MULTIPOLYGON Z EMPTY' => ['MULTIPOLYGON Z EMPTY', 'MULTIPOLYGON EMPTY'],
    'GEOMETRYCOLLECTION Z EMPTY' => ['GEOMETRYCOLLECTION Z EMPTY', 'GEOMETRYCOLLECTION EMPTY'],
    'CIRCULARSTRING Z EMPTY' => ['CIRCULARSTRING Z EMPTY', 'CIRCULARSTRING Z EMPTY'],
    'COMPOUNDCURVE Z EMPTY' => ['COMPOUNDCURVE Z EMPTY', 'COMPOUNDCURVE Z EMPTY'],
    'CURVEPOLYGON Z EMPTY' => ['CURVEPOLYGON Z EMPTY', 'CURVEPOLYGON Z EMPTY'],
    'MULTICURVE Z EMPTY' => ['MULTICURVE Z EMPTY', 'MULTICURVE Z EMPTY'],
    'MULTISURFACE Z EMPTY' => ['MULTISURFACE Z EMPTY', 'MULTISURFACE Z EMPTY'],

    'MULTIPOINT M EMPTY' => ['MULTIPOINT M EMPTY', 'MULTIPOINT EMPTY'],
    'MULTILINESTRING M EMPTY' => ['MULTILINESTRING M EMPTY', 'MULTILINESTRING EMPTY'],
    'MULTIPOLYGON M EMPTY' => ['MULTIPOLYGON M EMPTY', 'MULTIPOLYGON EMPTY'],
    'GEOMETRYCOLLECTION M EMPTY' => ['GEOMETRYCOLLECTION M EMPTY', 'GEOMETRYCOLLECTION EMPTY'],
    'CIRCULARSTRING M EMPTY' => ['CIRCULARSTRING M EMPTY', 'CIRCULARSTRING M EMPTY'],
    'COMPOUNDCURVE M EMPTY' => ['COMPOUNDCURVE M EMPTY', 'COMPOUNDCURVE M EMPTY'],
    'CURVEPOLYGON M EMPTY' => ['CURVEPOLYGON M EMPTY', 'CURVEPOLYGON M EMPTY'],
    'MULTICURVE M EMPTY' => ['MULTICURVE M EMPTY', 'MULTICURVE M EMPTY'],
    'MULTISURFACE M EMPTY' => ['MULTISURFACE M EMPTY', 'MULTISURFACE M EMPTY'],

    'MULTIPOINT ZM EMPTY' => ['MULTIPOINT ZM EMPTY', 'MULTIPOINT EMPTY'],
    'MULTILINESTRING ZM EMPTY' => ['MULTILINESTRING ZM EMPTY', 'MULTILINESTRING EMPTY'],
    'MULTIPOLYGON ZM EMPTY' => ['MULTIPOLYGON ZM EMPTY', 'MULTIPOLYGON EMPTY'],
    'GEOMETRYCOLLECTION ZM EMPTY' => ['GEOMETRYCOLLECTION ZM EMPTY', 'GEOMETRYCOLLECTION EMPTY'],
    'CIRCULARSTRING ZM EMPTY' => ['CIRCULARSTRING ZM EMPTY', 'CIRCULARSTRING ZM EMPTY'],
    'COMPOUNDCURVE ZM EMPTY' => ['COMPOUNDCURVE ZM EMPTY', 'COMPOUNDCURVE ZM EMPTY'],
    'CURVEPOLYGON ZM EMPTY' => ['CURVEPOLYGON ZM EMPTY', 'CURVEPOLYGON ZM EMPTY'],
    'MULTICURVE ZM EMPTY' => ['MULTICURVE ZM EMPTY', 'MULTICURVE ZM EMPTY'],
    'MULTISURFACE ZM EMPTY' => ['MULTISURFACE ZM EMPTY', 'MULTISURFACE ZM EMPTY'],

    'POLYGON ((1 2, 3 4, 5 6),(1 2, 2 2, 0 0))' => 
    ['POLYGON ((1 2,3 4,5 6),(1 2,2 2,0 0))', 'POLYGON ((1 2,3 4,5 6),(1 2,2 2,0 0))'],

    'POLYGON Z ((1 2, 3 4, 5 6 7),(1 2, 2 2, 0 0))' => 
    ['POLYGON Z ((1 2 0,3 4 0,5 6 7),(1 2 0,2 2 0,0 0 0))', 'POLYGON ((1 2 0,3 4 0,5 6 7),(1 2 0,2 2 0,0 0 0))'],

    'POLYGON M ((1 2, 3 4, 5 6),(1 2, 2 2, 0 0))' => 
    ['POLYGON M ((1 2 0,3 4 0,5 6 0),(1 2 0,2 2 0,0 0 0))', 'POLYGON ((1 2,3 4,5 6),(1 2,2 2,0 0))'],

    'POLYGON ZM ((1 2, 3 4, 5 6),(1 2, 2 2, 0 0))' => 
    ['POLYGON ZM ((1 2 0 0,3 4 0 0,5 6 0 0),(1 2 0 0,2 2 0 0,0 0 0 0))', 'POLYGON ((1 2 0,3 4 0,5 6 0),(1 2 0,2 2 0,0 0 0))'],

    'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2,50 60 7)))' => 
    ['MULTIPOLYGON Z (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0,50 60 7)))', 
     'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0,50 60 7)))'],

    'MULTIPOINT ((1 2 3 4),(5 6 7))' =>
    [ 'MULTIPOINT ZM ((1 2 3 4),(5 6 7 0))',
      'MULTIPOINT (1 2 3,5 6 7)' ],

    'MULTILINESTRING ((1 2 3 4,5 6 7),(5 6 7,1 2 3 4))' =>
    [ 'MULTILINESTRING ZM ((1 2 3 4,5 6 7 0),(5 6 7 0,1 2 3 4))',
      'MULTILINESTRING ((1 2 3,5 6 7),(5 6 7,1 2 3))' ],

    "POINT (1 2)" => 
    [ 'POINT (1 2)', 'POINT (1 2)'],

    "MULTIPOINT (1 2,3 4)" => 
    [ 'MULTIPOINT ((1 2),(3 4))', 'MULTIPOINT (1 2,3 4)'],

    "LINESTRING (1 2,3 4)" => 
    [ 'LINESTRING (1 2,3 4)', 'LINESTRING (1 2,3 4)'],

    "MULTILINESTRING ((1 2,3 4))" => 
    [ 'MULTILINESTRING ((1 2,3 4))', 'MULTILINESTRING ((1 2,3 4))'],

    "MULTILINESTRING ((1 2,3 4),(5 6,7 8))" => 
    [ 'MULTILINESTRING ((1 2,3 4),(5 6,7 8))', 'MULTILINESTRING ((1 2,3 4),(5 6,7 8))'],

    "POLYGON ((0 0,0 1,1 1,1 0,0 0))" => 
    [ 'POLYGON ((0 0,0 1,1 1,1 0,0 0))', 'POLYGON ((0 0,0 1,1 1,1 0,0 0))'],

    "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))" => 
    [ 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))', 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))'],

    "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))" => 
    [ 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))', 
      'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))'],

    "MULTIPOINT (1 2 -10,3 4 -20)" => 
    [ 'MULTIPOINT Z ((1 2 -10),(3 4 -20))', 'MULTIPOINT (1 2 -10,3 4 -20)'],

    "LINESTRING (1 2 -10,3 4 -20)" => 
    [ 'LINESTRING Z (1 2 -10,3 4 -20)', 'LINESTRING (1 2 -10,3 4 -20)'],

    "MULTILINESTRING ((1 2 -10,3 4 -20))" => 
    [ 'MULTILINESTRING Z ((1 2 -10,3 4 -20))', 'MULTILINESTRING ((1 2 -10,3 4 -20))'],

    "MULTILINESTRING ((1 2 -10,3 4 -20))" => 
    [ 'MULTILINESTRING Z ((1 2 -10,3 4 -20))', 'MULTILINESTRING ((1 2 -10,3 4 -20))'],

    "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))" => 
    [ 'POLYGON Z ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))', 'POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))'],

    "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" => 
    [ 'MULTIPOLYGON Z (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))', 'MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))'],

    "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" => 
    [ 'MULTIPOLYGON Z (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))', 'MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))'],

    "MULTIPOLYGON (((0 0 0,0 1 0,1 0 0,0 0 0)),((0 1 0,1 0 0,1 1 0,0 1 0)),((10 0 0,10 1 0,11 0 0,10 0 0)),((10 0 0,11 0 0,10 -1 0,10 0 0)),((5 0 0,5 1 0,6 0 0,5 0 0)),((15 0 0,15 1 0,16 0 0,15 0 0)),((100 0 0,100 1 0,101 1 0,101 0 0,100 0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0)))" => 
    [ 'MULTIPOLYGON Z (((0 0 0,0 1 0,1 0 0,0 0 0)),((0 1 0,1 0 0,1 1 0,0 1 0)),((10 0 0,10 1 0,11 0 0,10 0 0)),((10 0 0,11 0 0,10 -1 0,10 0 0)),((5 0 0,5 1 0,6 0 0,5 0 0)),((15 0 0,15 1 0,16 0 0,15 0 0)),((100 0 0,100 1 0,101 1 0,101 0 0,100 0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0)))', 
      'MULTIPOLYGON (((0 0 0,0 1 0,1 0 0,0 0 0)),((0 1 0,1 0 0,1 1 0,0 1 0)),((10 0 0,10 1 0,11 0 0,10 0 0)),((10 0 0,11 0 0,10 -1 0,10 0 0)),((5 0 0,5 1 0,6 0 0,5 0 0)),((15 0 0,15 1 0,16 0 0,15 0 0)),((100 0 0,100 1 0,101 1 0,101 0 0,100 0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0)))'],

    'CIRCULARSTRING (0 1,2 3,4 5)' => 
    [ 'CIRCULARSTRING (0 1,2 3,4 5)', 'CIRCULARSTRING (0 1,2 3,4 5)'],

    'CIRCULARSTRING Z (0 1 2,4 5 6,7 8 9)' => 
    [ 'CIRCULARSTRING Z (0 1 2,4 5 6,7 8 9)', 'CIRCULARSTRING Z (0 1 2,4 5 6,7 8 9)'],

    'COMPOUNDCURVE ((0 1,2 3,4 5))' => 
    [ 'COMPOUNDCURVE ((0 1,2 3,4 5))', 'COMPOUNDCURVE ((0 1,2 3,4 5))'],

    'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9))' => 
    [ 'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9))', 'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9))'],

    'COMPOUNDCURVE ((0 1,2 3,4 5),CIRCULARSTRING (4 5,6 7,8 9))' => 
    [ 'COMPOUNDCURVE ((0 1,2 3,4 5),CIRCULARSTRING (4 5,6 7,8 9))', 'COMPOUNDCURVE ((0 1,2 3,4 5),CIRCULARSTRING (4 5,6 7,8 9))'],

    'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9),CIRCULARSTRING Z (7 8 9,10 11 12,13 14 15))' => 
    [ 'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9),CIRCULARSTRING Z (7 8 9,10 11 12,13 14 15))', 'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9),CIRCULARSTRING Z (7 8 9,10 11 12,13 14 15))'],

    'CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))' => 
    [ 'CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))', 'CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))'],

    'CURVEPOLYGON Z ((0 0 2,0 1 3,1 1 4,1 0 5,0 0 2))' => 
    [ 'CURVEPOLYGON Z ((0 0 2,0 1 3,1 1 4,1 0 5,0 0 2))', 'CURVEPOLYGON Z ((0 0 2,0 1 3,1 1 4,1 0 5,0 0 2))'],

    'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))' => 
    [ 'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))', 'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))'],

    'CURVEPOLYGON Z (COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 2,1 0 3,0 0 2)))' => 
    [ 'CURVEPOLYGON Z (COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 2,1 0 3,0 0 2)))', 
      'CURVEPOLYGON Z (COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 2,1 0 3,0 0 2)))'],

    'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1))' => 
    [ 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1))', 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1))'],

    'MULTICURVE Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1),(0 0 1,1 1 1))' => 
    [ 'MULTICURVE Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1),(0 0 1,1 1 1))', 
      'MULTICURVE Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1),(0 0 1,1 1 1))'],

    'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))' => 
    [ 'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))', 
      'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))'],

    'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))' => 
    [ 'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))', 
      'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))'],

    'MULTISURFACE Z (((0 0 1,0 10 1,10 10 1,10 0 1,0 0 1)),CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1)))' => 
    [ 'MULTISURFACE Z (((0 0 1,0 10 1,10 10 1,10 0 1,0 0 1)),CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1)))', 
      'MULTISURFACE Z (((0 0 1,0 10 1,10 10 1,10 0 1,0 0 1)),CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1)))'],

    'GEOMETRYCOLLECTION (CIRCULARSTRING (0 1,2 3,4 5),COMPOUNDCURVE ((0 1,2 3,4 5)),CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0)),MULTICURVE ((0 0,1 1)),MULTISURFACE (((0 0,0 10,10 10,10 0,0 0))))' => 
    [ 'GEOMETRYCOLLECTION (CIRCULARSTRING (0 1,2 3,4 5),COMPOUNDCURVE ((0 1,2 3,4 5)),CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0)),MULTICURVE ((0 0,1 1)),MULTISURFACE (((0 0,0 10,10 10,10 0,0 0))))', 
      'GEOMETRYCOLLECTION (CIRCULARSTRING (0 1,2 3,4 5),COMPOUNDCURVE ((0 1,2 3,4 5)),CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0)),MULTICURVE ((0 0,1 1)),MULTISURFACE (((0 0,0 10,10 10,10 0,0 0))))'],

    'GEOMETRYCOLLECTION (POINT (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'GEOMETRYCOLLECTION (POINT Z (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'GEOMETRYCOLLECTION (POINT M (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION M (POINT M (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2))' ],

    'GEOMETRYCOLLECTION (POINT ZM (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 0))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'GEOMETRYCOLLECTION Z (POINT (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'GEOMETRYCOLLECTION Z (POINT M (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 0 3))', 'GEOMETRYCOLLECTION (POINT (1 2 0))' ],
    
    'GEOMETRYCOLLECTION Z (POINT ZM (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 0))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],
    
    'GEOMETRYCOLLECTION M (POINT (1 2 3))' => ['error', 'error'],
    
    'GEOMETRYCOLLECTION M (POINT Z (1 2 3))' => ['error', 'error'],

    'GEOMETRYCOLLECTION M (POINT M (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION M (POINT M (1 2 3))', 'GEOMETRYCOLLECTION (POINT (1 2))' ],
    
    'GEOMETRYCOLLECTION M (POINT ZM (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 0))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],
    
    'GEOMETRYCOLLECTION ZM (POINT (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 0))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],
    
    'GEOMETRYCOLLECTION ZM (POINT Z (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 0))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'GEOMETRYCOLLECTION ZM (POINT M (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 0 3))', 'GEOMETRYCOLLECTION (POINT (1 2 0))' ],

    'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3))' => 
    [ 'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 0))', 'GEOMETRYCOLLECTION (POINT (1 2 3))' ],

    'MULTICURVE (LINESTRING (1 2 3))' => 
    [ 'MULTICURVE Z ((1 2 3))', 'MULTICURVE Z ((1 2 3))' ],

    'MULTICURVE (LINESTRING Z (1 2 3))' => 
    [ 'MULTICURVE Z ((1 2 3))', 'MULTICURVE Z ((1 2 3))' ],

    'MULTICURVE (LINESTRING M (1 2 3))' => 
    [ 'MULTICURVE M ((1 2 3))', 'MULTICURVE M ((1 2 3))' ],

    'MULTICURVE (LINESTRING ZM (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 3 0))', 'MULTICURVE ZM ((1 2 3 0))' ],

    'MULTICURVE Z (LINESTRING (1 2 3))' => 
    [ 'MULTICURVE Z ((1 2 3))', 'MULTICURVE Z ((1 2 3))' ],

    'MULTICURVE Z (LINESTRING Z (1 2 3))' => 
    [ 'MULTICURVE Z ((1 2 3))', 'MULTICURVE Z ((1 2 3))' ],

    'MULTICURVE Z (LINESTRING M (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 0 3))', 'MULTICURVE ZM ((1 2 0 3))' ],

    'MULTICURVE Z (LINESTRING ZM (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 3 0))', 'MULTICURVE ZM ((1 2 3 0))' ],
    
    'MULTICURVE M (LINESTRING (1 2 3))' => ['error', 'error'],
    
    'MULTICURVE M (LINESTRING Z (1 2 3))' => ['error', 'error'],
    
    'MULTICURVE M (LINESTRING M (1 2 3))' => 
    [ 'MULTICURVE M ((1 2 3))', 'MULTICURVE M ((1 2 3))' ],
    
    'MULTICURVE M (LINESTRING ZM (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 3 0))', 'MULTICURVE ZM ((1 2 3 0))' ],
    
    'MULTICURVE ZM (LINESTRING (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 3 0))', 'MULTICURVE ZM ((1 2 3 0))' ],
    
    'MULTICURVE ZM (LINESTRING Z (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 3 0))', 'MULTICURVE ZM ((1 2 3 0))' ],
    
    'MULTICURVE ZM (LINESTRING M (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 0 3))', 'MULTICURVE ZM ((1 2 0 3))' ],
    
    'MULTICURVE ZM (LINESTRING ZM (1 2 3))' => 
    [ 'MULTICURVE ZM ((1 2 3 0))', 'MULTICURVE ZM ((1 2 3 0))' ],

    'MULTICURVE (CIRCULARSTRING (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))', 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' ],
    
    'MULTICURVE (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))', 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' ],
    
    'MULTICURVE (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE M (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))', 'MULTICURVE M (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))' ],
    
    'MULTICURVE (CIRCULARSTRING ZM (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))' ],
    
    'MULTICURVE Z (CIRCULARSTRING (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))', 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' ],
    
    'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))', 'MULTICURVE Z (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' ],
    
    'MULTICURVE Z (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 0 3,1 2 0 3,1 2 0 3))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 0 3,1 2 0 3,1 2 0 3))' ],
    
    'MULTICURVE Z (CIRCULARSTRING ZM (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))' ],
    
    'MULTICURVE M (CIRCULARSTRING (1 2 3,1 2 3,1 2 3))' => ['error', 'error'],
    
    'MULTICURVE M (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' => ['error', 'error'],
    
    'MULTICURVE M (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE M (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))', 'MULTICURVE M (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))' ],

    'MULTICURVE M (CIRCULARSTRING ZM (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))' ],

    'MULTICURVE ZM (CIRCULARSTRING (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))' ],

    'MULTICURVE ZM (CIRCULARSTRING Z (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))' ],

'MULTICURVE ZM (CIRCULARSTRING M (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 0 3,1 2 0 3,1 2 0 3))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 0 3,1 2 0 3,1 2 0 3))' ],

'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3,1 2 3,1 2 3))' => 
    [ 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))', 'MULTICURVE ZM (CIRCULARSTRING ZM (1 2 3 0,1 2 3 0,1 2 3 0))' ],

    'POINT (1 2)' => 
    [ 'POINT (1 2)', 'POINT (1 2)' ],
'POINT (1 2 3)' => 
    [ 'POINT Z (1 2 3)', 'POINT (1 2 3)' ],
'POINT (1 2 3 4)' => 
    [ 'POINT ZM (1 2 3 4)', 'POINT (1 2 3)' ],
'POINT Z (1 2)' => 
    [ 'POINT Z (1 2 0)', 'POINT (1 2 0)' ],
'POINT Z (1 2 3)' => 
    [ 'POINT Z (1 2 3)', 'POINT (1 2 3)' ],
'POINT Z (1 2 3 4)' => 
    [ 'POINT Z (1 2 3)', 'POINT (1 2 3)' ],
'POINT M (1 2)' => 
    [ 'POINT M (1 2 0)', 'POINT (1 2)' ],
'POINT M (1 2 3)' => 
    [ 'POINT M (1 2 3)', 'POINT (1 2)' ],
'POINT M (1 2 3 4)' => 
    [ 'POINT ZM (1 2 3 4)', 'POINT (1 2 3)' ],
'POINT ZM (1 2)' => 
    [ 'POINT ZM (1 2 0 0)', 'POINT (1 2 0)' ],
'POINT ZM (1 2 3)' => 
    [ 'POINT ZM (1 2 3 0)', 'POINT (1 2 3)' ],
'POINT ZM (1 2 3 4)' => 
    [ 'POINT ZM (1 2 3 4)', 'POINT (1 2 3)' ],
'LINESTRING (1 2,1 2)' => 
    [ 'LINESTRING (1 2,1 2)', 'LINESTRING (1 2,1 2)' ],
'LINESTRING (1 2 3,1 2 3)' => 
    [ 'LINESTRING Z (1 2 3,1 2 3)', 'LINESTRING (1 2 3,1 2 3)' ],
'LINESTRING (1 2 3 4,1 2 3 4)' => 
    [ 'LINESTRING ZM (1 2 3 4,1 2 3 4)', 'LINESTRING (1 2 3,1 2 3)' ],
'LINESTRING Z (1 2,1 2)' => 
    [ 'LINESTRING Z (1 2 0,1 2 0)', 'LINESTRING (1 2 0,1 2 0)' ],
'LINESTRING Z (1 2 3,1 2 3)' => 
    [ 'LINESTRING Z (1 2 3,1 2 3)', 'LINESTRING (1 2 3,1 2 3)' ],
'LINESTRING Z (1 2 3 4,1 2 3 4)' => 
    [ 'LINESTRING Z (1 2 3,1 2 3)', 'LINESTRING (1 2 3,1 2 3)' ],
'LINESTRING M (1 2,1 2)' => 
    [ 'LINESTRING M (1 2 0,1 2 0)', 'LINESTRING (1 2,1 2)' ],
'LINESTRING M (1 2 3,1 2 3)' => 
    [ 'LINESTRING M (1 2 3,1 2 3)', 'LINESTRING (1 2,1 2)' ],
'LINESTRING M (1 2 3 4,1 2 3 4)' => 
    [ 'LINESTRING ZM (1 2 3 4,1 2 3 4)', 'LINESTRING (1 2 3,1 2 3)' ],
'LINESTRING ZM (1 2,1 2)' => 
    [ 'LINESTRING ZM (1 2 0 0,1 2 0 0)', 'LINESTRING (1 2 0,1 2 0)' ],
'LINESTRING ZM (1 2 3,1 2 3)' => 
    [ 'LINESTRING ZM (1 2 3 0,1 2 3 0)', 'LINESTRING (1 2 3,1 2 3)' ],
'LINESTRING ZM (1 2 3 4,1 2 3 4)' => 
    [ 'LINESTRING ZM (1 2 3 4,1 2 3 4)', 'LINESTRING (1 2 3,1 2 3)' ],


    );

my %wkt = @wkt;

for (my $i = 0; $i < @wkt; $i+=2) {
    my $key = $wkt[$i];
    #say "$key"; next;
    #say "$key, $wkt{$key}"; next;
    #say "WKT not ok $key $wkt{$key}";
    my ($type) = $key =~ /^(\w+)/;
    if ($type =~ /^(\w)(\w+)/) {
        $type = $1 . lc($2);
    }
    $type =~ s/string/String/;
    my $wkt = $key;

    my $format = 'ISO';
    for my $out (@{$wkt{$key}}) {
        my $e = '';
        if ($format eq 'ISO') {
            if ($out =~ /ZM/) {
                $e = ' ZM';
            } elsif ($out =~ /Z/) {
                $e = ' Z';
            } elsif ($out =~ / M /) {
                $e = ' M'
            }
        }
        my $p;
        eval {
            $p = Geo::OGR::Geometry->new(WKT => $wkt);
        };
        my $result;
        my $exp;
        if ($@) {
            $result = 'error';
            $exp = $out;
        } else {
            $result = $p->As(Format => $format.' WKT');
            $exp = $out;
        }
        ok($exp eq $result, "$key, $format WKT: expected: $exp, got: $result");

        if ($p) {
            my $p = Geo::OGR::Geometry->new(WKT => $key);
            my $exp = '';
            my $exp2 = '';
            my $x = $wkt{$key}->[0];
            if ($x =~ /ZM/) {
                $exp2 = $exp = 'ZM';
            } elsif ($x =~ /Z/) {
                $exp = 'Z';
                $exp2 = '25D';
            } elsif ($x =~ / M /) {
                $exp2 = $exp = 'M'
            }
            $exp2 = $type.$exp2;
            $exp = $type.$exp;
            my $result = $p->GeometryType;
            ok(lc($exp) eq lc($result) || lc($exp2) eq lc($result), "$key, $format Type: expected: $exp, got: $result");
            
            my $wkb = $p->As(Format => $format.' WKB', ByteOrder => 'NDR');
            my $len = bytes::length($wkb);
            my $t = bytes::substr($wkb,1,4);
            $t = unpack("l",$t);
            my $q = Geo::OGR::Geometry->new(WKB => $wkb);
            $exp = $p->As(Format => $format.' WKT');
            $result = $q->As(Format => $format.' WKT');

            if ($format eq '' and ($key =~ /^POINT (\w*) (|\()EMPTY(|\))/ or $key =~ /^POINT (|\()EMPTY(|\))/)) {
                if ($1 =~ /Z/) {
                    $exp = 'POINT (0 0 0)';
                } else {
                    $exp = 'POINT (0 0)';
                }
            }

            ok($exp eq $result, "$key, WKB: expected: $exp, got: $result ($len,$t)");
        }
        $format = '';
    }
}
