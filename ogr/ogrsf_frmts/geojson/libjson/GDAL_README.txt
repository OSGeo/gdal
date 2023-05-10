Upgrading internal libjson is a non trivial exercice as we have a number of
patches.
Look at
git diff 5fee60ed3e64794f0a58bd7d4e3f6a072acd5fcf..HEAD ogr/ogrsf_frmts/geojson/libjson/
for changes that have been applied on top of libjson-c 0.15

A point of caution is related to libjson using uselocale()/setlocale() to force
the C locale to be able to use strtod().
On Windows, the related macros to signal the availability of those functions
are not set (at least currently), so we replace strtod() by CPLStrtod().
