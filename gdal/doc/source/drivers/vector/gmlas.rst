.. _vector.gmlas:

GMLAS - Mapping examples
========================

This page gives a few examples of how XML constructs are mapped to OGR
layers and fields by the `GMLAS <drv_gmlas.html>`__ driver.

+-----------------+-----------------+-----------------+-----------------+
| Schema          | Document        | OGR layers      | Comments        |
+=================+=================+=================+=================+
| ::              | ::              | ::              | Element with    |
|                 |                 |                 | attributes and  |
|    <schema xmln |    <MyFeature i |    Layer name:  | sub-elements of |
| s="http://www.w | d="my_id" attr= | MyFeature       | simple type and |
| 3.org/2001/XMLS | "attr_value">   |    Geometry: No | maximum         |
| chema">         |        <name>my | ne              | cardinality of  |
|    <element nam | _name</name>    |    id: String ( | 1.              |
| e="MyFeature">  |    </MyFeature> | 0.0) NOT NULL   |                 |
|      <complexTy |                 |    attr: String |                 |
| pe>             |                 |  (0.0)          |                 |
|        <sequenc |                 |    name: String |                 |
| e>              |                 |  (0.0) NOT NULL |                 |
|            <ele |                 |    OGRFeature(M |                 |
| ment name="name |                 | yFeature):1     |                 |
| " type="string" |                 |      id (String |                 |
| />              |                 | ) = my_id       |                 |
|        </sequen |                 |      attr (Stri |                 |
| ce>             |                 | ng) = attr_valu |                 |
|        <attribu |                 | e               |                 |
| te name="id" ty |                 |      name (Stri |                 |
| pe="ID" use="re |                 | ng) = my_name   |                 |
| quired"/>       |                 |                 |                 |
|        <attribu |                 |                 |                 |
| te name="attr"  |                 |                 |                 |
| type="string"/> |                 |                 |                 |
|      </complexT |                 |                 |                 |
| ype>            |                 |                 |                 |
|    </element>   |                 |                 |                 |
|    </schema>    |                 |                 |                 |
+-----------------+-----------------+-----------------+-----------------+
| ::              | ::              | ::              | Case of array   |
|                 |                 |                 | and child layer |
|    <schema xmln |    <MyFeature i |    Layer name:  |                 |
| s="http://www.w | d="my_id">      | MyFeature       |                 |
| 3.org/2001/XMLS |        <str_arr |    Geometry: No |                 |
| chema">         | ay>first string | ne              |                 |
|    <element nam | </str_array>    |    id: String ( |                 |
| e="MyFeature">  |        <str_arr | 0.0) NOT NULL   |                 |
|      <complexTy | ay>second strin |    str_array: S |                 |
| pe>             | g</str_array>   | tringList (0.0) |                 |
|        <sequenc |        <dt_arra |  NOT NULL       |                 |
| e>              | y>2016-09-24T15 |    OGRFeature(M |                 |
|            <ele | :31:00Z</dt_arr | yFeature):1     |                 |
| ment name="str_ | ay>             |      id (String |                 |
| array" type="st |        <dt_arra | ) = my_id       |                 |
| ring"           | y>2016-09-24T15 |      str_array  |                 |
|                 | :32:00Z</dt_arr | (StringList) =  |                 |
|      maxOccurs= | ay>             |        (2:first |                 |
| "2"/>           |    </MyFeature> |  string,second  |                 |
|            <ele |                 | string)         |                 |
| ment name="dt_a |                 |                 |                 |
| rray" type="dat |                 |    Layer name:  |                 |
| eTime"          |                 | MyFeature_dt_ar |                 |
|                 |                 | ray             |                 |
|      maxOccurs= |                 |    Geometry: No |                 |
| "unbounded"/>   |                 | ne              |                 |
|        </sequen |                 |    ogr_pkid: St |                 |
| ce>             |                 | ring (0.0) NOT  |                 |
|        <attribu |                 | NULL            |                 |
| te name="id" ty |                 |    parent_id: S |                 |
| pe="ID" use="re |                 | tring (0.0) NOT |                 |
| quired"/>       |                 |  NULL           |                 |
|      </complexT |                 |    value: DateT |                 |
| ype>            |                 | ime (0.0)       |                 |
|    </element>   |                 |    OGRFeature(M |                 |
|    </schema>    |                 | yFeature_dt_arr |                 |
|                 |                 | ay):1           |                 |
|                 |                 |      ogr_pkid ( |                 |
|                 |                 | String) = my_id |                 |
|                 |                 | _dt_array_1     |                 |
|                 |                 |      parent_id  |                 |
|                 |                 | (String) = my_i |                 |
|                 |                 | d               |                 |
|                 |                 |      value (Dat |                 |
|                 |                 | eTime) = 2016/0 |                 |
|                 |                 | 9/24 15:31:00+0 |                 |
|                 |                 | 0               |                 |
|                 |                 |                 |                 |
|                 |                 |    OGRFeature(M |                 |
|                 |                 | yFeature_dt_arr |                 |
|                 |                 | ay):2           |                 |
|                 |                 |      ogr_pkid ( |                 |
|                 |                 | String) = my_id |                 |
|                 |                 | _dt_array_2     |                 |
|                 |                 |      parent_id  |                 |
|                 |                 | (String) = my_i |                 |
|                 |                 | d               |                 |
|                 |                 |      value (Dat |                 |
|                 |                 | eTime) = 2016/0 |                 |
|                 |                 | 9/24 15:32:00+0 |                 |
|                 |                 | 0               |                 |
+-----------------+-----------------+-----------------+-----------------+
| ::              | ::              | ::              | Case of nested  |
|                 |                 |                 | element, that   |
|    <schema xmln |    <MyFeature i |    Layer name:  | can be folded   |
| s="http://www.w | d="my_id">      | MyFeature       | into main       |
| 3.org/2001/XMLS |        <identif |    Geometry: No | layer. Use of   |
| chema">         | ier>            | ne              | an attribute on |
|    <element nam |            <nam |    id: String ( | a sub-element.  |
| e="MyFeature">  | e lang="en">my_ | 0.0) NOT NULL   |                 |
|      <complexTy | name</name>     |    identifier_n |                 |
| pe>             |            <nam | ame_lang: Strin |                 |
|        <sequenc | espace>baz</nam | g (0.0)         |                 |
| e>              | espace>         |    identifier_n |                 |
|          <eleme |        </identi | ame: String (0. |                 |
| nt name="identi | fier>           | 0)              |                 |
| fier">          |    </MyFeature> |    identifier_n |                 |
|            <com |                 | amespace: Strin |                 |
| plexType>       |                 | g (0.0)         |                 |
|              <s |                 |    OGRFeature(M |                 |
| equence>        |                 | yFeature):1     |                 |
|                 |                 |      id (String |                 |
| <element name=" |                 | ) = my_id       |                 |
| name">          |                 |      identifier |                 |
|                 |                 | _name_lang (Str |                 |
|   <complexType> |                 | ing) = en       |                 |
|                 |                 |      identifier |                 |
|     <simpleCont |                 | _name (String)  |                 |
| ent>            |                 | = my_name       |                 |
|                 |                 |      identifier |                 |
|       <extensio |                 | _namespace (Str |                 |
| n base="string" |                 | ing) = baz      |                 |
| >               |                 |                 |                 |
|                 |                 |                 |                 |
|         <attrib |                 |                 |                 |
| ute name="lang" |                 |                 |                 |
|                 |                 |                 |                 |
|                 |                 |                 |                 |
|     type="strin |                 |                 |                 |
| g"/>            |                 |                 |                 |
|                 |                 |                 |                 |
|       </extensi |                 |                 |                 |
| on>             |                 |                 |                 |
|                 |                 |                 |                 |
|     </simpleCon |                 |                 |                 |
| tent>           |                 |                 |                 |
|                 |                 |                 |                 |
|   </complexType |                 |                 |                 |
| >               |                 |                 |                 |
|                 |                 |                 |                 |
| </element>      |                 |                 |                 |
|                 |                 |                 |                 |
| <element name=" |                 |                 |                 |
| namespace" type |                 |                 |                 |
| ="string"       |                 |                 |                 |
|                 |                 |                 |                 |
|          minOcc |                 |                 |                 |
| urs="0"/>       |                 |                 |                 |
|              </ |                 |                 |                 |
| sequence>       |                 |                 |                 |
|            </co |                 |                 |                 |
| mplexType>      |                 |                 |                 |
|          </elem |                 |                 |                 |
| ent>            |                 |                 |                 |
|        </sequen |                 |                 |                 |
| ce>             |                 |                 |                 |
|        <attribu |                 |                 |                 |
| te name="id" ty |                 |                 |                 |
| pe="ID" use="re |                 |                 |                 |
| quired"/>       |                 |                 |                 |
|      </complexT |                 |                 |                 |
| ype>            |                 |                 |                 |
|    </element>   |                 |                 |                 |
|    </schema>    |                 |                 |                 |
+-----------------+-----------------+-----------------+-----------------+
| ::              | ::              | ::              | Case of a       |
|                 |                 |                 | common element  |
|    <schema xmln |    <FeatureColl |    Layer name:  | "name"          |
| s:myns="http:// | ection xmlns="h | name            | referenced by 2 |
| myns"           | ttp://myns">    |    OGRFeature(n | layers          |
|            targ |      <MyFeature | ame):1          | "MyFeature" and |
| etNamespace="ht |  id="my_id">    |      ogr_pkid ( | "MyFeature1".   |
| tp://myns"      |        <names>  | String) = _name | The links are   |
|            elem |            <nam | _1              | established     |
| entFormDefault= | e>              |      name (Stri | through the     |
| "qualified"     |              <n | ng) = name      | junction layers |
|            xmln | ame>name</name> |      lang (Stri | "MyFeature_name |
| s="http://www.w |              <l | ng) = en        | s_name_name"    |
| 3.org/2001/XMLS | ang>en</lang>   |                 | and             |
| chema">         |            </na |    OGRFeature(n | "MyFeature2_nam |
|                 | me>             | ame):2          | es_name_name".  |
|    <element nam |            <nam |      ogr_pkid ( |                 |
| e="AbstractFeat | e>              | String) = _name |                 |
| ure" abstract=" |              <n | _2              |                 |
| true"/>         | ame>nom</name>  |      name (Stri |                 |
|                 |              <l | ng) = nom       |                 |
|    <element nam | ang>fr</lang>   |      lang (Stri |                 |
| e="FeatureColle |            </na | ng) = fr        |                 |
| ction">         | me>             |                 |                 |
|      <complexTy |        </names> |    OGRFeature(n |                 |
| pe><sequence>   |      </MyFeatur | ame):3          |                 |
|          <eleme | e>              |      ogr_pkid ( |                 |
| nt ref="myns:Ab |      <MyFeature | String) = _name |                 |
| stractFeature"  | 2 id="my_id2">  | _3              |                 |
| maxOccurs="unbo |        <names>  |      name (Stri |                 |
| unded"/>        |            <nam | ng) = nom2      |                 |
|      </sequence | e>              |      lang (Stri |                 |
| ></complexType> |              <n | ng) = fr        |                 |
|    </element>   | ame>nom2</name> |                 |                 |
|                 |              <l |                 |                 |
|    <complexType | ang>fr</lang>   |    Layer name:  |                 |
|  name="namesTyp |            </na | MyFeature       |                 |
| e">             | me>             |    OGRFeature(M |                 |
|      <sequence> |        </names> | yFeature):1     |                 |
|        <element |      </MyFeatur |      id (String |                 |
|  ref="myns:name | e2>             | ) = my_id       |                 |
| " maxOccurs="un |    </FeatureCol |                 |                 |
| bounded"/>      | lection>        |    Layer name:  |                 |
|      </sequence |                 | MyFeature2      |                 |
| >               |                 |    OGRFeature(M |                 |
|    </complexTyp |                 | yFeature2):1    |                 |
| e>              |                 |      id (String |                 |
|                 |                 | ) = my_id2      |                 |
|    <element nam |                 |                 |                 |
| e="MyFeature" s |                 |                 |                 |
| ubstitutionGrou |                 |    Layer name:  |                 |
| p="myns:Abstrac |                 | MyFeature_names |                 |
| tFeature">      |                 | _name_name      |                 |
|      <complexTy |                 |    OGRFeature(M |                 |
| pe><sequence>   |                 | yFeature_names_ |                 |
|          <eleme |                 | name_name):1    |                 |
| nt name="names" |                 |      occurrence |                 |
|  type="myns:nam |                 |  (Integer) = 1  |                 |
| esType"/>       |                 |      parent_pki |                 |
|        </sequen |                 | d (String) = my |                 |
| ce>             |                 | _id             |                 |
|        <attribu |                 |      child_pkid |                 |
| te name="id" ty |                 |  (String) = _na |                 |
| pe="ID" use="re |                 | me_1            |                 |
| quired"/>       |                 |                 |                 |
|      </complexT |                 |    OGRFeature(M |                 |
| ype>            |                 | yFeature_names_ |                 |
|    </element>   |                 | name_name):2    |                 |
|                 |                 |      occurrence |                 |
|    <element nam |                 |  (Integer) = 2  |                 |
| e="MyFeature2"  |                 |      parent_pki |                 |
| substitutionGro |                 | d (String) = my |                 |
| up="myns:Abstra |                 | _id             |                 |
| ctFeature">     |                 |      child_pkid |                 |
|      <complexTy |                 |  (String) = _na |                 |
| pe><sequence>   |                 | me_2            |                 |
|          <eleme |                 |                 |                 |
| nt name="names" |                 |                 |                 |
|  type="myns:nam |                 |    Layer name:  |                 |
| esType"/>       |                 | MyFeature2_name |                 |
|        </sequen |                 | s_name_name     |                 |
| ce>             |                 |    OGRFeature(M |                 |
|        <attribu |                 | yFeature2_names |                 |
| te name="id" ty |                 | _name_name):1   |                 |
| pe="ID" use="re |                 |      occurrence |                 |
| quired"/>       |                 |  (Integer) = 1  |                 |
|      </complexT |                 |      parent_pki |                 |
| ype>            |                 | d (String) = my |                 |
|    </element>   |                 | _id2            |                 |
|                 |                 |      child_pkid |                 |
|    <element nam |                 |  (String) = _na |                 |
| e="name">       |                 | me_3            |                 |
|      <complexTy |                 |                 |                 |
| pe><sequence>   |                 |                 |                 |
|          <eleme |                 |                 |                 |
| nt name="name"  |                 |                 |                 |
| type="string"/> |                 |                 |                 |
|          <eleme |                 |                 |                 |
| nt name="lang"  |                 |                 |                 |
| type="string"/> |                 |                 |                 |
|      </sequence |                 |                 |                 |
| ></complexType> |                 |                 |                 |
|    </element>   |                 |                 |                 |
|                 |                 |                 |                 |
|    </schema>    |                 |                 |                 |
+-----------------+-----------------+-----------------+-----------------+

swe:DataArray
-------------

The following snippet

::

       <swe:DataArray>
           <swe:elementCount>
               <swe:Count>
                       <swe:value>2</swe:value>
               </swe:Count>
           </swe:elementCount>
           <swe:elementType name="Components">
               <swe:DataRecord>
                       <swe:field name="myTime">
                           <swe:Time definition="http://www.opengis.net/def/property/OGC/0/SamplingTime">
                                   <swe:uom xlink:href="http://www.opengis.net/def/uom/ISO-8601/0/Gregorian"/>
                           </swe:Time>
                       </swe:field>
                       <swe:field name="myCategory">
                           <swe:Category definition="http://dd.eionet.europa.eu/vocabulary/aq/observationverification"/>
                       </swe:field>
                           <swe:field name="myQuantity">
                           <swe:Quantity definition="http://dd.eionet.europa.eu/vocabulary/aq/primaryObservation/hour">
                                   <swe:uom xlink:href="http://dd.eionet.europa.eu/vocabulary/uom/concentration/ug.m-3"/>
                           </swe:Quantity>
                       </swe:field>
                       <swe:field name="myCount">
                           <swe:Count definition="http://"/>
                       </swe:field>
                           <swe:field name="myText">
                           <swe:Text definition="http://"/>
                       </swe:field>
                           <swe:field name="myBoolean">
                           <swe:Boolean definition="http://"/>
                       </swe:field>
               </swe:DataRecord>
           </swe:elementType>
           <swe:encoding>
                   <swe:TextEncoding decimalSeparator="." blockSeparator="@@" tokenSeparator=","/>
           </swe:encoding>
           <swe:values>2016-09-01T00:00:00+01:00,1,2.34,3,foo,true@@2017-09-01T00:00:00,2,3.45</swe:values>
       </swe:DataArray>

will receive a special processing to be mapped into a dedicated layer:

::


   Layer name: dataarray_1_components
   Geometry: None
   Feature Count: 2
   Layer SRS WKT:
   (unknown)
   parent_ogr_pkid: String (0.0) NOT NULL
   mytime: DateTime (0.0)
   mycategory: String (0.0)
   myquantity: Real (0.0)
   mycount: Integer (0.0)
   mytext: String (0.0)
   myboolean: Integer(Boolean) (0.0)
   OGRFeature(dataarray_1_components):1
     parent_ogr_pkid (String) = BAE8440FC4563A80D2AB1860A47AA0A3_DataArray_1
     mytime (DateTime) = 2016/09/01 00:00:00+01
     mycategory (String) = 1
     myquantity (Real) = 2.34
     mycount (Integer) = 3
     mytext (String) = foo
     myboolean (Integer(Boolean)) = 1

   OGRFeature(dataarray_1_components):2
     parent_ogr_pkid (String) = BAE8440FC4563A80D2AB1860A47AA0A3_DataArray_1
     mytime (DateTime) = 2017/09/01 00:00:00
     mycategory (String) = 2
     myquantity (Real) = 3.45


See Also
--------

-  `GMLAS <drv_gmlas.html>`__: main documentation page for GMLAS driver.
