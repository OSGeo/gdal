from osgeo import gdal, ogr

gdal.UseExceptions()

ds = gdal.OpenEx("point", gdal.OF_VECTOR)

lyr = ds.GetLayerByName("point")

lyr.ResetReading()

for feat in lyr:

    feat_defn = lyr.GetLayerDefn()
    for i in range(feat_defn.GetFieldCount()):
        field_defn = feat_defn.GetFieldDefn(i)

        # Tests below can be simplified with just :
        # print feat.GetField(i)
        if (
            field_defn.GetType() == ogr.OFTInteger
            or field_defn.GetType() == ogr.OFTInteger64
        ):
            print("%d" % feat.GetFieldAsInteger64(i))
        elif field_defn.GetType() == ogr.OFTReal:
            print("%.3f" % feat.GetFieldAsDouble(i))
        elif field_defn.GetType() == ogr.OFTString:
            print("%s" % feat.GetFieldAsString(i))
        else:
            print("%s" % feat.GetFieldAsString(i))

    geom = feat.GetGeometryRef()
    if geom is not None and geom.GetGeometryType() == ogr.wkbPoint:
        print("%.3f, %.3f" % (geom.GetX(), geom.GetY()))
    else:
        print("no point geometry")

ds.Close()
