# Example 1
from osgeo import gdal

gdal.UseExceptions()

stac_url = "https://planetarycomputer.microsoft.com/api/stac/v1/search"
bbox = "-100,40,-99,41"
datetime = "2019-01-01T00:00:00Z%2F.."
collections = "naip"
url = f"{stac_url}?collections={collections}&bbox={bbox}&datetime={datetime}"

ds = gdal.OpenEx(
    url, allowed_drivers=["STACIT"]
)  # force the STACIT driver or it opens as GeoJSON

print(gdal.Info(ds, format="json"))

# Example 2

subdatasets = ds.GetSubDatasets()
# get the first subdataset properties
sds_properties = subdatasets[0]
subdataset_url = sds_properties[0]  # a tuple of URL and description
subdataset = gdal.OpenEx(subdataset_url, allowed_drivers=["STACIT"])

print(gdal.Info(subdataset, format="json"))
