%feature("docstring") GDALAlgorithmHS {

Python proxy of a ``GDALAlgorithm``.

};

%extend GDALAlgorithmHS {

%feature("docstring")  Finalize {

Close any dataset arguments or outputs of the algorithm.

Returns
-------
bool

};

%feature("docstring")  GetActualAlgorithm {

Return the actual algorithm that is going to be invoked, if the
current algorithm has sub-algorithms.

Returns
-------
Algorithm

Examples
--------
>>> alg = gdal.GetGlobalAlgorithmRegistry().InstantiateAlg("raster")
>>> alg.ParseCommandLineArguments(["convert" ,"--input", "byte.tif", "--output",  "/tmp/byte.png"])
True
>>> alg.GetActualAlgorithm().GetDescription()
'Convert a raster dataset.'

};

%feature("docstring")  GetArg {

Get an :py:class:`AlgorithmArg` object that can be queried for information about an argument.

Parameters
----------
argName : str

Returns
-------
AlgorithmArg

Examples
--------
>>> alg = gdal.Algorithm("raster", "polygonize")
>>> arg = alg.GetArg("connect-diagonal-pixels")
>>> arg.GetDescription()
'Consider diagonal pixels as connected'

};

%feature("docstring")  GetArgNames {

Get a list of arguments understood by the Algorithm.

Returns
-------
list

Examples
--------
>>> alg = gdal.Algorithm("raster", "convert")
>>> sorted(alg.GetArgNames())
['append', 'config', 'creation-option', ..., 'progress', 'quiet']


};

%feature("docstring")  GetArgNonConst {

:meta private:

};

%feature("docstring")  GetDescription {

Get a human-readable description of the Algorithm.

Returns
-------
str

Examples
--------
>>> alg = gdal.Algorithm("raster", "convert")
>>> alg.GetDescription()
'Convert a raster dataset.'

};

%feature("docstring")  GetHelpFullURL {

Get a URL for the algorithm's documentation.

Returns
-------
str

Examples
--------
>>> alg = gdal.Algorithm("raster", "convert")
>>> alg.GetHelpFullURL()
'https://gdal.org/programs/gdal_raster_convert.html'

};

%feature("docstring")  GetLongDescription {

:meta private:

};

%feature("docstring")  GetName {

Get the name of the algorithm.

Returns
-------
str

Examples
--------
>>> gdal.Algorithm("raster", "info").GetName()
'info'

};

%feature("docstring")  GetSubAlgorithmNames {

Return a list of names of sub-algorithms.

Returns
-------
list or None

Examples
--------
>>> alg = gdal.Algorithm("raster")
>>> alg.GetSubAlgorithmNames()
['as-features', 'aspect', ..., 'viewshed', 'zonal-stats']

};

%feature("docstring")  GetUsageAsJSON {

Returns a JSON representation of an algorithm's usage information, serialized to a string.

Returns
-------
str

Examples
--------
>>> import json
>>> import pprint
>>> usage = json.loads(gdal.Algorithm("vector", "info").GetUsageAsJSON())
>>> pprint.pprint(usage)
{'description': 'Return information on a vector dataset.',
 'full_path': ['gdal', 'vector', 'info'],
 'input_arguments': [{'category': 'Base',
                      'choices': ['json', 'text'],
                      'description': 'Output format',
                      'name': 'output-format',
                      'required': False,
                      'type': 'string'},
                     {'category': 'Advanced',
                      'description': 'Open options',
                      'max_count': 2147483647,
                      'metavar': '<KEY>=<VALUE>',
                      'min_count': 0,
                      'name': 'open-option',
                      'packed_values_allowed': False,
                      'repeated_arg_allowed': True,
                      'required': False,
                      'type': 'string_list'},
                     {'category': 'Advanced',
                      'description': 'Input formats',
                      'max_count': 2147483647,
                      'metadata': {'required_capabilities': ['DCAP_VECTOR']},
                      'min_count': 0,
                      'name': 'input-format',
                      'packed_values_allowed': True,
                      'repeated_arg_allowed': True,
                      'required': False,
                      'type': 'string_list'},
                     {'category': 'Base',
                      'dataset_type': ['vector'],
                      'description': 'Input vector datasets',
                      'input_flags': ['name', 'dataset'],
                      'max_count': 2147483647,
                      'min_count': 0,
                      'name': 'input',
                      'packed_values_allowed': False,
                      'repeated_arg_allowed': True,
                      'required': True,
                      'type': 'dataset_list'},
                     {'category': 'Base',
                      'description': 'Input layer name',
                      'max_count': 2147483647,
                      'min_count': 0,
                      'mutual_exclusion_group': 'layer-sql',
                      'name': 'input-layer',
                      'packed_values_allowed': True,
                      'repeated_arg_allowed': True,
                      'required': False,
                      'type': 'string_list'},
                     {'category': 'Base',
                      'description': 'List all features (beware of RAM '
                                     'consumption on large layers)',
                      'mutual_exclusion_group': 'summary-features',
                      'name': 'features',
                      'required': False,
                      'type': 'boolean'},
                     {'category': 'Base',
                      'description': 'List the layer names and the geometry '
                                     'type',
                      'mutual_exclusion_group': 'summary-features',
                      'name': 'summary',
                      'required': False,
                      'type': 'boolean'},
                     {'category': 'Base',
                      'description': 'Limit the number of features per layer '
                                     '(implies --features)',
                      'metavar': 'FEATURE-COUNT',
                      'min_value': 0,
                      'min_value_is_included': True,
                      'name': 'limit',
                      'required': False,
                      'type': 'integer'},
                     {'category': 'Base',
                      'description': 'Execute the indicated SQL statement and '
                                     'return the result',
                      'metavar': '<statement>|@<filename>',
                      'mutual_exclusion_group': 'layer-sql',
                      'name': 'sql',
                      'required': False,
                      'type': 'string'},
                     {'category': 'Base',
                      'description': 'Attribute query in a restricted form of '
                                     'the queries used in the SQL WHERE '
                                     'statement',
                      'metavar': '<WHERE>|@<filename>',
                      'name': 'where',
                      'required': False,
                      'type': 'string'},
                     {'category': 'Base',
                      'description': 'SQL dialect',
                      'name': 'dialect',
                      'required': False,
                      'type': 'string'}],
 'input_output_arguments': [],
 'name': 'info',
 'output_arguments': [{'category': 'Base',
                       'description': 'Output string, in which the result is '
                                      'placed',
                       'name': 'output-string',
                       'required': False,
                       'type': 'string'}],
 'short_url': '/programs/gdal_vector_info.html',
 'sub_algorithms': [],
 'supports_streamed_output': True,
 'url': 'https://gdal.org/programs/gdal_vector_info.html'}

};

%feature("docstring")  HasSubAlgorithms {

Returns whether the algorithm has sub-algorithms.

Returns
-------
bool

Examples
--------
>>> alg = gdal.Algorithm("raster")
>>> alg.HasSubAlgorithms()
True
>>> subalg = alg.InstantiateSubAlgorithm("convert")
>>> subalg.HasSubAlgorithms()
False

};

%feature("docstring")  InstantiateSubAlgorithm {

Instantiate a sub-algorithm if available.

Returns
-------
Algorithm or None

Examples
--------
>>> alg = gdal.Algorithm("raster")
>>> alg.InstantiateSubAlgorithm("convert")
<osgeo.gdal.Algorithm; proxy of <Swig Object of type 'GDALAlgorithmHS *' at ...> >
>>> subalg = alg.InstantiateSubAlgorithm("does_not_exist")

};

%feature("docstring")  ParseCommandLineArguments {

Parse a list of arguments for the algorithm, setting the appropriate parameters. May only be called once per algorithm.

Parameters
----------
args : list
    List of named or positional arguments

Returns
-------
bool
    Whether the arguments were successfully parsed

Examples
--------
>>> alg = gdal.Algorithm("vector", "info")
>>> alg.ParseCommandLineArguments(['poly.shp', '--format', 'text'])
True

};

%feature("docstring")  ParseRunAndFinalize {

Convenience method that calls :py:meth:`ParseCommandLineArguments`, :py:meth:`Run`, and :py:meth:`Finalize`.

Parameters
----------
args : list
    List of command line arguments
callback : callable, optional
    Optional progress callback function
callback_data
    Optional value to be provided as final argument to callback function

Returns
-------
bool
    ``True`` if the algorithm succeeded, ``False`` otherwise

};

};

