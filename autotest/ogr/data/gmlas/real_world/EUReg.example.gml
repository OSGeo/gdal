<?xml version="1.0" ?>
<gml:FeatureCollection xmlns:net="http://inspire.ec.europa.eu/schemas/net/4.0"
    xmlns:sc="http://www.interactive-instruments.de/ShapeChange/AppInfo"
    xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:gco="http://www.isotc211.org/2005/gco"    
    xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:ad="http://inspire.ec.europa.eu/schemas/ad/4.0"
    xmlns:base2="http://inspire.ec.europa.eu/schemas/base2/2.0"
    xmlns:pf="http://inspire.ec.europa.eu/schemas/pf/4.0"
    xmlns:act-core="http://inspire.ec.europa.eu/schemas/act-core/4.0"   
    xmlns:base="http://inspire.ec.europa.eu/schemas/base/3.3"
    xmlns:gn="http://inspire.ec.europa.eu/schemas/gn/4.0"
    xmlns:EUReg="http://dd.eionet.europa.eu/euregistryonindustrialsites"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gsr="http://www.isotc211.org/2005/gsr" xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:gss="http://www.isotc211.org/2005/gss"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    gml:id="_8291565a-3002-4532-8918-96218bdb93c9"
    xsi:schemaLocation="http://dd.eionet.europa.eu/euregistryonindustrialsites 
    http://dd.eionet.europa.eu/schemas/euregistryonindustrialsites/EUReg.xsd">
   
    <gml:featureMember>
        <EUReg:ProductionSite gml:id="_123456789.SITE">
            <pf:inspireId>
                <base:Identifier>
                    <base:localId>123456789.SITE</base:localId>
                    <base:namespace>ES.CAED</base:namespace>
                </base:Identifier>
            </pf:inspireId>
            <pf:status xsi:nil="true"/>
            <EUReg:siteName>
                <EUReg:FeatureName>
                    <EUReg:nameOfFeature>EXAMPLE SITE 1</EUReg:nameOfFeature>
                </EUReg:FeatureName>
            </EUReg:siteName>
            <EUReg:location>
                <gml:Point gml:id="_av427559-abb7-4632-af1f-764e317723gf"
                    srsName="urn:ogc:def:crs:EPSG::4258" srsDimension="2">
                    <gml:pos>41.991925 2.104334</gml:pos>
                </gml:Point>
            </EUReg:location>
            <EUReg:reportData xlink:href="#ES.RD.2017"/>
        </EUReg:ProductionSite>
    </gml:featureMember>
    
    <gml:featureMember>
        <EUReg:ProductionFacility gml:id="_000000002.FACILITY">
            <act-core:inspireId>
                <base:Identifier>
                    <base:localId>000000002.FACILITY</base:localId>
                    <base:namespace>ES.CAED</base:namespace>
                </base:Identifier>
            </act-core:inspireId>
            <act-core:geometry>
                <gml:Point gml:id="_cd427459-cfb7-4612-af1f-764e317723eb"
                    srsName="urn:ogc:def:crs:EPSG::4258" srsDimension="2">
                    <gml:pos>41.991932 2.104331</gml:pos>
                </gml:Point>
            </act-core:geometry>
            <act-core:function>
                <act-core:Function>
                    <act-core:activity
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/NACEValue/35.11"/>
                </act-core:Function>
            </act-core:function>
            <act-core:validFrom xsi:nil="true"/>
            <act-core:beginLifespanVersion xsi:nil="true"/>
            <pf:status>
                <pf:StatusType>
                    <pf:statusType
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/ConditionOfFacilityValue/functional"/>
                    <pf:validFrom xsi:nil="true"/>
                </pf:StatusType>
            </pf:status>
            <pf:hostingSite xlink:href="#_123456789.SITE"/>
            <pf:groupedInstallation xlink:href="#_010101011.INSTALLATION"/>
            <pf:groupedInstallation xlink:href="#_010101012.INSTALLATION"/>
            <EUReg:facilityName>
                <EUReg:FeatureName>
                    <EUReg:nameOfFeature>EXAMPLE FACILITY 1</EUReg:nameOfFeature>
                </EUReg:FeatureName>
            </EUReg:facilityName>
            <EUReg:competentAuthorityEPRTR>
                <EUReg:CompetentAuthority>
                    <EUReg:organisationName>MINISTRY FOR PRTR</EUReg:organisationName>
                    <EUReg:individualName>Mr John Wayne</EUReg:individualName>
                    <EUReg:electronicMailAddress>john.wayne@fake.com</EUReg:electronicMailAddress>
                    <EUReg:address>
                        <EUReg:AddressDetails>
                            <EUReg:streetName>Ministry Street</EUReg:streetName>
                            <EUReg:buildingNumber>20</EUReg:buildingNumber>
                            <EUReg:city>MADRID</EUReg:city>
                            <EUReg:postalCode>EX1 000</EUReg:postalCode>                            
                        </EUReg:AddressDetails>
                    </EUReg:address>
                    <EUReg:telephoneNo>+34 91 000 000</EUReg:telephoneNo>
                    <EUReg:faxNo>34 91 000 001</EUReg:faxNo>
                </EUReg:CompetentAuthority>
            </EUReg:competentAuthorityEPRTR>
            <EUReg:parentCompany>
                <EUReg:ParentCompanyDetails>
                    <EUReg:parentCompanyName>OWNER LIMITED</EUReg:parentCompanyName>
                    <EUReg:parentCompanyURL>http://www.fakeurl.fake</EUReg:parentCompanyURL>
                </EUReg:ParentCompanyDetails>
            </EUReg:parentCompany>
            <EUReg:EPRTRAnnexIActivity>
                <EUReg:EPRTRAnnexIActivityType>
                    <EUReg:mainActivity
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/EPRTRAnnexIActivityValue/1(c)"/>
                    <EUReg:otherActivity
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/EPRTRAnnexIActivityValue/5(b)"/> 
                </EUReg:EPRTRAnnexIActivityType>
            </EUReg:EPRTRAnnexIActivity>
            <EUReg:remarks>This is a simple example of how a facility would be reported to the EU
                Registry on Industrial Sites</EUReg:remarks>
            <EUReg:dateOfStartOfOperation>1982-01-01+01:00</EUReg:dateOfStartOfOperation>
            <EUReg:address>
                <EUReg:AddressDetails>
                    <EUReg:streetName>Administrative Street</EUReg:streetName>
                    <EUReg:buildingNumber>20</EUReg:buildingNumber>
                    <EUReg:city>BARCELONA</EUReg:city>
                    <EUReg:postalCode>EX2 000</EUReg:postalCode>
                    <EUReg:confidentialityReason
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/ReasonValue/Article4(2)(a)/"
                    />
                </EUReg:AddressDetails>
            </EUReg:address>
        </EUReg:ProductionFacility>
    </gml:featureMember>
    
    <gml:featureMember>
        <EUReg:ProductionInstallation gml:id="_010101011.INSTALLATION">
            <pf:inspireId>
                <base:Identifier>
                    <base:localId>010101011.INSTALLATION</base:localId>
                    <base:namespace>ES.CAED</base:namespace>
                </base:Identifier>
            </pf:inspireId>
            <pf:pointGeometry>
                <gml:Point gml:id="_1a18948e-ebda-4e56-8e6e-9adc3d848547"
                    srsName="urn:ogc:def:crs:EPSG::4258" srsDimension="2">
                    <gml:pos>41.991931 2.10433</gml:pos>
                </gml:Point>
            </pf:pointGeometry>
            <pf:status>
                <pf:StatusType>
                    <pf:statusType
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/ConditionOfFacilityValue/functional"/>
                    <pf:validFrom xsi:nil="true"/>
                </pf:StatusType>
            </pf:status>
            <pf:type/>
            <pf:groupedInstallationPart xlink:href="#_987654321.PART"/>
            <EUReg:installationName>
                <EUReg:FeatureName>
                    <EUReg:nameOfFeature>EXAMPLE INSTALLATION 1</EUReg:nameOfFeature>
                </EUReg:FeatureName>
            </EUReg:installationName>
            <EUReg:baselineReportPreparedIndicator>true</EUReg:baselineReportPreparedIndicator>
            <EUReg:BATDerogationIndicator>false</EUReg:BATDerogationIndicator>
            <EUReg:competentAuthorityPermits>
                <EUReg:CompetentAuthority>
                    <EUReg:organisationName>MINISTRY FOR IPPC</EUReg:organisationName>
                    <EUReg:individualName>MR TOM RICHARDSON</EUReg:individualName>
                    <EUReg:electronicMailAddress>tom.richardson@fake.com</EUReg:electronicMailAddress>
                    <EUReg:address>
                        <EUReg:AddressDetails>
                            <EUReg:streetName>Ministry Street</EUReg:streetName>
                            <EUReg:buildingNumber>10</EUReg:buildingNumber>
                            <EUReg:city>MADRID</EUReg:city>
                            <EUReg:postalCode>EX3 000</EUReg:postalCode>
                        </EUReg:AddressDetails>
                    </EUReg:address>
                    <EUReg:telephoneNo>+34 91 001 000</EUReg:telephoneNo>
                    <EUReg:faxNo>+ 34 91 001 001</EUReg:faxNo>
                </EUReg:CompetentAuthority>
            </EUReg:competentAuthorityPermits>
            <EUReg:competentAuthorityInspections>
                <EUReg:CompetentAuthority>
                    <EUReg:organisationName>MINISTRY FOR IPPC – Compliance
                        Department</EUReg:organisationName>
                    <EUReg:individualName>MRS KEANE WALTER</EUReg:individualName>
                    <EUReg:electronicMailAddress>keane.walter@fake.com</EUReg:electronicMailAddress>
                    <EUReg:address>
                        <EUReg:AddressDetails>
                            <EUReg:streetName>Ministry Street</EUReg:streetName>
                            <EUReg:buildingNumber>10</EUReg:buildingNumber>
                            <EUReg:city>MADRID</EUReg:city>
                            <EUReg:postalCode> EX4 000</EUReg:postalCode>
                        </EUReg:AddressDetails>
                    </EUReg:address>
                    <EUReg:telephoneNo>+34 91 002 000</EUReg:telephoneNo>
                    <EUReg:faxNo>+ 34 91 002 001</EUReg:faxNo>
                </EUReg:CompetentAuthority>
            </EUReg:competentAuthorityInspections>
            <EUReg:inspections>2</EUReg:inspections>
            <EUReg:ETSIdentifier>ETS.5463.fake</EUReg:ETSIdentifier>
            <EUReg:eSPIRSIdentifier>Seveso1235.ES.fake</EUReg:eSPIRSIdentifier>
            <EUReg:IEDAnnexIActivity>
                <EUReg:IEDAnnexIActivityType>
                    <EUReg:mainActivity
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/IEDAnnexIActivityValue/1.1"
                    />
                </EUReg:IEDAnnexIActivityType>
            </EUReg:IEDAnnexIActivity>
            <EUReg:permit>
                <EUReg:PermitDetails>
                    <EUReg:permitGranted>true</EUReg:permitGranted>
                    <EUReg:permitReconsidered>false</EUReg:permitReconsidered>
                    <EUReg:permitUpdated>false</EUReg:permitUpdated>
                    <EUReg:dateOfGranting>1956-01-01+01:00</EUReg:dateOfGranting>
                </EUReg:PermitDetails>
            </EUReg:permit>
            <EUReg:otherRelevantChapters
                xlink:href="http://dd.eionet.europa.eu/vocabularyconcept/euregistryonindustrialsites/RelevantChapterValue/ChapterIII"/>
            <EUReg:dateOfStartOfOperation>1956-01-01+01:00</EUReg:dateOfStartOfOperation>
        </EUReg:ProductionInstallation>
    </gml:featureMember>
    
    <gml:featureMember>
        <EUReg:ProductionInstallation gml:id="_010101012.INSTALLATION">
            <pf:inspireId>
                <base:Identifier>
                    <base:localId>010101012.INSTALLATION</base:localId>
                    <base:namespace>ES.CAED</base:namespace>
                </base:Identifier>
            </pf:inspireId>
            <pf:pointGeometry>
                <gml:Point gml:id="_e7c8d123-f757-4adb-a4b3-cd7e18d87437"
                    srsName="urn:ogc:def:crs:EPSG::4258" srsDimension="2">
                    <gml:pos>41.991929 2.104327</gml:pos>
                </gml:Point>
            </pf:pointGeometry>
            <pf:status>
                <pf:StatusType>
                    <pf:statusType
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/ConditionOfFacilityValue/disused"/>
                    <pf:validFrom xsi:nil="true"/>
                </pf:StatusType>
            </pf:status>
            <pf:type/>
            <pf:groupedInstallationPart xlink:href="#_987654322.PART"/>
            <EUReg:installationName>
                <EUReg:FeatureName>
                    <EUReg:nameOfFeature>EXAMPLE INSTALLATION 2</EUReg:nameOfFeature>
                </EUReg:FeatureName>
            </EUReg:installationName>
            <EUReg:baselineReportPreparedIndicator>true</EUReg:baselineReportPreparedIndicator>
            <EUReg:BATDerogationIndicator>true</EUReg:BATDerogationIndicator>
            <EUReg:competentAuthorityPermits>
                <EUReg:CompetentAuthority>
                    <EUReg:organisationName>MINISTRY FOR IPPC</EUReg:organisationName>
                    <EUReg:individualName>MR TOM RICHARDSON</EUReg:individualName>
                    <EUReg:electronicMailAddress>tom.richardson@fake.com</EUReg:electronicMailAddress>
                    <EUReg:address>
                        <EUReg:AddressDetails>
                            <EUReg:streetName>Ministry Street</EUReg:streetName>
                            <EUReg:buildingNumber>10</EUReg:buildingNumber>
                            <EUReg:city>MADRID</EUReg:city>
                            <EUReg:postalCode>EX5 000</EUReg:postalCode>
                        </EUReg:AddressDetails>
                    </EUReg:address>
                    <EUReg:telephoneNo>+34 91 001 000</EUReg:telephoneNo>
                    <EUReg:faxNo>+ 34 91 001 001</EUReg:faxNo>
                </EUReg:CompetentAuthority>
            </EUReg:competentAuthorityPermits>
            <EUReg:competentAuthorityInspections>
                <EUReg:CompetentAuthority>
                    <EUReg:organisationName>MINISTRY FOR IPPC – Compliance
                        Department</EUReg:organisationName>
                    <EUReg:individualName>MRS KEANE WALTER</EUReg:individualName>
                    <EUReg:electronicMailAddress>keane.walter@fake.com</EUReg:electronicMailAddress>
                    <EUReg:address>
                        <EUReg:AddressDetails>
                            <EUReg:streetName>Ministry Street</EUReg:streetName>
                            <EUReg:buildingNumber>10</EUReg:buildingNumber>
                            <EUReg:city>MADRID</EUReg:city>
                            <EUReg:postalCode>EX6 000</EUReg:postalCode>
                        </EUReg:AddressDetails>
                    </EUReg:address>
                    <EUReg:telephoneNo>+34 91 002 000</EUReg:telephoneNo>
                    <EUReg:faxNo>+ 34 91 002 001</EUReg:faxNo>
                </EUReg:CompetentAuthority>
            </EUReg:competentAuthorityInspections>
            <EUReg:inspections>0</EUReg:inspections>
            <EUReg:ETSIdentifier>ETS.54622.fake</EUReg:ETSIdentifier>
            <EUReg:eSPIRSIdentifier>Seveso1235.ES.fake</EUReg:eSPIRSIdentifier>
            <EUReg:IEDAnnexIActivity>
                <EUReg:IEDAnnexIActivityType>
                    <EUReg:mainActivity
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/IEDAnnexIActivityValue/5.2(a)"
                    />
                </EUReg:IEDAnnexIActivityType>
            </EUReg:IEDAnnexIActivity>
            <EUReg:permit>
                <EUReg:PermitDetails>
                    <EUReg:permitGranted>true</EUReg:permitGranted>
                    <EUReg:permitReconsidered>false</EUReg:permitReconsidered>
                    <EUReg:permitUpdated>false</EUReg:permitUpdated>
                    <EUReg:dateOfGranting>1982-01-01+01:00</EUReg:dateOfGranting>
                </EUReg:PermitDetails>
            </EUReg:permit>
            <EUReg:otherRelevantChapters
                xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/RelevantChapterValue/ChapterIV"/>
            <EUReg:dateOfStartOfOperation>1982-01-01+01:00</EUReg:dateOfStartOfOperation>
        </EUReg:ProductionInstallation>
    </gml:featureMember>
    
    <gml:featureMember>
        <EUReg:ProductionInstallationPart gml:id="_987654321.PART">
            <pf:inspireId>
                <base:Identifier>
                    <base:localId>987654321.PART</base:localId>
                    <base:namespace>ES.CAED</base:namespace>
                </base:Identifier>
            </pf:inspireId>
            <pf:pointGeometry>
                <gml:Point gml:id="_9fea4901-71be-4b0e-8669-11ba2e075511"
                    srsName="urn:ogc:def:crs:EPSG::4258" srsDimension="2">
                    <gml:pos>41.991929 2.104327</gml:pos>
                </gml:Point>
            </pf:pointGeometry>
            <pf:status>
                <pf:StatusType>
                    <pf:statusType
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/ConditionOfFacilityValue/functional"/>
                    <pf:validFrom xsi:nil="true"/>
                </pf:StatusType>
            </pf:status>
            <pf:type/>
            <pf:technique xsi:nil="true"/>
            <EUReg:installationPartName>
                <EUReg:FeatureName>
                    <EUReg:nameOfFeature>Example large combustion plant</EUReg:nameOfFeature>
                </EUReg:FeatureName>
            </EUReg:installationPartName>
            <EUReg:plantType
                xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/PlantTypeValue/LCP"/>
            <EUReg:totalRatedThermalInput>125.0</EUReg:totalRatedThermalInput>
            <EUReg:dateOfStartOfOperation>1956-01-01+01:00</EUReg:dateOfStartOfOperation>
        </EUReg:ProductionInstallationPart>
    </gml:featureMember>
    
    <gml:featureMember>
        <EUReg:ProductionInstallationPart gml:id="_987654322.PART">
            <pf:inspireId>
                <base:Identifier>
                    <base:localId>987654322.PART</base:localId>
                    <base:namespace>ES.CAED</base:namespace>
                </base:Identifier>
            </pf:inspireId>
            <pf:pointGeometry>
                <gml:Point gml:id="_d487ed0f-e9e7-4275-825c-367d4fb6b4e1"
                    srsName="urn:ogc:def:crs:EPSG::4258" srsDimension="2">
                    <gml:pos>41.991931 2.10433</gml:pos>
                </gml:Point>
            </pf:pointGeometry>
            <pf:status>
                <pf:StatusType>
                    <pf:statusType
                        xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/ConditionOfFacilityValue/disused"/>
                    <pf:validFrom xsi:nil="true"/>
                </pf:StatusType>
            </pf:status>
            <pf:type/>
            <pf:technique xsi:nil="true"/>
            <EUReg:installationPartName>
                <EUReg:FeatureName>
                    <EUReg:nameOfFeature>Example waste incinerator</EUReg:nameOfFeature>
                </EUReg:FeatureName>
            </EUReg:installationPartName>
            <EUReg:plantType
                xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/PlantTypeValue/WI"/>
            <EUReg:nominalCapacity>
                <EUReg:CapacityWasteIncinerationType>
                    <EUReg:totalNominalCapacityAnyWasteType>80.0</EUReg:totalNominalCapacityAnyWasteType>
                    <EUReg:permittedCapacityHazardous>0.0</EUReg:permittedCapacityHazardous>
                </EUReg:CapacityWasteIncinerationType>
            </EUReg:nominalCapacity>           
            <EUReg:dateOfStartOfOperation>1982-01-01+01:00</EUReg:dateOfStartOfOperation>
        </EUReg:ProductionInstallationPart>
    </gml:featureMember>
    
    <gml:featureMember>
        <EUReg:ReportData gml:id="ES.RD.2017">
            <EUReg:reportingYear>2017</EUReg:reportingYear>
            <EUReg:countryId
                xlink:href="http://dd.eionet.europa.eu/vocabulary/euregistryonindustrialsites/CountryCodeValue/ES"
            />
        </EUReg:ReportData>
    </gml:featureMember>
</gml:FeatureCollection>
