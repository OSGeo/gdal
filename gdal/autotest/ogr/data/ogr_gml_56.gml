<?xml version="1.0" encoding="utf-8" ?>
<gdalautotest:FeatureCollection
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
     xsi:schemaLocation="http://gdal.org/gdalautotest http://non_existing.com/ogr_gml_56.xsd"
     xmlns:gdalautotest="http://gdal.org/gdalautotest"
     xmlns:gml="http://www.opengis.net/gml">
  <gml:boundedBy>
    <gml:Envelope srsName="EPSG:32630">
      <gml:lowerCorner>0 0</gml:lowerCorner>
      <gml:upperCorner>1 1</gml:upperCorner>
    </gml:Envelope>
  </gml:boundedBy>
  <gml:featureMember>
    <gdalautotest:mainFeature gml:id="mainFeature.0">
      <gdalautotest:geometryProperty><gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:coordinates>0 0,0 1,1 1,1 0,0 0</gml:coordinates></gml:LinearRing></gml:outerBoundaryIs></gml:Polygon></gdalautotest:geometryProperty>
      <gdalautotest:secondGeometryProperty><gml:Point><gml:coordinates>10 10</gml:coordinates></gml:Point></gdalautotest:secondGeometryProperty>
      <gdalautotest:subFeatureProperty>
        <gdalautotest:subFeature gml:id="subFeature.0">
          <gdalautotest:foo>bar</gdalautotest:foo>
          <gdalautotest:subFeatureRepeatedProperty>
            <gdalautotest:subFeatureRepeated gml:id="subFeatureRepeated.2">
              <gdalautotest:bar>baz</gdalautotest:bar>
            </gdalautotest:subFeatureRepeated>
          </gdalautotest:subFeatureRepeatedProperty>
        </gdalautotest:subFeature>
      </gdalautotest:subFeatureProperty>
      <gdalautotest:subFeatureRepeatedProperty>
        <gdalautotest:subFeatureRepeated gml:id="subFeatureRepeated.0">
        </gdalautotest:subFeatureRepeated>
      </gdalautotest:subFeatureRepeatedProperty>
      <gdalautotest:subFeatureRepeatedProperty>
        <gdalautotest:subFeatureRepeated gml:id="subFeatureRepeated.1">
        </gdalautotest:subFeatureRepeated>
      </gdalautotest:subFeatureRepeatedProperty>
    </gdalautotest:mainFeature>
  </gml:featureMember>
</gdalautotest:FeatureCollection>
