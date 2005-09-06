BEGIN { $| = 1; }
END {print "not ok 1\n" unless $loaded;}
use gdal;
use ogr;
$loaded = 1;

gdal::AllRegister();

ogr::RegisterAll();

$dataset = gdal::Open('/d/data/Corine/100x100/lceugr100_00_pct.tif', gdal::GA_ReadOnly);

if ($dataset) {
    print $dataset,' ',ref($dataset),"\n";
    $driver = $dataset->GetDriver;
    print $driver->swig_ShortName_get,"\n";
    print $dataset->swig_RasterXSize_get,"\n";
    print $dataset->swig_RasterYSize_get,"\n";
    print $dataset->GetProjection,"\n";
}

$datasource = ogr::Open('/d/data/misc/gma1geo.shp');

if ($datasource) {
    print $datasource->swig_name_get,"\n";
}
