.. _vector.edigeo:

EDIGEO
======

.. shortname:: EDIGEO

.. built_in_by_default::

This driver reads files encoded in the French EDIGEO exchange format, a
text based file format aimed at exchanging geographical information
between GIS, with powerful description capabilities, topology modeling,
etc.

The driver has been developed to read files of the French PCI (Plan
Cadastral Informatisé - Digital Cadastral Plan) as produced by the DGI
(Direction Générale des Impots - General Tax Office). The driver should
also be able to open other EDIGEO based products.

The driver must be provided with the .THF file describing the EDIGEO
exchange and it will read the associated .DIC, .GEO, .SCD, .QAL and .VEC
files.

In order the SRS of the layers to be correctly built, the IGNF file that
contains the definition of IGN SRS must be placed in the directory of
PROJ resource files.

The whole set of files will be parsed into memory. This may be a
limitation if dealing with big EDIGEO exchanges.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Labels
------

For EDIGEO PCI files, the labels are contained in the ID_S_OBJ_Z_1_2_2
layer. OGR will export styling following the :ref:`ogr_feature_style`.

It will also add the following fields :

-  OGR_OBJ_LNK: the id of the object that is linked to this label
-  OBJ_OBJ_LNK_LAYER: the name of the layer of the linked object
-  OGR_ATR_VAL: the value of the attribute to display (found in the ATR
   attribute of the OGR_OBJ_LNK object)
-  OGR_ANGLE: the rotation angle in degrees (0 = horizontal,
   counter-clock-wise oriented)
-  OGR_FONT_SIZE: the value of the HEI attribute multiplied by the value
   of the configuration option :decl_configoption:`OGR_EDIGEO_FONT_SIZE_FACTOR`
   that defaults to 2.

Combined with the FON (font family) attributes, they can be used to
define the styling in QGIS for example.

By default, OGR will create specific layers (xxx_LABEL) to dispatch into
the various labels of the ID_S_OBJ_Z_1_2_2 layer according to the value
of xxx=OBJ_OBJ_LNK_LAYER. This can be disabled by setting
OGR_EDIGEO_CREATE_LABEL_LAYERS to NO.

See Also
--------

-  `Introduction to the EDIGEO
   standard <http://georezo.net/wiki/main/donnees/edigeo>`__ (in French)
-  `EDIGEO standard - AFNOR NF Z
   52000 <http://georezo.net/wiki/_media/main/geomatique/norme_edigeo.zip>`__
   (in French)
-  `Standard d'échange des objets du PCI selon la norme
   EDIGEO <https://www.craig.fr/sites/www.craig.fr/files/contenu/60-2010-le-pci-en-auvergne/docs/edigeopci.pdf>`__
   (in French)
-  `Homepage of the French Digital Cadastral
   Plan <http://www.cadastre.gouv.fr>`__ (in French)
-  `Geotools EDIGEO module
   description <http://old.geotools.org/77692976.html>`__
   (in English), `unmaintained and removed <https://github.com/geotools/geotools/pull/2446/files>`__
-  `Sample of EDIGEO
   data <https://github.com/geotools/geotools/tree/affa340d16681f1bb78673d23fb38a6c1eb2b38a/modules/unsupported/edigeo/src/test/resources/org/geotools/data/edigeo/test-data>`__
