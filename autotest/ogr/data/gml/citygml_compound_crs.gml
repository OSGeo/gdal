<core:CityModel xmlns:grp="http://www.opengis.net/citygml/cityobjectgroup/2.0" xmlns:core="http://www.opengis.net/citygml/2.0" xmlns:pbase="http://www.opengis.net/citygml/profiles/base/2.0" xmlns:smil20lang="http://www.w3.org/2001/SMIL20/Language" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:smil20="http://www.w3.org/2001/SMIL20/" xmlns:bldg="http://www.opengis.net/citygml/building/2.0" xmlns:xAL="urn:oasis:names:tc:ciq:xsdschema:xAL:2.0" xmlns:uro="http://www.kantei.go.jp/jp/singi/tiiki/toshisaisei/itoshisaisei/iur/uro/1.4" xmlns:luse="http://www.opengis.net/citygml/landuse/2.0" xmlns:app="http://www.opengis.net/citygml/appearance/2.0" xmlns:gen="http://www.opengis.net/citygml/generics/2.0" xmlns:dem="http://www.opengis.net/citygml/relief/2.0" xmlns:tex="http://www.opengis.net/citygml/texturedsurface/2.0" xmlns:tun="http://www.opengis.net/citygml/tunnel/2.0" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:sch="http://www.ascc.net/xml/schematron" xmlns:veg="http://www.opengis.net/citygml/vegetation/2.0" xmlns:frn="http://www.opengis.net/citygml/cityfurniture/2.0" xmlns:gml="http://www.opengis.net/gml" xmlns:tran="http://www.opengis.net/citygml/transportation/2.0" xmlns:wtr="http://www.opengis.net/citygml/waterbody/2.0" xmlns:brid="http://www.opengis.net/citygml/bridge/2.0" xsi:schemaLocation="http://www.kantei.go.jp/jp/singi/tiiki/toshisaisei/itoshisaisei/iur/uro/1.4 http://www.kantei.go.jp/jp/singi/tiiki/toshisaisei/itoshisaisei/iur/schemas/uro/1.4/urbanObject.xsd http://www.opengis.net/citygml/2.0 http://schemas.opengis.net/citygml/2.0/cityGMLBase.xsd http://www.opengis.net/citygml/landuse/2.0 http://schemas.opengis.net/citygml/landuse/2.0/landUse.xsd http://www.opengis.net/citygml/building/2.0 http://schemas.opengis.net/citygml/building/2.0/building.xsd http://www.opengis.net/citygml/transportation/2.0 http://schemas.opengis.net/citygml/transportation/2.0/transportation.xsd http://www.opengis.net/citygml/generics/2.0 http://schemas.opengis.net/citygml/generics/2.0/generics.xsd http://www.opengis.net/citygml/cityobjectgroup/2.0 http://schemas.opengis.net/citygml/cityobjectgroup/2.0/cityObjectGroup.xsd http://www.opengis.net/gml http://schemas.opengis.net/gml/3.1.1/base/gml.xsd http://www.opengis.net/citygml/appearance/2.0 http://schemas.opengis.net/citygml/appearance/2.0/appearance.xsd">
    <gml:description>Test data for GML driver. This data is part of Project PLATEAU 53394654_bldg_6697_op2.gml of plateau-tokyo23ku-citygml-2020/533946_2.7z.</gml:description>
    <gml:boundedBy>
        <gml:Envelope srsName="http://www.opengis.net/def/crs/EPSG/0/6697" srsDimension="3">
            <gml:lowerCorner>35.70819416724163 139.79979831321313 -0.974</gml:lowerCorner>
            <gml:upperCorner>35.71682557957052 139.8126483718091 100.486</gml:upperCorner>
        </gml:Envelope>
    </gml:boundedBy>
    <core:cityObjectMember>
        <bldg:Building gml:id="BLD_26758af8-c58c-4b0e-9a7b-b824269a5f59">
            <bldg:lod0RoofEdge>
                <gml:MultiSurface>
                    <gml:surfaceMember>
                        <gml:Polygon>
                            <gml:exterior>
                                <gml:LinearRing>
                                    <gml:posList>35.70921304132792 139.81248493871712 12.08 35.709254640636615 139.8124814223092 12.08 35.70925174840172 139.8124361114021 12.08 35.70921129565019 139.812439721473 12.08 35.70916107222451 139.8124442027463 12.08 35.709164144653336 139.81248907149094 12.08 35.70921304132792 139.81248493871712 12.08</gml:posList>
                                </gml:LinearRing>
                            </gml:exterior>
                        </gml:Polygon>
                    </gml:surfaceMember>
                </gml:MultiSurface>
            </bldg:lod0RoofEdge>
            <bldg:lod1Solid>
                <gml:Solid>
                    <gml:exterior>
                        <gml:CompositeSurface>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.70921304132792 139.81248493871712 0.15 35.709164144653336 139.81248907149094 0.15 35.70916107222451 139.8124442027463 0.15 35.70921129565019 139.812439721473 0.15 35.70925174840172 139.8124361114021 0.15 35.709254640636615 139.8124814223092 0.15 35.70921304132792 139.81248493871712 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.70921304132792 139.81248493871712 0.15 35.709254640636615 139.8124814223092 0.15 35.709254640636615 139.8124814223092 12.08 35.70921304132792 139.81248493871712 12.08 35.70921304132792 139.81248493871712 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.709254640636615 139.8124814223092 0.15 35.70925174840172 139.8124361114021 0.15 35.70925174840172 139.8124361114021 12.08 35.709254640636615 139.8124814223092 12.08 35.709254640636615 139.8124814223092 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.70925174840172 139.8124361114021 0.15 35.70921129565019 139.812439721473 0.15 35.70921129565019 139.812439721473 12.08 35.70925174840172 139.8124361114021 12.08 35.70925174840172 139.8124361114021 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.70921129565019 139.812439721473 0.15 35.70916107222451 139.8124442027463 0.15 35.70916107222451 139.8124442027463 12.08 35.70921129565019 139.812439721473 12.08 35.70921129565019 139.812439721473 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.70916107222451 139.8124442027463 0.15 35.709164144653336 139.81248907149094 0.15 35.709164144653336 139.81248907149094 12.08 35.70916107222451 139.8124442027463 12.08 35.70916107222451 139.8124442027463 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.709164144653336 139.81248907149094 0.15 35.70921304132792 139.81248493871712 0.15 35.70921304132792 139.81248493871712 12.08 35.709164144653336 139.81248907149094 12.08 35.709164144653336 139.81248907149094 0.15</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                            <gml:surfaceMember>
                                <gml:Polygon>
                                    <gml:exterior>
                                        <gml:LinearRing>
                                            <gml:posList>35.70921304132792 139.81248493871712 12.08 35.709254640636615 139.8124814223092 12.08 35.70925174840172 139.8124361114021 12.08 35.70921129565019 139.812439721473 12.08 35.70916107222451 139.8124442027463 12.08 35.709164144653336 139.81248907149094 12.08 35.70921304132792 139.81248493871712 12.08</gml:posList>
                                        </gml:LinearRing>
                                    </gml:exterior>
                                </gml:Polygon>
                            </gml:surfaceMember>
                        </gml:CompositeSurface>
                    </gml:exterior>
                </gml:Solid>
            </bldg:lod1Solid>
        </bldg:Building>
    </core:cityObjectMember>
</core:CityModel>