%feature("docstring") GDALAlgorithmRegistryHS {

Python proxy of a ``GDALAlgorithmRegistry``.
Typically, the global algorithm registry is used (accessed via :py:func:`GetGlobalAlgorithmRegistry`.)

};

%extend GDALAlgorithmRegistryHS {

%feature("docstring")  GetAlgNames {

Return the names of available algorithms.

Returns
-------
list

Examples
--------
>>> gdal.GetGlobalAlgorithmRegistry().GetAlgNames()
['convert', 'dataset', 'info', 'mdim', 'pipeline', 'raster', 'vector', 'vsi']

};

%feature("docstring")  InstantiateAlg {

Instantiate an :py:class:`Algorithm`.

Most useful algorithms are accessed as a sub-algorithm of another algorithm.
For example, ``polygonize`` is a sub-algorithm of ``raster`` and can be
instantiated by first instantiating the primary algorithm, then using
:py:meth:`Algorithm.InstantiateSubAlgorithm`.

Returns
-------
Algorithm

Examples
--------

>>> alg = gdal.GetGlobalAlgorithmRegistry().InstantiateAlg('pipeline')    # or alg= gdal.Algorithm("pipeline")
>>> alg.GetName()
'pipeline'
>>> alg.GetArgNames()
['help', 'help-doc', 'json-usage', 'config', 'quiet', 'progress', 'input', 'output', 'output-format', 'pipeline', 'output-string', 'stdout']

>>> subalg = gdal.GetGlobalAlgorithmRegistry().InstantiateAlg('raster').InstantiateSubAlgorithm('polygonize')
>>> subalg.GetArgNames()
['help', 'help-doc', 'json-usage', 'config', 'quiet', 'progress', 'input-format', 'open-option', 'input', 'output-format', 'output-open-option', 'output', 'creation-option', 'layer-creation-option', 'overwrite', 'update', 'overwrite-layer', 'append', 'output-layer', 'band', 'attribute-name', 'connect-diagonal-pixels', 'commit-interval']

};

};

%feature("docstring")  GDALGetGlobalAlgorithmRegistry {

Return the default algorithm registry.

Returns
-------
AlgorithmRegistry

};

%feature("docstring")  GDALAlgorithmArgTypeName {

Return a text representation of an argument type code.

Parameters
----------
type : int

Returns
-------
str

Examples
--------
>>> gdal.AlgorithmArgTypeName(gdal.GAAT_REAL)
'real'

};

%feature("docstring")  GDALAlgorithmArgTypeIsList {

Test whether an argument type represents a list.

Parameters
----------
type : int

Returns
-------
bool

Examples
--------
>>> gdal.AlgorithmArgTypeIsList(gdal.GAAT_REAL)
False
>>> gdal.AlgorithmArgTypeIsList(gdal.GAAT_REAL_LIST)
True

};

