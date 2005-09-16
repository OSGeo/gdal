BEGIN { $| = 1; }
END {print "not ok 1\n" unless $loaded;}
use gdal;
use ogr;
$loaded = 1;

gdal::AllRegister();

ogr::RegisterAll();

$dataset = gdal::Open('../../data/gdalicon.png', gdal::GA_ReadOnly);

if ($dataset) {
    print $dataset,' ',ref($dataset),"\n";
    $driver = $dataset->GetDriver;
    print $driver->swig_ShortName_get,"\n";
    print $dataset->swig_RasterXSize_get,"\n";
    print $dataset->swig_RasterYSize_get,"\n";
    print $dataset->GetProjection,"\n";
}

$datasource = ogr::Open('../../data/stateplane.csv');

if ($datasource) {
    print $datasource->swig_name_get,"\n";
}
