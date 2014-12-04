<?xml version="1.0" encoding="utf-8"?>
<ogr:FeatureCollection xmlns:gml="http://www.opengis.net/gml" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ogr="http://www.gdal.org/ogr" xsi:schemaLocation="http://www.gdal.org/ogr choicepolygonmultipolygon.xsd">
  <gml:featureMember>
    <ogr:test>
      <ogr:attr>1</ogr:attr>
      <gml:polygonProperty>
        <gml:Polygon xmlns:gml="http://www.opengis.net/gml">
          <gml:outerBoundaryIs>
            <gml:LinearRing>
              <gml:coordinates>0,0 0,1 1,1 1,0 0,0</gml:coordinates>
            </gml:LinearRing>
          </gml:outerBoundaryIs>
        </gml:Polygon>
      </gml:polygonProperty>
    </ogr:test>
  </gml:featureMember>
  <gml:featureMember>
    <ogr:test>
      <ogr:attr>2</ogr:attr>
      <gml:multiPolygonProperty>
        <gml:MultiPolygon xmlns:gml="http://www.opengis.net/gml">
          <gml:polygonMember>
            <gml:Polygon>
              <gml:outerBoundaryIs>
                <gml:LinearRing>
                  <gml:coordinates>0,0 0,1 1,1 1,0 0,0</gml:coordinates>
                </gml:LinearRing>
              </gml:outerBoundaryIs>
            </gml:Polygon>
          </gml:polygonMember>
          <gml:polygonMember>
            <gml:Polygon>
              <gml:outerBoundaryIs>
                <gml:LinearRing>
                  <gml:coordinates>10,0 10,1 11,1 11,0 10,0</gml:coordinates>
                </gml:LinearRing>
              </gml:outerBoundaryIs>
            </gml:Polygon>
          </gml:polygonMember>
        </gml:MultiPolygon>
      </gml:multiPolygonProperty>
    </ogr:test>
  </gml:featureMember>
</ogr:FeatureCollection>
