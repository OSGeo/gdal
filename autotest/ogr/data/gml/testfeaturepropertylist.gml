<?xml version="1.0" encoding="UTF-8"?>
<ex1:FeatureCollection xmlns:ex1="http://example.org/ex1" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xlink="http://www.w3.org/1999/xlink">
  <gml:featureMember>
    <ex1:FeatureA gml:id="FA">
      <ex1:name>Feature A</ex1:name>      
      <ex1:roleInline>
        <ex1:FeatureB gml:id="FB1_1">
          <ex1:name>Feature B 1_1</ex1:name>
        </ex1:FeatureB>
      </ex1:roleInline>
      <ex1:roleInline>
        <ex1:FeatureB gml:id="FB1_2">
          <ex1:name>Feature B 1_2</ex1:name>
        </ex1:FeatureB>
      </ex1:roleInline>
      <ex1:roleByReference xlink:href="#FB2"/>
      <ex1:roleByReference xlink:href="http://example.com/Features#FBXXX"/>
    </ex1:FeatureA>
  </gml:featureMember>
  <gml:featureMember>
    <ex1:FeatureB gml:id="FB2">
      <ex1:name>Feature B 2</ex1:name>
    </ex1:FeatureB>
  </gml:featureMember>
</ex1:FeatureCollection>
