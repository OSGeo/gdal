/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/

#pragma once

//
// Dictionary CUSRDSystem
//
// Do not manually edit this file. It should be re-generated, when required, by running MakeAPICodes.exe.
// This program will also re-generate RasterDictionary.xml.
//
// New error codes should be added to m_vvDictionary in MINTSystem.cpp
//

#define MINT_SUCCESS                                                            0             //    The operation succeeded.
#define MINT_NO_ERROR                                                           1             //    The operation reported no error.
#define MINT_NO_RESULT                                                          2             //    The operation did not report a result.
#define MINT_UNKNOWN_RESULT                                                     3             //    The operation reported an unknown result.
#define MINT_UNKNOWN_FAILURE                                                    4             //    An unknown error has occurred.
#define MINT_FAILURE                                                            5             //    The operation failed.
#define MINT_NO_STATUS                                                          6             //    
#define MINT_INVALID_ARG                                                        7             //    An invalid argument was supplied.
#define MINT_NULL_ARG                                                           8             //    An invalid (null) argument was supplied.
#define MINT_EXCEPTION                                                          9             //    An unexpected failure occurred.
#define MINT_EXCEPTION_MSG                                                      10            //    An unexpected failure occurred: "%1!%r!".
#define MINT_ALLOCATE_FAIL                                                      11            //    Failed to allocate memory.
#define MINT_UNSUPPORTED_VERSION                                                12            //    Version not supported.
#define MINT_DLL_NOTLOADED                                                      971           //    The required DLL could not be loaded.
#define MINT_FILEERROR_GENERAL                                                  13            //    An error occurred accessing the file: %1!%r!.
#define MINT_FILEERROR_OPEN                                                     14            //    Failed to open file.
#define MINT_FILEERROR_OPEN_NAME                                                15            //    Failed to open file: %1!%r!.
#define MINT_FILEERROR_OPEN_CAUSE                                               16            //    Failed to open file: %1!%r!, cause %2!%r!.
#define MINT_FILEERROR_CLOSE                                                    17            //    Failed to close file: %1!%r!, cause %2!%r!.
#define MINT_FILEERROR_READ                                                     18            //    An error was encountered while reading the file: %1!%r!.
#define MINT_FILEERROR_WRITE                                                    19            //    An error was encountered while writing the file: %1!%r!.
#define MINT_FILEERROR_DELETE                                                   20            //    Failed to delete file: %1!%r!, cause %2!%r!.
#define MINT_FILEERROR_SETLENGTH                                                21            //    Failed to resize file: %1!%r!, cause %2!%r!.
#define MINT_FILEERROR_NOTFOUND                                                 22            //    The file does not exist: %1!%r!.
#define MINT_FILEERROR_BADPATH                                                  23            //    File path not found: %1!%r!.
#define MINT_FILEERROR_EMPTYNAME                                                24            //    The file name is empty.
#define MINT_FILEERROR_TOOMANYOPEN                                              25            //    There are too many open files in this system. Try closing some programs or rebooting the system.
#define MINT_FILEERROR_ACCESSROOTDIR                                            26            //    This is the root directory. You cannot use it as a file: %1!%r!.
#define MINT_FILEERROR_ACCESSDIR                                                27            //    This is a directory. You cannot use it as a file: %1!%r!.
#define MINT_FILEERROR_ACCESSSYSTEM                                             28            //    This is a special system file. You cannot access it: %1!%r!.
#define MINT_FILEERROR_ACCESSRO                                                 29            //    You cannot write to a read-only file: %1!%r!.
#define MINT_FILEERROR_INVALIDFILE                                              30            //    An invalid file handle was used.
#define MINT_FILEERROR_REMOVECURDIR                                             31            //    An attempt was made to remove the current directory.
#define MINT_FILEERROR_DIRFULL                                                  32            //    The file could not be created because the directory is full: %1!%r!.
#define MINT_FILEERROR_BADSEEK                                                  33            //    A software error occurred: seek to bad file position.
#define MINT_FILEERROR_HARDIO                                                   34            //    There was a hardware error. There may be a problem with your computer or disk drive.
#define MINT_FILEERROR_HARDIO_REMOVE                                            35            //    There was a problem accessing the drive. Please ensure that the medium is present.
#define MINT_FILEERROR_HARDIO_REMOTE                                            36            //    There was a problem accessing the file. There may be a problem with your network.
#define MINT_FILEERROR_SHARING                                                  37            //    A file sharing violation occurred: %1!%r!.
#define MINT_FILEERROR_LOCK                                                     38            //    A file lock violation occurred: %1!%r!.
#define MINT_FILEERROR_DISKFULL                                                 39            //    The file could not be written as the disk is full.
#define MINT_FILEERROR_EOF                                                      40            //    An attempt was made to access past the end of the file: %1!%r!.
#define MINT_FILEERROR_RENAME                                                   41            //    Failed to rename file %1!%r!, cause %2!%r!.
#define MINT_FILEERROR_BADEXT                                                   42            //    The file has an invalid extension.
#define MINT_FILEERROR_NOSTREAM                                                 43            //    No file stream was supplied.
#define MINT_FILEERROR_STREAM_NOTOPEN                                           44            //    The file stream is not open.
#define MINT_FAIL_CREATE_FILE                                                   45            //    Failed to create the file.
#define MINT_CREATE_FILE                                                        46            //    Creating output file: %1!%r!.
#define MINT_FILE_EXISTS                                                        47            //    The output file already exists.
#define MINT_FILE_OPEN                                                          48            //    Opening file: %1!%r!.
#define MINT_FILE_NO_DIR                                                        49            //    The folder does not exist : %1!%r.
#define MIR_INTERP_NO_OUTPUT_FOLDER                                             50            //    Output folder verification failure.
#define MINT_FILE_NOFINDDIR                                                     51            //    A suitable folder could not be found.
#define MINT_FILE_OVERWRITE                                                     52            //    Warning: Overwriting file %1!%r!.
#define MIR_INTERP_OUTPUT_ALREADY_OPEN                                          53            //    The output file is already open in another process.
#define MIR_INTERP_VERIFY_ALREADY_OPEN                                          54            //    The file is currently open for read or write by another process: %1!%r!.
#define MIR_INTERP_VERIFY_CLOSE                                                 55            //    Close the file before proceeding: %1!%r!.
#define MIR_INTERP_VERIFY_CLOSE2                                                56            //    The output file %1!%r! exists and is currently open in another process. Please close the file (or modify the output raster file name) and restart interpolation.
#define MIR_ERROR_CREATETEMPFILE                                                57            //    Error: Failed to create temporary file.
#define MIR_INTERP_VERIFY_INPUT_FAILURE                                         58            //    Input file verification failure.
#define MIR_INTERP_VERIFY_OUTPUT_FILENAME                                       59            //    The output raster file name %1!%r! is invalid. The filename is blank.
#define MIR_DELETED                                                             60            //    Deleted %1!%r!.
#define MINT_XML_NODOC                                                          61            //    XML parsing error. No valid XML document found.
#define MINT_XML_NOROOT                                                         62            //    XML parsing error. No root element found.
#define MINT_XML_NOROOT_NAMED                                                   63            //    XML parsing error. No root element found. (expecting: %1!%r!).
#define MINT_XML_BADELEMENT                                                     64            //    XML parse warning. Unrecognized element found.
#define MINT_XML_BADELEMENT_NAMED                                               65            //    XML parse warning. Unrecognized element "%1!%r!" found containing "%2!%r!".
#define MINT_PARSE_UNKNOWNTOKEN                                                 66            //    Unknown token encountered whilst parsing: %1!%r!
#define MINT_PROCESS_BEGIN                                                      67            //    Processing operation started.
#define MINT_PROCESS_COMPLETE                                                   68            //    Processing operation complete.
#define MINT_PROCESS_TERMINATED                                                 69            //    Processing operation terminated.
#define MINT_PROCESS_FAILED                                                     70            //    Processing operation failed.
#define MINT_PROCESS_PAUSED                                                     71            //    Process execution paused.
#define MINT_PROCESS_RESUMED                                                    72            //    Process execution resumed.
#define MINT_PROCESS_STOPPED                                                    73            //    Process execution stopped.
#define MINT_PROCESS_STARTED                                                    74            //    Process execution restarted.
#define MINT_PROCESS_WAITING                                                    75            //    Process waiting to resume.
#define MINT_PROCESS_CANCELED                                                   76            //    Process execution canceled.
#define MINT_OPERATION_PROGRESS                                                 77            //    Operation %1!%r!% complete.
#define MINT_PROCESS_PROGRESS                                                   78            //    Process %1!%r!% complete.
#define MINT_TRACKER_CREATE_FAIL                                                79            //    Memory allocation failure. Could not create a process tracker object.
#define MINT_ZIP_FAILINIT                                                       80            //    ZIP compression failed to initialize. %1!%r!
#define MINT_ZIP_FAIL                                                           81            //    ZIP compression failed. %1!%r!
#define MINT_ZIP_FAILEND                                                        82            //    ZIP Compression failed to end. %1!%r!
#define MINT_LZMA_FAILMEMRETRY                                                  83            //    LZMA compression at level %1!%r! failed due to a failed memory allocation. Retrying.
#define MINT_LZMA_FAILMEM                                                       84            //    LZMA compression failed due to a failed memory allocation.
#define MINT_LZMA_FAILPARAM                                                     85            //    LZMA compression failed due to a parameter error.
#define MINT_LZMA_FAILOVRRUNRETRY                                               86            //    LZMA compression at level %1!%r! failed due to a buffer overrun. Retrying.
#define MINT_LZMA_FAILOVRUN                                                     87            //    LZMA compression failed due to a buffer overrun.
#define MINT_LZMA_FAILTHREADRETRY                                               88            //    LZMA compression at level %1!%r! failed due to thread issue.
#define MINT_LZMA_FAILTHREAD                                                    89            //    LZMA compression failed due to a thread issue.
#define MINT_LZMA_FAIL                                                          90            //    LZMA compression failed. %1!%r!
#define MINT_UNZIP_FAILINIT                                                     91            //    ZIP decompression failed to initialize. %1!%r!
#define MINT_UNZIP_FAIL                                                         92            //    ZIP decompression failed. %1!%r!
#define MINT_UNLZMA_FAIL                                                        93            //    LZMA decompression failed. %1!%r!
#define MINT_INVALID_EXPRESSION                                                 94            //    Invalid expression.
#define MINT_INVALID_EXPRESSION_SQUARE_BRACKETS                                 95            //    Invalid expression: Invalid use of square brackets.
#define MINT_INVALID_EXPRESSION_STATS_SINGLE_BAND_ONLY                          96            //    Invalid expression: The statistics functions cannot be applied to multiple bands.
#define MINT_INTERP_READING                                                     97            //    Reading file: %1!%r!.
#define MINT_INTERP_FILE_FAIL                                                   98            //    Failed to read file: %1!%r!.
#define MINT_INTERP_ZIP_FIND_FAIL                                               99            //    Failed to find file %1!%r! in ZIP file: %2!%r!.
#define MIR_INTERP_ZIP_READ_FAIL                                                100           //    Failed to read ZIP file: %1!%r!.
#define MINT_COMPUTE_STATS1                                                     101           //    Compute histogram.
#define MINT_COMPUTE_STATS2                                                     102           //    Compute basic univariate statistics.
#define MINT_COMPUTE_STATS3                                                     103           //    Compute skewness statistics.
#define MINT_COMPUTE_STATS4                                                     104           //    Compute basic bivariate statistics.
#define MINT_COMPUTE_STATS5                                                     105           //    Compute correlation statistics.
#define MINT_COMPUTE_STATS6                                                     106           //    Compute rank statistics.
#define MINT_COMPUTE_STATS7                                                     107           //    Compute expectation statistics.
#define MINT_COMPUTE_STATS8                                                     108           //    Compute basic multivariate statistics.
#define MINT_COMPUTE_STATS9                                                     109           //    Compute first order surface fit.
#define XCFS_NOTOPEN_MOD                                                        110           //    XCFS error: XCFS is not modifiable.
#define XCFS_FLGS_INCOMPATIBLE                                                  111           //    XCFS error: Unable to re-open stream (supplied flags are incompatible).
#define XCFS_MOD_NOTALLOWED                                                     112           //    XCFS error: Stream is not modifiable.
#define XCFS_EX_ACCESS                                                          113           //    XCFS error: Stream is open for exclusive access.
#define XCFS_ZERO_FILESIZE                                                      114           //    XCFS error: Unable to truncate stream.
#define XCFS_ZERO_LEN                                                           115           //    XCFS error: File has zero length.
#define XCFS_EST_FILELEN                                                        116           //    XCFS error: Unable to determine XCFS file length.
#define XCFS_FAIL_OPEN                                                          117           //    XCFS error: Unable to open XCFS file.
#define XCFS_FAIL_OPEN_STREAM                                                   118           //    XCFS error: Unable to open stream.
#define XCFS_FAIL_CREATE_STREAM                                                 119           //    XCFS error: Unable to create stream.
#define XCFS_FAIL_OPEN_MFT                                                      120           //    XCFS error: Unable to open file table system stream.
#define XCFS_FAIL_OPEN_EMPTY_CLUSTER                                            121           //    XCFS error: Unable to open empty cluster stream.
#define XCFS_FAIL_OPEN_FILE_INDEX                                               122           //    XCFS error: Unable to open file index server stream.
#define XCFS_FAIL_READ_ROOT                                                     123           //    XCFS error: Unable to read root cluster.
#define XCFS_FAIL_CLUSTERCHAIN                                                  124           //    XCFS error: Unable to read file cluster chain.
#define XCFS_FAIL_CLOSE                                                         125           //    XCFS error: Unable to close XCFS file.
#define XCFS_FAIL_COMMIT                                                        126           //    XCFS error: Unable to commit XCFS file.
#define XCFS_FAIL_COMMIT_ROOT                                                   127           //    XCFS error: Unable to commit root.
#define XCFS_FAIL_COMMIT_TABLE                                                  128           //    XCFS error: Unable to commit file table stream.
#define XCFS_FAIL_COMMIT_FILE                                                   129           //    XCFS error: Unable to commit a file.
#define XCFS_FAIL_COMMIT_FILE_INDEX                                             130           //    XCFS error: Unable to commit file index server stream.
#define XCFS_FAIL_COMMIT_EMPTY_CLUSTER                                          131           //    XCFS error: Unable to commit empty cluster stream.
#define XCFS_FAIL_CLOSUREFLAG                                                   132           //    XCFS error: Closure flag indicates the file failed to close properly. The file may be corrupt.
#define XCFS_FAIL_VERSION                                                       133           //    XCFS error: File version is not supported.
#define XCFS_FAIL_NOTXCFS                                                       134           //    XCFS error: File is not an XCFS file.
#define XCFS_FAIL_STREAM_NOTOPEN                                                135           //    XCFS error: The stream is not open.
#define XCFS_FAIL_ARCHIVE_CORRUPT                                               136           //    XCFS error: Archive sub-file is corrupt.
#define XCFS_FAIL_COMPRESS                                                      137           //    XCFS error: Compression operation failed. File data is corrupt.
#define XCFS_FAIL_DECOMPRESS                                                    138           //    XCFS error: Decompression operation failed. File data is corrupt.
#define XCFS_INVALID_FILECOUNT                                                  139           //    XCFS error: Root file count does not match file server.
#define XCFS_INVALID_CLUSTERCOUNT                                               140           //    XCFS error: Root cluster count does not match file size.
#define MIR_NOT_LICENSED                                                        141           //    MapInfo Advanced (Raster) is not licensed.
#define MIR_INVALID_FIELD                                                       142           //    An invalid field index was referenced.
#define MIR_INVALID_BAND                                                        143           //    An invalid band index was referenced.
#define MIR_INVALID_EVENT                                                       144           //    An invalid event index was referenced.
#define MIR_INVALID_LEVEL                                                       145           //    An invalid level index was referenced.
#define MIR_FAIL_LOADSTRUCTURE                                                  146           //    Failed to load the raster structural data.
#define MIR_INVALID_STRUCTURE                                                   147           //    Failed to validate the raster structural data.
#define MIR_FAIL_CREATE                                                         148           //    Failed to create the raster.
#define MIR_FAIL_COMMIT                                                         149           //    Failed to commit the raster to file.
#define MIR_FAIL_NOZIPSUPPORT                                                   150           //    Unable to support ZIP file format.
#define MIR_ZIPEDIT_NOT_ALLOWED                                                 151           //    The edit cell operation is not supported for rasters stored in a ZIP archive.
#define MIR_RASTER_CLOSED                                                       152           //    Raster has been closed.
#define MIR_INVALIDHANDLE                                                       153           //    An invalid handle has been provided.
#define MIR_FAIL_INVALIDDRIVER                                                  154           //    An invalid driver was requested.
#define MIR_INVALID_CONVERSION                                                  155           //    The requested raster conversion is not supported.
#define MIR_BEGIN_ITER_FAILED                                                   156           //    Unable to start raster cell iterator.
#define MIR_RGB_FIELD_NOT_FOUND                                                 157           //    RGB field not found.
#define MIR_CLASSVAL_FIELD_NOT_FOUND                                            158           //    Class value field not found in classification table.
#define MIR_NO_TILE                                                             159           //    The raster contains no tiles.
#define MIR_EDITCELL_NOT_ALLOWED                                                160           //    The edit cell operation is not supported.
#define MIR_CREATE_NOT_ALLOWED                                                  161           //    The create operation is not supported by this raster driver.
#define MIR_ZIPCREATE_NOT_ALLOWED                                               162           //    The create operation is not supported for rasters stored in a ZIP archive.
#define MIR_NOT_IMPLEMENTED                                                     163           //    The requested feature is not yet implemented.
#define MIR_READ_NOT_ALLOWED                                                    164           //    Raster is not readable.
#define MIR_WRITE_NOT_ALLOWED                                                   165           //    Raster is not writable.
#define MIR_EDIT_INVALID                                                        166           //    An edit operation has been attempted on a raster that is not editable.
#define MIR_EDIT_FAILED                                                         167           //    An edit operation failed.
#define MIR_FORWARD_VERSION                                                     168           //    The version of input raster is higher. Some features may not be loaded.
#define MIR_ONLY_CONTINOUS_SUPPORTED                                            169           //    The raster field type is invalid: only continuous field is supported.
#define MIR_RASTER_SOURCE_FILE_CHANGE                                           964           //    A driver has changed the raster source file name and restarted mounting.
#define MIR_VD_EXTRA_FIELDS                                                     170           //    Too many fields - additional fields will be ignored.
#define MIR_VD_NO_FIELDS                                                        171           //    No fields - raster is invalid.
#define MIR_VD_EXTRA_BANDS                                                      172           //    Too many bands - additional bands will be ignored.
#define MIR_VD_NO_BANDS                                                         173           //    No bands - raster is invalid.
#define MIR_BAND_TYPE_NOTSUPPORTED                                              174           //    Band type is not supported by the raster driver.
#define MIR_VD_MADE_BASETILESIZE                                                175           //    Base level tile size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_UNDERVIEWTILESIZE                                           176           //    Underview tile size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_OVERVIEWTILESIZE                                            177           //    Overview tile size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_BASEMAPSIZE                                                 178           //    Base level map size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_UNDERVIEWMAPSIZE                                            179           //    Underview map size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_OVERVIEWMAPSIZE                                             180           //    Overview map size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_CAPSTONEMAPSIZE                                             181           //    Capstone map size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_MADE_GRIDSIZE                                                    182           //    Grid size was changed to (%1!%r!,%2!%r!).
#define MIR_VD_EXCEEDSGRIDSIZE                                                  183           //    Grid size exceeds limit supported by the driver.
#define MIR_VD_MADE_CELLSIZE                                                    184           //    Cell size was changed to (%1!%.10g!,%2!%.10g!).
#define MIR_VD_BAD_COORDSYS                                                     185           //    Supplied coordinate system is invalid.
#define MIR_VD_NO_TRANSFORM                                                     186           //    A default world-cell transformation has been adopted.
#define MIR_VD_NO_ENDIAN                                                        187           //    Supplied endian mode is not supported by this driver
#define MIR_VD_NO_CONTINUOUS                                                    188           //    Field type (continuous) is not supported by this driver.
#define MIR_VD_NO_IMAGE                                                         189           //    Field type (image) is not supported by this driver.
#define MIR_VD_NO_IMAGEPALETTE                                                  190           //    Field type (image palette) is not supported by this driver.
#define MIR_VD_NO_CLASSIFIED                                                    191           //    Field type (classified) is not supported by this driver.
#define MIR_VD_MADE_CONTINUOUS                                                  192           //    Field type was changed (to continuous).
#define MIR_VD_MADE_IMAGE                                                       193           //    Field type was changed (to image).
#define MIR_VD_MADE_IMAGEPALETTE                                                194           //    Field type was changed (to image palette).
#define MIR_VD_MADE_CLASSIFIED                                                  195           //    Field type was changed (to classified).
#define MIR_VD_MADENO_COMRPESSION                                               196           //    Compression was disabled.
#define MIR_VD_MADE_COMRPESSION                                                 197           //    Compression was changed to %1!%r!.
#define MIR_VD_MADE_COMRPESSION_LOSSLESS                                        198           //    Invalid compression method. Reset to default lossless compression.
#define MIR_VD_MADE_DATATYPE                                                    199           //    Band data type was changed to %1!%r!.
#define MIR_VD_MADE_STORETYPE                                                   200           //    Band store data type was changed to %1!%r!.
#define MIR_VD_MADE_STORETYPEEQUAL                                              201           //    Band data type and storage data type were made to match.
#define MIR_VD_BANDDATASTORENOTEQUAL                                            202           //    Band data type and stored type differ, but no transform is in use. Is this the intention?
#define MIR_VD_MADE_NULLTYPE                                                    203           //    Band null type was changed to %1!%r!.
#define MIR_VD_MADE_NULLDATATYPE                                                204           //    Band null data type was changed to %1!%r!.
#define MIR_VD_MADE_NULLDATATYPEEQUAL                                           205           //    Band null data type was changed to match the data type.
#define MIR_VD_MADE_NULLVALUE                                                   206           //    Band null type was changed to %1!%.10g!.
#define MIR_VD_NO_RESTRICTDECIMALS                                              207           //    Band decimal restriction is not supported by this driver.
#define MIR_VD_SET_TRANSFORM                                                    208           //    A band transform was defined.
#define MIR_VD_BAD_TRANSFORM                                                    209           //    The supplied band transform was not honored.
#define MIR_VD_NO_TRANSFORMSCALE                                                210           //    The supplied band transform scale was not honored.
#define MIR_VD_NO_TRANSFORMOFFSET                                               211           //    The supplied band transform offset was not honored.
#define MIR_VD_SET_CLIP                                                         212           //    A band clip range was defined.
#define MIR_VD_NO_CLIPMIN                                                       213           //    The supplied band clip band clip minimum value was not honored.
#define MIR_VD_NO_CLIPMAX                                                       214           //    The supplied band clip band clip maximum value was not honored.
#define MIR_VD_NO_CLASSTABLE                                                    215           //    No classification table was supplied.
#define MIR_VD_SET_PRIMARYCOLOR                                                 216           //    The color palette was assigned a primary color field.
#define MIR_VD_NO_PRIMARYCOLOR                                                  217           //    The color palette does not contain a color field.
#define MIR_VD_EXTRA_TABLE                                                      218           //    The supplied classification table was deleted as it is not supported by the field type.
#define MIR_VD_NO_PREDENCODE                                                    219           //    The supplied predictive encoding settings were not honored.
#define MIR_VD_NO_DISCRETE                                                      220           //    The discrete value setting is not supported by this driver.
#define MIR_VD_NO_EVENT                                                         221           //    The raster driver does not support multiple events. Only the last event will be retained.
#define MIR_ERROR_ITERATORBEGINFAIL                                             222           //    Error: Failed to begin iterator.
#define MIR_ERROR_ITERATORFAIL                                                  223           //    Error: Iterator failed.
#define MIR_ERROR_ITERATORFAILWRITE                                             224           //    Error: Iterator failed to write data.
#define MIR_ERROR_NORASTERACCESS                                                225           //    Error: Failed to acquire raster access.
#define MIR_RASTER_INFO_UNAVAILABLE                                             226           //    Raster information is not available.
#define MIR_RASTERINFO_NOT_MATCHED                                              227           //    Unable to match raster information.
#define MIR_CONNECT_TO_EXISTING                                                 228           //    Connected to open raster.
#define MIR_NO_CONCURRENT_ACCESS                                                229           //    The raster is currently open. No concurrent access is allowed.
#define MIR_FAIL_STATS                                                          230           //    Failed to compute the raster statistics.
#define MIR_RASTER_CACHE_UNAVAILABLE                                            231           //    Raster cache is unavailable.
#define MIR_FAIL_CREATE_RASTER_CACHE                                            232           //    Failed to create a raster cache.
#define MIR_TOO_LARGE                                                           233           //    The raster exceeds the maximum number of rows or columns allowed by this driver.
#define MIR_INTERP_DRIVER_CAPS                                                  234           //    Error: Raster driver capabilities were not retrieved.
#define MIR_RASTER_CLASS_SIZE_TOO_LARGE                                         235           //    Raster class table size is too large.
#define MIR_FLUSHING_TILES_S                                                    236           //    Writing cached tiles to disk.
#define MIR_FLUSHING_TILES_FINISHED_S                                           237           //    Writing cached tiles to disk completed successfully.
#define MIR_CONVERT_COMPLETED                                                   238           //    Convert operation completed successfully.
#define MIR_COMPUTE_STATS_FOR_BAND_S                                            239           //    Computing statistics for band %1!%r!.
#define MIR_COMPUTE_STATS_FOR_BAND_COMPLETE_S                                   240           //    Computing statistics for band %1!%r! completed.
#define MIR_COMPUTE_STATS_FOR_ALL_BAND_S                                        241           //    Computing statistics for field %1!%r! - all bands.
#define MIR_COMPUTE_STATS_FOR_ALL_BAND_COMPLETE_S                               242           //    Computing statistics for all bands completed.
#define MIR_START_COPYTO_MRRCACHE_S                                             243           //    Started copying to MRR cache.
#define MIR_FINISH_COPYTO_MRRCACHE_S                                            244           //    Finished copying to MRR cache.
#define MIR_START_COPYFROM_MRRCACHE_S                                           245           //    Started copying from MRR cache.
#define MIR_FINISH_COPYFROM_MRRCACHE_S                                          246           //    Finished copying from MRR cache.
#define MIR_UPDATING_OVERVIEWS_S                                                247           //    Updating overviews.
#define MIR_UPDATING_OVERVIEWS_FINISHED_S                                       248           //    Updating overviews completed.
#define MIR_START_CREATE_PERC_S                                                 249           //    Started creating PERC file %1!%r!.
#define MIR_FINISH_CREATE_PERC_S                                                250           //    Finished creating PERC file %1!%r!.
#define MIR_START_CREATE_PPRC_S                                                 251           //    Started creating PPRC file %1!%r!.
#define MIR_FINISH_CREATE_PPRC_S                                                252           //    Finished creating PPRC file %1!%r!.
#define MIR_START_CREATE_TERC_S                                                 253           //    Started creating TERC file.
#define MIR_FINISH_CREATE_TERC_S                                                254           //    Finished creating TERC file.
#define MIR_CONVERTING_INPUT_FILE                                               255           //    Convert operation started.
#define MIR_CONVERT_1                                                           256           //    Source raster: %1!%r!.
#define MIR_CONVERT_2                                                           257           //    Source raster format: %1!%r!.
#define MIR_CONVERT_3                                                           258           //    Source raster size %1!%r! x %2!%r! (total of %3!%r! cells).
#define MIR_SUGGESTINFO_FAIL                                                    259           //    Unable to Suggest Raster Info for provided inputs.
#define MIR_WRITING_OUTPUT_FILE                                                 260           //    Writing output file.
#define MIR_JOIN1                                                               261           //    Identified %1!%r! raster source files for joining.
#define MIR_JOIN2                                                               262           //    Validated %1!%r! raster source files for joining.
#define MIR_COLOR_BADFILE                                                       263           //    Invalid color file format.
#define MIR_READ_COLOUR_FAIL                                                    264           //    Failed to read color file.
#define MIR_COLOR_FAILWRITE                                                     265           //    Failed to write color file.
#define MIR_SEAMLESSTAB_FAILURE                                                 266           //    Seamless TAB output failure.
#define MIR_INTERP_LOW_MEM                                                      267           //    Warning: Available physical memory is low: %1!%r! MB reserved.
#define MIR_INTERP_BAND_COUNT_UNEXPECTED                                        268           //    Warning: File %1!%r! has an unexpected band count (Parse string: %2!%r!).
#define MIR_INTERP_INPUT_CONNECTED                                              269           //    Input stream statistics: %1!%r! sources connected, total of %2!%r! bytes (indicative).
#define MIR_INTERP_BAND_COUNT                                                   270           //    Output raster band count: %1!%r!.
#define MIR_INTERP_BAND_TYPE                                                    271           //    Band %1!%r!: %2!%r! Type = %3!%r!.
#define MIR_INTERP_SPATIAL_STATS_READ                                           272           //    Reading a small portion of the input data...
#define MIR_INTERP_SPATIAL_STATS_WARN1                                          273           //    Warning: Data may be reordered. Cell size recommendation may be poor.
#define MIR_INTERP_SPATIAL_STATS_WARN2                                          274           //    Warning: Data distribution may be clustered or skewed. Cell size recommendation may be poor.
#define MIR_INTERP_SPATIAL_STATS_TYPE1                                          275           //    Coordinates are UTM.
#define MIR_INTERP_SPATIAL_STATS_TYPE2                                          276           //    Coordinates are geodetic.
#define MIR_INTERP_SPATIAL_STATS_SEPERATION                                     277           //    Estimated station separation: %1!%.10g! units.
#define MIR_INTERP_SPATIAL_STATS_DIRECTION                                      278           //    Estimated station trend direction: %1!%.10g! degrees.
#define MIR_INTERP_SPATIAL_STATS_CELL1                                          279           //    Raster cell size recommendation: %1!%.10g! units.
#define MIR_INTERP_SPATIAL_STATS_CELL2                                          280           //    Raster cell size recommendation: %1!%.10g! units (%2!%.10g! arc sec).
#define MIR_COINCIDENTPOINT_REPORT                                              281           //    Found %1!%r! points coincident and %2!%r! eliminated.
#define MIR_INTERP_TYPE6                                                        282           //    Gridding: Stamped data distance.
#define MIR_INTERP_TYPE9                                                        283           //    Gridding: Grid stamp.
#define MIR_INTERP_TYPE_NN                                                      284           //    Gridding: Nearest Neighbour.
#define MIR_INTERP_TYPE_NNI                                                     285           //    Gridding: Natural Neighbour by Integration.
#define MIR_INTERP_BAND_COUNT_FAILURE                                           286           //    Error: Band count is zero.
#define MIR_INTERP_VALID_STATION_COUNT_ZERO                                     287           //    Error: No valid stations found.
#define MIR_ERROR_CREATERASTERFILE                                              288           //    Error: Failed to create raster file.
#define MIR_ERROR_NOPOINTCACHE                                                  289           //    Error: No point cache.
#define MIR_INTERP_BUILD_INPUT                                                  290           //    Build and verify the input data stream.
#define MIR_INTERP_SPATIAL_STATS                                                291           //    Determine representative spatial statistics.
#define MIR_INTERP_PARSE_SORT                                                   292           //    Error: Parsing and sorting input data.
#define MIR_TASK_COINCIDENTPOINT                                                293           //    Coincident point analysis.
#define MIR_TASK_DEFINEOUTPUTTILES                                              294           //    Declare output raster tiles.
#define MIR_TASK_NEARESTNEIGHBOUR                                               295           //    Grid by nearest neighbor.
#define MIR_TASK_STAMP                                                          296           //    Grid by stamping.
#define MIR_TASK_DISTANCE                                                       297           //    Grid by distance.
#define MIR_TASK_DENSITY                                                        298           //    Grid by density.
#define MIR_TASK_IDW                                                            299           //    Grid by inversed distance weighted.
#define MIR_INTERP_EXPORT                                                       300           //    Finalize raster export.
#define MIR_TASK_VORONOITESSELATE                                               301           //    Populate Voronoi tesselation.
#define MIR_TASK_VORONOIINTEGRATE                                               302           //    Grid by natural neighbor (integration).
#define MIR_TASK_SMOOTH                                                         303           //    Smooth.
#define MIR_INTERP_SPATIAL_OUTOFMEM                                             304           //    Out of memory in spatial analysis: Set the grid cell size manually to a larger value.
#define MIR_INTERP_DATA_PYRAMID_ALLOCATION_FAIL                                 305           //    Data pyramid memory allocation failure.
#define MIR_INTERP_CHECK_STORAGE                                                306           //    Checking storage volumes and system memory. This can cause a short delay.
#define MIR_INTERP_VOLUME_INFO1                                                 307           //    Volume %1!%r! [%2!%r!]: Size %3!%r! (%4!%r! MB), Free %5!%r! (%6!%r! MB) %7!%r! %8!%r!
#define MIR_INTERP_VOLUME_INFO2                                                 308           //    READ_ONLY
#define MIR_INTERP_VOLUME_INFO3                                                 309           //    COMPRESSED
#define MIR_INTERP_PHYSICAL_MEM_TOTAL                                           310           //    Physical memory (total)     : %1!%6llu! MB
#define MIR_INTERP_PHYSICAL_MEM_AVAILABLE                                       311           //    Physical memory (available) : %1!%6llu! MB
#define MIR_INTERP_VIRTUAL_MEM_TOTAL                                            312           //    Virtual memory (total)      : %1!%6llu! MB
#define MIR_INTERP_VIRTUAL_MEM_AVAILABLE                                        313           //    Virtual memory (available)  : %1!%6llu! MB
#define MIR_INTERP_PAGING_MEM_TOTAL                                             314           //    Paging memory (total)       : %1!%6llu! MB
#define MIR_INTERP_PAGING_MEM_AVAILABLE                                         315           //    Paging memory (available)   : %1!%6llu! MB
#define MIR_INTERP_CHECKING_STATUS                                              316           //    Checking status of input files.
#define MIR_INTERP_ZIP_FAILURE                                                  317           //    Could not open the ZIP file.
#define MIR_INTERP_ZIPITEM_FAILURE                                              318           //    Could not find item within the ZIP file.
#define MIR_INTERP_NO_TEMP_FOLDER                                               319           //    Temp folder verification failure.
#define MIR_INTERP_NO_OUTPUT_FILENAME                                           320           //    Output filename verification failure.
#define MIR_INTERP_SPATIAL_STATS_WARN3                                          321           //    Estimated point separation and cell size are widely divergent.
#define MIR_INTERP_EXTENTS                                                      322           //    Parse input, determine data extents and distribution.
#define MIR_INTERP_EXTENTS_PARSE1                                               323           //    Parsing for extents...
#define MIR_INTERP_EXTENTS_PARSE2                                               324           //    Parsing input extents (skipping %1!%r!)...
#define MIR_INTERP_EXTENTS_PARSE3                                               325           //    Parsing input extents...
#define MIR_INTERP_EXTENTS_READING                                              326           //    Reading all input files to obtain data extents and distribution statistics...
#define MIR_INTERP_EXTENTS_SUFFIX                                               327           //    (estimated)
#define MIR_INTERP_EXTENTS_WARN1                                                328           //    Warning: Statistics are based on sub-sampled data.
#define MIR_INTERP_EXTENTS_TOTAL_POINTS                                         329           //    Total input points: %1!%r! %2!%r!.
#define MIR_INTERP_EXTENTS_TOTAL_VALID_POINTS                                   330           //    Total valid input points: %1!%r! %2!%r!.
#define MIR_INTERP_EXTENTS_SPATIALX                                             331           //    Spatial range X: (%1!%.10g! to %2!%.10g!) %3!%.10g! %4!%r!.
#define MIR_INTERP_EXTENTS_SPATIALY                                             332           //    Spatial range Y: (%1!%.10g! to %2!%.10g!) %3!%.10g! %4!%r!.
#define MIR_INTERP_EXTENTS_WARN2                                                333           //    Warning: Point separation statistics may be invalid.
#define MIR_INTERP_EXTENTS_POINTSEP1                                            334           //    Point separation: less than or equal to %1!%.10g! (Avg : %2!%.10g!) : %3!%r! stations.
#define MIR_INTERP_EXTENTS_POINTSEP2                                            335           //    Point separation: greater than %1!%.10g! (Avg : %2!%.10g!) : %3!%r! stations.
#define MIR_INTERP_EXTENTS_BAND_RANGE                                           336           //    Band %1!%r! range: %2!%.10g! to %3!%.10g! %4!%r!.
#define MIR_INTERP_EXTENTS_COVERAGE1                                            337           //    Data coverage: Number of tiles %1!%r! occupying area %2!%.10g!.
#define MIR_INTERP_EXTENTS_COVERAGE2                                            338           //    Data coverage: Number of cells %1!%r! occupying area %2!%.10g!.
#define MIR_INTERP_EXTENTS_COVERAGE3                                            339           //    Data coverage: Minimum density %1!%.10g! points/unit area (Square of length %2!%.10g! units).
#define MIR_INTERP_EXTENTS_COVERAGE4                                            340           //    Data coverage: Maximum density %1!%.10g! points/unit area (Square of length %2!%.10g! units).
#define MIR_INTERP_EXTENTS_COVERAGE5                                            341           //    Data coverage: Mean density %1!%.10g! points/unit area (Square of length %2!%.10g! units).
#define MIR_INTERP_EXTENTS_COVERAGE6                                            342           //    Data coverage: Median density %1!%.10g! points/unit area (Square of length %2!%.10g! units).
#define MIR_INTERP_EXTENTS_COVERAGE7                                            343           //    Data coverage: Mode density %1!%.10g! points/unit area (Square of length %2!%.10g! units).
#define MIR_INTERP_PREPARE_GRID                                                 344           //    Prepare raster parameters.
#define MIR_INTERP_PREPARE_GRID_SIZE                                            345           //    Raster size: (%1!%r! x %2!%r!) cells of size (%3!%.10g!, %4!%.10g!).
#define MIR_INTERP_PREPARE_GRID_EXTENT                                          346           //    Raster extent: (%1!%.10g! %2!%.10g!) to (%3!%.10g! %4!%.10g!).
#define MIR_INTERP_PREPARE_GRID_WARN1                                           347           //    Warning: No patch coverage data available as input data was not parsed.
#define MIR_INTERP_PREPARE_GRID_PATCH1                                          348           //    Patch coverage: %1!%r! patches of size %2!%r! x %3!%r! cells with %4!%r! points per patch.
#define MIR_INTERP_PREPARE_GRID_PATCH2                                          349           //    Patch coverage: %1!%r! patches occupied (estimated) with %2!%r! points per patch.
#define MIR_INTERP_INVALID_DATA_RANGE                                           350           //    Interpolation Data range is invalid.
#define MIR_INTERP_EXPORT_MASK                                                  351           //    Export Source Mask.
#define MIR_INTERP_EXPORT_MASK1                                                 352           //    Exporting Source Mask...
#define MIR_INTERP_FINALISE_EXPORT                                              353           //    Finalize raster export.
#define MIR_INTERP_FINALISE_EXPORT_UNPAD                                        354           //    Processing: un-padding raster...
#define MIR_INTERP_PREPARE                                                      355           //    Prepare to interpolate.
#define MIR_INTERP_PREPARE_WARN1                                                356           //    Volume %1!%r! requires an estimated %2!%r! MB.
#define MIR_INTERP_PREPARE_WARN2                                                357           //    Warning: Volume %1!%r! has insufficient storage space.
#define MIR_INTERP_PREPARE_WARN3                                                358           //    Warning: Volume %1!%r! may run out of storage space.
#define MIR_INTERP_PREPARE_ERROR1                                               359           //    Error: Volume %1!%r! is read-only.
#define MIR_INTERP_PREPARE_STAMP1                                               360           //    Source data stamping will be performed direct to memory.
#define MIR_INTERP_PREPARE_STAMP2                                               361           //    Establishing data cache. This can cause a short delay...
#define MIR_INTERP_PREPARE_STAMP3                                               362           //    Source data will be in memory.
#define MIR_INTERP_PREPARE_STAMP4                                               363           //    Source data will be in virtual memory.
#define MIR_INTERP_PREPARE_STAMP5                                               364           //    Patch cache size: %1!%r! bytes.
#define MIR_INTERP_PREPARE_STAMP6                                               365           //    Warning: Estimated memory usage (%1!%r! MB) exceeds available physical memory (%2!%r! MB).
#define MIR_INTERP_PREPARE_STAMP7                                               366           //    Source data stamping will be performed in virtual memory.
#define MIR_INTERP_PREPARE_STAMP8                                               367           //    Patch size has been adjusted.
#define MIR_INTERP_PREPARE_STAMP9                                               368           //    Patch coverage: %1!%r! patches of size %2!%r! x %3!%r! cells with %4!%r! points per patch.
#define MIR_INTERP_PREPARE_STAMP10                                              369           //    Patch coverage: %1!%r! patches occupied (estimated) with %2!%r! points per patch.
#define MIR_INTERP_PREPARE_STAMP11                                              370           //    Up to %1!%r! stations will be cached per patch.
#define MIR_INTERP_PREPARE_STAMP12                                              371           //    Up to %1!%r! input stations will be internally cached.
#define MIR_INTERP_PREPARE_STAMP13                                              372           //    Source data stamping will be performed in-memory.
#define MIR_INTERP_PREPARE_PATCH1                                               373           //    Patch cache: All points will be stored in memory.
#define MIR_INTERP_PREPARE_ESTABLISH                                            374           //    Establishing grid files on disk.
#define MIR_INTERP_SOURCE_MASK_FAILURE1                                         375           //    Export source mask: Failed to open for writing.
#define MIR_INTERP_GRID_READER_FAILURE                                          376           //    Could not open the raster reader.
#define MIR_INTERP_GRID_WRITER_FAILURE                                          377           //    Could not open the raster writer.
#define MIR_INTERP_OUTPUT_VOLUME_NOSPACE                                        378           //    The output volume has insufficient storage space.
#define MIR_INTERP_OUTPUT_VOLUME_MAYHAVENOSPACE                                 379           //    The output volume may run out of storage space.
#define MIR_INTERP_OUTPUT_VOLUME_READONLY                                       380           //    The output volume is read-only.
#define MIR_INTERP_SOURCE_MASK_FAILURE                                          381           //    Source mask could not be read from file.
#define MIR_INTERP_MATH_EXCEPTION                                               382           //    Invalid mathematical operation attempted.
#define MIR_INTERP_FREE_MEMORY                                                  383           //    Estimated memory usage exceeds available physical.
#define MIR_INTERP_SYSTEM_ERROR                                                 384           //    System error trying to allocate patch memory.
#define MIR_INTERP_FAILURE1                                                     385           //    No command line control file argument found.
#define MIR_INTERP_FAILURE2                                                     386           //    Failed to read supplied command file.
#define MIR_INTERP_FAILURE3                                                     387           //    Failed to build input stream.
#define MIR_INTERP_FAILURE4                                                     388           //    Failed to parse input for statistics.
#define MIR_INTERP_FAILURE5                                                     389           //    Failed to parse input for extents.
#define MIR_INTERP_FAILURE6                                                     390           //    Failed to prepare raster extents.
#define MIR_INTERP_FAILURE7                                                     391           //    Failed raster export.
#define MIR_INTERP_FAILURE8                                                     392           //    Failed to write a final parameters command file.
#define MIR_INTERP_FAILURE9                                                     393           //    Failed to prepare for interpolation.
#define MIR_INTERP_FAILURE10                                                    394           //    Failed to stamp input data.
#define MIR_INTERP_FAILURE11                                                    395           //    Failed to clip.
#define MIR_INTERP_FAILURE12                                                    396           //    Failed first phase interpolation.
#define MIR_INTERP_FAILURE13                                                    397           //    Failed second phase interpolation.
#define MIR_INTERP_FAILURE14                                                    398           //    Failed third phase interpolation.
#define MIR_INTERP_FAILURE15                                                    399           //    Failed to sort input data.
#define MIR_INTERP_FAILURE16                                                    400           //    Failed to triangulate.
#define MIR_INTERP_FAILURE17                                                    401           //    Failed to focus source mask.
#define MIR_INTERP_FAILURE18                                                    402           //    Failed to export source mask.
#define MIR_INTERP_PARSE_AND_STAMP                                              403           //    Parse input and stamp.
#define MIR_INTERP_PARSE_AND_STAMP1                                             404           //    Stamping input data...
#define MIR_INTERP_PARSE_AND_STAMP2                                             405           //    Reading all input files and stamping to grid...
#define MIR_INTERP_PARSE_AND_STAMP3                                             406           //    Stamped a total of %1!%r! grid cells: %2!%.10g! percent.
#define MIR_INTERP_PARSE_AND_STAMP4                                             407           //    Normalizing stamped data...
#define MIR_INTERP_PARSE_AND_STAMP_SYSERROR                                     408           //    System error %1!%r! trying to allocate patch memory.
#define MIR_INTERP_FINALISE                                                     409           //    Final minimum curvature iteration.
#define MIR_INTERP_SORTING                                                      410           //    Sorting input data...
#define MIR_INTERP_SORTING1                                                     411           //    Reading all input files and spatially sorting data...
#define MIR_INTERP_PATCH_STATS1                                                 412           //    Patch statistics: %1!%r! occupied patches.
#define MIR_INTERP_PATCH_STATS2                                                 413           //    Patch statistics: Maximum points %1!%r!, minimum %2!%r!.
#define MIR_INTERP_PATCH_STATS3                                                 414           //    Patch statistics: Median points %1!%r!.
#define MIR_INTERP_FINAL_MINC                                                   415           //    Final minimum curvature iteration.
#define MIR_INTERP_PATCH_COLLAPSE                                               416           //    Collapsed patch from %1!%r! stations to %2!%r! unique stations.
#define MIR_INTERP_EXIT                                                         417           //    Exit
#define MIR_INTERP_FINAL_CHANGE                                                 418           //     Final change: %1!%.10g!.
#define MIR_INTERP_CACHE_FLUSH                                                  419           //    Flushing caches - there may a delay.
#define MIR_INTERP_TRIANGULATION                                                420           //    Triangulation.
#define MIR_INTERP_TRIANGULATION1                                               421           //    Triangulating...
#define MIR_INTERP_SMALL_CACHE                                                  422           //    Warning: Raster cache is small - performance may be affected.
#define MIR_INTERP_TRIANGULATION2                                               423           //    Incrementally triangulating the dataset and writing to output raster.
#define MIR_INTERP_PATCH_ROW                                                    424           //    Patch row %1!%r!:
#define MIR_INTERP_FULL_LOAD                                                    425           //     Fully loaded
#define MIR_INTERP_TRIANGULATION3                                               426           //    Patch triangulation attempt failed due to memory allocation failure. Trying again...
#define MIR_INTERP_UNLOAD                                                       427           //     Unloading 
#define MIR_INTERP_LOADED                                                       428           //    Patches loaded %1!%r!.
#define MIR_INTERP_PYRAMID                                                      429           //    Populate pyramid upwards.
#define MIR_INTERP_PYRAMID1                                                     430           //    Changing cache status. This can cause a short delay...
#define MIR_INTERP_INTERPOLATING                                                431           //    Interpolating %1!%r!% complete.
#define MIR_INTERP_CLIP                                                         432           //    Clip.
#define MIR_INTERP_CLIP_CREATE_MASK                                             433           //    Creating clip mask...
#define MIR_INTERP_CLIP_NEAR                                                    434           //    Near field search...
#define MIR_INTERP_CLIP_FAR                                                     435           //    Far field search...
#define MIR_INTERP_FOCUS                                                        436           //    Focus source mask.
#define MIR_INTERP_FOCUS1                                                       437           //    Focusing source mask...
#define MIR_INTERP_PYRAMID2                                                     438           //    Populate pyramid downwards.
#define MIR_INTERP_EXPORT1                                                      439           //    Exporting raster...
#define MIR_INTERP_EXPORT_HEADER                                                440           //    Writing raster header...
#define MIR_INTERP_EXPORT_HEADER1                                               441           //    Output header: %1!%r!.
#define MIR_INTERP_EXPORT_DATA                                                  442           //    Writing raster data...
#define MIR_INTERP_EXPORT2                                                      443           //    Output: %1!%r!.
#define MIR_INTERP_WRITE_TAB                                                    444           //    Writing TAB companion...
#define MIR_INTERP_WRITE_TAB1                                                   445           //    Output TAB companion: %1!%r!.
#define MIR_INTERP_COMMAND_FILE                                                 446           //    Input command file: %1!%r!.
#define MIR_INTERP_ESTIMATE                                                     447           //    Statistics: Estimating appropriate cell size.
#define MIR_INTERP_TERMINATED                                                   448           //    Terminated...
#define MIR_INTERP_PHASE1                                                       449           //    Interpolation phase 1...
#define MIR_INTERP_PHASE2                                                       450           //    Interpolation phase 2...
#define MIR_INTERP_PHASE3                                                       451           //    Interpolation phase 3...
#define MIR_INTERP_STAMP_COMPLETE                                               452           //    Stamping %1!%r!% complete.
#define MIR_INTERP_EXPORT_COMPLETE                                              453           //    Exporting %1!%r!% complete.
#define MIR_INTERP_OUTPUT_HEADER_FAILED                                         454           //    Output header: Failed to open for writing.
#define MIR_INTERP_OUTPUT_BIL_FAILED                                            455           //    Output BIL: Failed to open for writing.
#define MIR_INTERP_OUTPUT_TAB_FAILED                                            456           //    Output TAB companion: Failed to open for writing.
#define MIR_INTERP_OPEN_CREATE_FAILED                                           457           //    Could not create output file.
#define MIR_INTERP_ITERATOR_FAILED                                              458           //    Could not create iterator to write to output file.
#define MIR_INTERP_ERROR                                                        459           //    An error has occurred during interpolating a grid.
#define MIR_INTERP_TYPE1                                                        460           //    Gridding: Stamped minimum curvature.
#define MIR_INTERP_TYPE2                                                        461           //    Gridding: Minimum curvature.
#define MIR_INTERP_TYPE3                                                        462           //    Gridding: Delaunay triangulation with linear interpolation.
#define MIR_INTERP_TYPE4                                                        463           //    Gridding: Stamped inverse distance.
#define MIR_INTERP_TYPE5                                                        464           //    Gridding: Stamped data density.
#define MIR_INTERP_TYPE7                                                        465           //    Processing: Raster Padding.
#define MIR_INTERP_TYPE8                                                        466           //    Processing: Raster Unpadding.
#define MIR_INTERP_OUTPUTFILES                                                  467           //    Determining output file names.
#define MIR_INTERP_CLEAN                                                        468           //    Clearing memory and erasing temporary files...
#define MIR_INTERP_INVALID_TAB_INTERNAL                                         469           //    Invalid TAB format: %1!%r!.
#define MIR_INTERP_INVALID_INPUT_POINTS                                         470           //    There are no valid input points.
#define MIR_INTERP_JANUARY                                                      471           //    January
#define MIR_INTERP_FEBRUARY                                                     472           //    February
#define MIR_INTERP_MARCH                                                        473           //    March
#define MIR_INTERP_APRIL                                                        474           //    April
#define MIR_INTERP_MAY                                                          475           //    May
#define MIR_INTERP_JUNE                                                         476           //    June
#define MIR_INTERP_JULY                                                         477           //    July
#define MIR_INTERP_AUGUST                                                       478           //    August
#define MIR_INTERP_SEPTEMBER                                                    479           //    September
#define MIR_INTERP_OCTOBER                                                      480           //    October
#define MIR_INTERP_NOVEMBER                                                     481           //    November
#define MIR_INTERP_DECEMBER                                                     482           //    December
#define MIR_INTERP_MONDAY                                                       483           //    Monday
#define MIR_INTERP_TUESDAY                                                      484           //    Tuesday
#define MIR_INTERP_WEDNESDAY                                                    485           //    Wednesday
#define MIR_INTERP_THURSDAY                                                     486           //    Thursday
#define MIR_INTERP_FRIDAY                                                       487           //    Friday
#define MIR_INTERP_SATURDAY                                                     488           //    Saturday
#define MIR_INTERP_SUNDAY                                                       489           //    Sunday
#define MIR_INTERP_NULL_FIELD_BAND                                              948           //    Grid %1!%r! not written as it only contains null data.
#define MIR_INTERP_INVALID_TAB_NOT_MAPPABLE_INTERNAL                            949           //    Non mappable TAB file %1!%r! is not supported.
#define MIR_INTERP_INPUT_FILE_COUNT_ZERO                                        490           //    Input file count is zero.
#define MIR_INTERP_INPUT_DATAFORMAT_INVALID                                     491           //    Input file data format is invalid.
#define MIR_INTERP_INPUT_FILENAME_INVALID                                       492           //    Input file name is invalid.
#define MIR_INTERP_INPUT_ZIPFILE_INVALID                                        493           //    Input ZIP file does not contain a valid input file format.
#define MIR_INTERP_INPUT_DATAFIELD_COUNT_ZERO                                   494           //    No data columns have been selected in the Input file.
#define MIR_INTERP_OUTPUT_FILENAME_INVALID                                      495           //    Output file name is invalid.
#define MIR_INTERP_OUTPUT_RASTER_FORMAT_INVALID                                 496           //    Output file raster format is invalid.
#define MIR_INTERP_OUTPUT_DATATYPE_COUNT_ZERO                                   497           //    Output file defined data type count is zero.
#define MIR_INTERP_OUTPUT_DATATYPE_INVALID                                      498           //    Output file defined data type is invalid.
#define MIR_INTERP_OUTPUT_COORDSYS_INVALID                                      499           //    Output file coordinate system is invalid.
#define MIR_INTERP_GEOMETRY_CELLSIZE_INVALID                                    500           //    Geometry cell size is invalid, must be greater than 0.
#define MIR_INTERP_GEOMETRY_CELLCOUNT_INVALID                                   501           //    Geometry cell count is invalid, must be greater than 0.
#define MIR_INTERP_TRIANGULATION_PATCHMULTIPLIER_INVALID                        502           //    Triangulation patch multiplier is invalid.
#define MIR_INTERP_TRIANGULATION_LONGTRIANGLE_INVALID                           503           //    Triangulation longest triangle is invalid.
#define MIR_INTERP_IDW_RADIUSX_INVALID                                          504           //    Inverse Distance Weighted radius X is invalid.
#define MIR_INTERP_IDW_RADIUSY_INVALID                                          505           //    Inverse Distance Weighted radius Y is invalid.
#define MIR_INTERP_IDW_SEARCH_INCREMENT_INVALID                                 956           //    Inverse Distance Weighted search increment is invalid, valid values are 1 or greater.
#define MIR_INTERP_DISTANCE_RADIUSX_INVALID                                     506           //    Distance radius X is invalid.
#define MIR_INTERP_DISTANCE_RADIUSY_INVALID                                     507           //    Distance radius Y is invalid.
#define MIR_INTERP_DENSITY_RADIUSX_INVALID                                      508           //    Density radius X is invalid.
#define MIR_INTERP_DENSITY_RADIUSY_INVALID                                      509           //    Density radius Y is invalid.
#define MIR_INTERP_STAMP_METHOD_INVALID                                         510           //    Stamp method is invalid.
#define MIR_INTERP_MINCURV_RADIUS_INVALID                                       511           //    Minimum Curvature radius is invalid.
#define MIR_INTERP_CONTROL_FILE_INVALID                                         512           //    Control file name is invalid.
#define MIR_INTERP_X_INVALID                                                    513           //    The X column index is invalid.
#define MIR_INTERP_Y_INVALID                                                    514           //    The Y column index is invalid.
#define MIR_INTERP_GROUP_INVALID                                                515           //    The Group column index is invalid.
#define MIR_INTERP_X_Y_INVALID                                                  516           //    The X and Y column indexes are invalid, they cannot be the same.
#define MIR_INTERP_X_GROUP_INVALID                                              517           //    The X and Group column indexes are invalid, they cannot be the same.
#define MIR_INTERP_Y_GROUP_INVALID                                              518           //    The Y and Group column indexes are invalid, they cannot be the same.
#define MIR_INTERP_DATA_INVALID                                                 519           //    The Data column index is invalid.
#define MIR_INTERP_X_DATA_INVALID                                               520           //    The X and Data column indexes are invalid, they cannot be the same.
#define MIR_INTERP_Y_DATA_INVALID                                               521           //    The Y and Data column indexes are invalid, they cannot be the same.
#define MIR_INTERP_GROUP_DATA_INVALID                                           522           //    The Group and Data column indexes are invalid, they cannot be the same.
#define MIR_INTERP_DATA_INDEX_COUNT_INVALID                                     523           //    The data column indexes are invalid. The input file and data index count mismatched, they need to be the same.
#define MIR_INTERP_NEARFAR_LESS_THAN_ONE                                        524           //    Near or Far clipping parameter invalid, must be 1 or greater.
#define MIR_INTERP_INVALID_ASCII_FILE                                           525           //    Invalid ASCII file format.
#define MIR_INTERP_COORDINATE_CONDITIONING_INVALID                              526           //    Coordinate conditioning parameters are invalid.
#define MIR_INTERP_DATA_CONDITIONING_INVALID                                    527           //    Data conditioning parameters are invalid.
#define MIR_INTERP_LASZIP_DLL_NOT_LOADED                                        528           //    LASZip DLL was not loaded.
#define MIR_INTERP_LASZIP_ERROR_READING_FILE                                    529           //    An error occurred while reading the LASZip file.
#define MIR_INTERP_NN_SEARCH_DISTANCE_INVALID                                   530           //    Nearest Neighbour maximum search distance should be greater than 0.
#define MIR_INTERP_NNI_SEARCH_DISTANCE_INVALID                                  531           //    Natural Neighbour Integration maximum search distance should be greater than 0.
#define MIR_INTERP_NNI_GAUSSIAN_RANGE_INVALID                                   532           //    Natural Neighbour Integration Gaussian range should be greater than 0.
#define MIR_INTERP_INVALID_LAS                                                  533           //    Invalid LAS format header signature, file needs to be of type LASF.
#define MIR_INTERP_INVALID_TAB                                                  534           //    Invalid TAB format.
#define MIR_INTERP_NEARFAR_LESS_THAN_ZERO                                       535           //    Near or Far clipping parameter invalid, must be greater than 0.
#define MIR_INTERP_NEAR_GREATER_THAN_FAR                                        536           //    Near or Far clipping parameter invalid, Near must be less than Far.
#define MIR_INTERP_SMOOTHING_INVALID                                            537           //    The Smoothing parameter is invalid.
#define MIR_INTERP_PATCH_SIZE_INVALID                                           538           //    Patch size for triangulation exceeds 44 million points, reduce cell size and try again.
#define MIR_INTERP_DATA_TYPE_FOR_MIN_MAX_INVALID                                539           //    Column data type is invalid for minimum and maximum comparisons.
#define MIR_INTERP_POLYGONFILE_INVALID                                          540           //    The supplied polygon file for clipping is invalid.
#define MIR_INTERP_CLIPPING_METHOD_INVALID                                      541           //    The clipping method supplied is invalid for this interpolation method.
#define MIR_INTERP_INVALID_TAB_NOT_MAPPABLE                                     950           //    Non-mappable TAB files are not supported.
#define MIR_INTERP_IDW_SECTOR_COUNT_INVALID                                     951           //    Inverse Distance Weighted sector count is invalid, valid values are 1 to 32.
#define MIR_INTERP_IDW_MIN_POINTS_INVALID                                       952           //    Inverse Distance Weighted sector minimum points is invalid, valid values are 1 and greater.
#define MIR_INTERP_IDW_MAX_POINTS_INVALID                                       953           //    Inverse Distance Weighted sector maximum points is invalid, valid values are 1 and greater.
#define MIR_INTERP_IDW_MIN_SECTORS_INVALID                                      954           //    Inverse Distance Weighted sector minimum valid count is invalid, valid values are 1 and greater.
#define MIR_INTERP_IDW_SECTOR_ORIENTATION_INVALID                               955           //    Inverse Distance Weighted sector orientation is invalid, valid values are 0 to 360.
#define MIR_INTERP_LIDAR_CLASS_0                                                927           //    Created, never classified
#define MIR_INTERP_LIDAR_CLASS_1                                                928           //    Unclassified
#define MIR_INTERP_LIDAR_CLASS_2                                                929           //    Ground
#define MIR_INTERP_LIDAR_CLASS_3                                                930           //    Low Vegetation
#define MIR_INTERP_LIDAR_CLASS_4                                                931           //    Medium Vegetation
#define MIR_INTERP_LIDAR_CLASS_5                                                932           //    High Vegetation
#define MIR_INTERP_LIDAR_CLASS_6                                                933           //    Building
#define MIR_INTERP_LIDAR_CLASS_7                                                934           //    Low Point (Low noise)
#define MIR_INTERP_LIDAR_CLASS_8                                                935           //    Model Key/Reserved
#define MIR_INTERP_LIDAR_CLASS_9                                                936           //    Water
#define MIR_INTERP_LIDAR_CLASS_10                                               937           //    Rail
#define MIR_INTERP_LIDAR_CLASS_11                                               938           //    Road Surface
#define MIR_INTERP_LIDAR_CLASS_12                                               939           //    Bridge Deck
#define MIR_INTERP_LIDAR_CLASS_13                                               940           //    Wire - Guard (Shield)
#define MIR_INTERP_LIDAR_CLASS_14                                               941           //    Wire - Conductor (Phase)
#define MIR_INTERP_LIDAR_CLASS_15                                               942           //    Transmission Tower
#define MIR_INTERP_LIDAR_CLASS_16                                               943           //    Wire-structure Connector (Insulator)
#define MIR_INTERP_LIDAR_CLASS_17                                               944           //    Bridge
#define MIR_INTERP_LIDAR_CLASS_18                                               945           //    High Point (High noise)
#define MIR_INTERP_LIDAR_CLASS_19_63                                            946           //    Reserved for ASPRS Definition
#define MIR_INTERP_LIDAR_CLASS_64_255                                           947           //    User defined
#define MIR_CONTOUR_ELAPSED                                                     542           //    Elapsed time: %1!%r! s
#define MIR_CONTOUR_VERIFY_INPUT                                                543           //    Verifying input data sources...
#define MIR_CONTOUR_NO_TEMP                                                     544           //    Error: The temporary file folder %1!%r! does not exist.
#define MIR_CONTOUR_COORDSYS_FAILURE                                            545           //    Failed to assign coordinate system to output.
#define MIR_CONTOUR_MINIMUM_SUPPORT                                             546           //    Minimum contour level could not be supported, modified from %1!%r! to %2!%r!.
#define MIR_CONTOUR_MAXIMUM_SUPPORT                                             547           //    Maximum contour level could not be supported, modified from %1!%r! to %2!%r!.
#define MIR_CONTOUR_PREPARE2                                                    548           //    Preparing to contour...
#define MIR_PLYGISE_GENERATING                                                  549           //    Generating polygons...
#define MIR_CONTOUR_INVALID_EXTENTS                                             550           //    Contouring extents are invalid.
#define MIR_CONTOUR_REGION1                                                     551           //    Contouring region from (%1!%r!, %2!%r!) to (%3!%r!, %4!%r!)
#define MIR_CONTOUR_REGION2                                                     552           //    Contouring region over (%1!%r!, %2!%r!) cells of dimension (%3!%r!, %4!%r!)
#define MIR_CONTOUR_PHYSICAL_LOW2                                               553           //    Warning: The available physical memory is low - performance may be affected.
#define MIR_CONTOUR_2                                                           554           //    Generating contours...
#define MIR_CONTOUR_FINALISED                                                   555           //    Finalized %1!%r! contours with %2!%r! in progress and %3!%r! deferred...
#define MIR_PLYGISE_INPROGRESS                                                  556           //    Finalized %1!%r! polygons with %2!%r! in progress and %3!%r! deferred...
#define MIR_CONTOUR_FINALISE_2                                                  557           //    Finalizing contours...
#define MIR_PLYGISE_FINALISE                                                    558           //    Finalising polygons...
#define MIR_CONTOUR_EXPORT_2                                                    559           //    Exporting contours...
#define MIR_PLYGISE_EXPORT                                                      560           //    Exporting polygons...
#define MIR_CONTOUR_EXPORT_3                                                    561           //    Exporting a total of %1!%r! contours.
#define MIR_CONTOUR_EXPORT_MULT_1                                               562           //    Multiple output contour files will be generated.
#define MIR_CONTOUR_EXPORT_MULT_2                                               563           //    A total of %1!%r! output contour files will be written.
#define MIR_CONTOUR_EXPORT_OUTPUT                                               564           //    Created output file %1!%r! (%2!%r! lines and %3!%r! points).
#define MIR_CONTOUR_GENERATE_1                                                  565           //    Loaded command file: %1!%r!.
#define MIR_CONTOUR_GENERATE_FAIL0                                              566           //    No command line control file argument found.
#define MIR_CONTOUR_GENERATE_FAIL1                                              567           //    Failed to read supplied command file.
#define MIR_CONTOUR_GENERATE_FAIL2                                              568           //    Failed to verify input raster file.
#define MIR_CONTOUR_GENERATE_FAIL3                                              569           //    Failed to initialize operation.
#define MIR_CONTOUR_GENERATE_FAIL4                                              570           //    Failed to execute operation.
#define MIR_CONTOUR_GENERATE_FAIL5                                              571           //    Failed to finalize operation.
#define MIR_CONTOUR_GENERATE_FAIL6                                              572           //    Failed to export.
#define MIR_CONTOUR_REMOVE_DUPLICATE                                            573           //    Removing duplicate manual level %1!%r!
#define MIR_CONTOUR_AREA_REMOVED                                                574           //    Number of contours removed under %1!%f! in size = %2!%r!
#define MIR_CONTOUR_NO_INPUT_FILE                                               575           //    Unable to open input file for contouring.
#define MIR_MINCONTOUR_GREATERTHAN_MAXCONTOUR                                   576           //    The minimum contour value is greater than the maximum contour value.
#define MIR_CONTOUR_COUNT_ZERO                                                  577           //    Zero contours have been generated, output file will not be created.
#define MIR_CONTOUR_NO_MANUAL_LEVELS                                            578           //    Level Type is set to manual but no manual levels have been provided.
#define MIR_CONTOUR_RASTER_OPEN_FAILURE                                         579           //    Failed to open raster.
#define MIR_CONTOUR_STATS_FAILURE                                               580           //    Failed to acquire raster statistics.
#define MIR_CONTOUR_TAB_FAILURE                                                 581           //    TAB output failure.
#define MIR_CONTOUR_INVALID_SPACING                                             582           //    Contour spacing cannot be a negative value.
#define MIR_CONTOUR_INVALID_LINESTYLE_NONE                                      583           //    Contour line style pattern cannot be set to PAT_NONE or a negative value.
#define MIR_CONTOUR_INVALID_MAJORSTEP                                           584           //    Major step cannot be a negative value.
#define MIR_CONTOUR_INVALID_BRUSHSTYLE_NONE                                     585           //    Contour brush style pattern cannot be set to a negative value.
#define MIR_CONTOUR_INVALID_MIN_AREA                                            586           //    Minimum defined contour area cannot be negative.
#define MIR_CONTOUR_DUPLICATE_LEVEL                                             587           //    A duplicate contour level has been found, please remove any duplicated levels.
#define MIR_CONTOUR_INVALID_LINEWIDTH                                           588           //    Contour line width can only be set to 0 if line style pattern is set to PAT_HOLLOW.
#define MIR_CONTOUR_NOMEM                                                       589           //    Memory failure occurred during contouring.
#define MIR_CONTOUR_ERROR                                                       590           //    An error has occurred during contour processing.
#define MIR_POLY_ERROR                                                          591           //    An error has occurred during polygon processing.
#define MIR_CONTOUR_LOWERBOUND                                                  592           //    Lower_Bound
#define MIR_CONTOUR_UPPERBOUND                                                  593           //    Upper_Bound
#define MIR_CONTOUR_LEVEL                                                       594           //    Level
#define MIR_CONTOUR_INDEX                                                       595           //    Index
#define MIR_CONTOUR_CLASS                                                       596           //    Class
#define MIR_CONTOUR_VALUE                                                       597           //    Value
#define MIR_CONTOUR_LABEL                                                       598           //    Label
#define MIR_CLASSLBL_FIELD_NOT_FOUND                                            599           //    Class Label field not found in class table.
#define MIR_CLASSID_FIELD_NOT_FOUND                                             600           //    Class ID field not found in class table.
#define MIR_COLOUR_PALETTE_NOTFOUND                                             601           //    Failed to find color palette directory.
#define MIR_LEGEND_GHX_MISSING                                                  602           //    There is no GHX file for this Raster.
#define MIR_ERR_FIELD_TYPE_NOT_ALLOWED                                          603           //    Field type is not allowed by the driver.
#define MIR_ERR_LEGEND_INVALID_RENDER_STYLE                                     604           //    Legend not supported for this render style.
#define MIR_LEGEND_NO_INFLECTION_DATA                                           605           //    There is no inflection data for this raster.
#define MIR_WRITE_LEGEND_FAIL                                                   606           //    Failed to write legend file.
#define MIR_NORELEASE                                                           607           //    Unable to release memory.
#define MIR_TRACKER_CREATE_FAIL                                                 608           //    Memory allocation failure. Could not create a process tracker object.
#define MIR_FILE_ABSPATH_ERROR                                                  609           //    Absolute path error.
#define MIR_FILE_INVALID_EXTN                                                   610           //    Invalid file extension.
#define MIR_LOCATION_OFF_GRID                                                   611           //    Location provided is outside the raster extents.
#define MIR_INVALID_COMPRESSION                                                 612           //    An invalid compression method has been provided.
#define MIR_INVALID_FINALISE_ARGS                                               613           //    Invalid finalization option.
#define MIR_READ_CONFIG_FAIL                                                    614           //    Failed to read configuration file.
#define MIR_FAIL_EXTRACT_FNAME                                                  615           //    Unable to extract file name from the given file.
#define MIR_BEGIN_INTERP_FAILED                                                 616           //    Unable to start interpolator.
#define MIR_TABLE_UNAVAILABLE                                                   617           //    Invalid input table.
#define MIR_INVALID_RECORD                                                      618           //    An invalid table record index has been passed.
#define MIR_EDIT_PROPERTY_NOT_SUPPORTED                                         619           //    Editing the raster property is not supported.
#define MIR_EDIT_PROPERTY_INVALID_VALUE                                         620           //    Invalid property value.
#define MIR_EDIT_PROPERTY_NOT_FOUND                                             621           //    No property found to edit.
#define MIR_CONTOUR_INVALID_MANUAL_LEVEL_NULL                                   622           //    Number of manual levels is greater than zero but the manual level array has not been initialized.
#define MIR_INVALID_FIELD_BAND_FILTER                                           623           //    Invalid Field and Band filter for current operation.
#define MIR_CONTOUR_SUGGESTED_REGION_SIZE                                       624           //    Sub-region size is too small. Suggested sub-region size is X=%1!%r! Y=%2!%r!.
#define MIR_CONTOUR_REGION_SIZE_TOO_SMALL                                       625           //    Sub-region size is too small, increase the sub-region size.
#define MIR_CONTOUR_CELLCOUNT_ZERO                                              626           //    Cell count to contour is zero.
#define MIR_CONTOUR_BLOCK_ERROR                                                 627           //    Block %1!%r! error: %2!%r!
#define MIR_CONTOUR_BLOCK_PROCESS                                               628           //    Processing block %1!%r! of %2!%r! for cell indexes (%3!%r!,%4!%r! to %5!%r!,%6!%r!) %7!%r!
#define MIR_CONTOUR_CREATING_SEAMLESS                                           629           //    Creating seamless table: %1!%r!
#define MIR_CONTOUR_SEAMLESSTAB_FAILURE                                         630           //    Seamless TAB output failure.
#define MIR_CONTOUR_CREATED_SEAMLESS                                            631           //    Created seamless table: %1!%r!
#define MIR_RESOLUTION_RANGE_UNAVAILABLE                                        632           //    Unable to retrieve resolution range for the raster file.
#define MIR_FAIL_EPSG_NOTFOUND                                                  633           //    EPSG code matching MapInfo projection string not found.
#define MIR_StatisticsFail                                                      634           //    Statistics failure.
#define MIR_CLIPRECT_NO_OVERLAP                                                 635           //    No overlap found between raster and clip extent.
#define MIR_DRIVER_NO_MULTI_FIELD_SUPPORT                                       636           //    Selected driver does not provide Multi-field support for output.
#define MIR_ALL_BANDS_SAME_TYPE                                                 637           //    Field type must match for all input rasters selected.
#define MIR_CONTOUR_FIELD_INVALID                                               638           //    Specified input Field does not exist.
#define MIR_CONTOUR_BAND_INVALID                                                639           //    Specified input Band does not exist.
#define MIR_CONTOUR_VERIFY_INPUT_FAILURE                                        640           //    Verify input file failure.
#define MIR_CONTOUR_NO_OUTPUT_FOLDER                                            641           //    The output grid file folder does not exist.
#define MIR_CONTOUR_LOW_MEMORY                                                  642           //    Contouring is experiencing low memory. Adjust parameters and try again.
#define MIR_FAIL_WRITE_STRUCTURE                                                643           //    Unable to write structure.
#define MIR_SAME_RASTER_TAB                                                     644           //    Input Raster's TAB file and Output Vector TAB file are at same location with same Name.
#define MIR_FILTER_ONLY_SUPPORTS_ODD_KERNEL_ROW_COL                             645           //    Filter only supports odd numbers of rows and columns in the kernel.
#define MIR_CLASSIFIED_FILTER_MIN_REGION_SIZE_TOO_LARGE                         646           //    The minimum region size for the classified filter is too large.
#define MIR_NO_POINT_OBJECTS_FOUND                                              647           //    No point objects found in Input vector file.
#define MIR_ONLY_CONTINOUS_CLASSIFIED_SUPPORTED                                 648           //    Field type is not valid, only Continuous and Classified fields allowed.
#define MIR_IMPORT_DRIVER_NO_MULTI_BAND_SUPPORT                                 649           //    Selected Driver doesn't provide Multi-band Support to Import.
#define MIR_FILE_ACCESS_ERROR                                                   650           //    The file is not present or the user does not have access permission.
#define MIR_SAME_SOURCE_DESTINATION                                             651           //    Input and output file names are same.
#define MIR_INVALID_VECTOR_FILE                                                 652           //    Invalid vector file.
#define MIR_INVALID_CELLSIZE                                                    653           //    Invalid cell size.
#define MIR_SOURCE_ORIGIN_NOT_VALID_CELL                                        654           //    Source origin not over a valid cell, so height cannot be determined.
#define MIR_DEST_ORIGIN_NOT_VALID_CELL                                          655           //    Destination origin not over a valid cell, so height cannot be determined.
#define MIR_MERGE_INCOMPATIBLE_GRIDS                                            656           //    Input rasters are not compatible for merge operation.
#define MIR_MRT_NOT_SUPPORTED                                                   657           //    Multi-resolution output is not supported by selected output driver.
#define MIR_FAIL_SET_TABLE_RECORD                                               658           //    Unable to set classification table record.
#define MIR_INVALID_BANDED_AND_NONBANDED_COMBINATION                            659           //    You cannot mix banded and non-banded notation for the same raster/field in an expression.
#define MIR_RASTER_FORMAT_ONLY_SINGLE_BANDED                                    660           //    Raster format does not support multiple bands.
#define MIR_START_FILE_OPEN_S                                                   661           //    Opening file %1!%r!.
#define MIR_CLASSIFIED_OUTPUT_NOT_VALID                                         662           //    Classified raster output is not supported for the specified parameters.
#define MIR_GRIDS_DO_NOT_OVERLAP                                                663           //    Source rasters do not overlap.
#define MIR_CLASSIFIED_RASTERS_CAN_NOT_HAVE_MULTIPLE_BANDS                      664           //    Cannot create a classified raster with multiple bands.
#define MIR_INVALID_MULTIBAND_CONTINUOUS_TO_IMAGE_INVALID                       665           //    You cannot convert a multi-band continuous raster directly to an image raster. Try using the individual bands.
#define MIR_TOO_MANY_CALCULATOR_EXPRESSIONS                                     666           //    You cannot have more calculator expressions than there are bands.
#define MIR_WRONG_NUMBER_OF_CALCULATOR_EXPRESSIONS                              667           //    There must either be one expression or one expression for each band.
#define MIR_CALCULATING                                                         668           //    Calculate operation started.
#define MIR_BEGIN_INTERPOLATE_FAILED                                            669           //    Failure while creating interpolator.
#define MIR_CALCULATING_COMPLETE_S                                              670           //    Calculating %1!%r!% complete.
#define MIR_CREATING_FILE_S                                                     671           //    Creating output file %1!%r!.
#define MIR_CALCULATE_COMPLETED                                                 672           //    Calculate operation completed successfully.
#define MIR_VALID_EXPRESSION                                                    673           //    Valid expression.
#define MIR_TOO_MANY_RASTER_ALIASES                                             674           //    Too many raster aliases.
#define MIR_OPERATION_STARTED_S                                                 675           //    %1!%r! operation started.
#define MIR_CLIP_RASTER_TO_RASTER_OPERATION_NAME                                676           //    Clip raster to raster
#define MIR_OPERATION_ENDED_S                                                   677           //    %1!%r! operation ended.
#define MIR_CORDSYS_UNAVAILABLE                                                 678           //    Coordinate system is unavailable.
#define MIR_CLIP_STARTED                                                        679           //    Clip operation started.
#define MIR_INPUT_POLY_BOUNDS                                                   680           //    Input polygon world space clip bounds MinX: %1!%g! MaxX: %2!%g! MinY: %3!%g! MaxY: %4!%g!.
#define MIR_INPUT_CELL_BOUNDS                                                   681           //    Input polygon Cell space clip bounds MinX: %1!%r! MaxX: %2!%r! MinY: %3!%r! MaxY: %4!%r!.
#define MIR_INPUT_POLY_RETAIN                                                   682           //    Retain Inside Polygon: %1!%r!.
#define MIR_CLIP_COMPLETED                                                      683           //    Clip operation completed successfully.
#define MIR_VEC_FEATURE_NOTFOUND                                                684           //    No supported vector feature found.
#define MIR_TPIP_FAIL_ALLOCATE                                                  685           //    TPIP error, failed to allocate memory.
#define MIR_TPIP_FAIL_PROCESS                                                   686           //    TPIP processing failed.
#define MIR_SET_RANDOM_BLOCK_FAILED                                             687           //    Failed to set Random Block.
#define MIR_GET_RANDOM_BLOCK_FAILED                                             688           //    Failed to get Random Block.
#define MIR_CLIP_COMPLETE_S                                                     689           //    Clipping %1!%r!% complete.
#define MIR_DRIVER_NO_MULTI_BAND_SUPPORT                                        690           //    Selected driver does not provide Multi-band support to output.
#define MIR_ALL_FIELDS_PROJECTION_NOT_MATCHING                                  691           //    Projection must match for all selected rasters.
#define MIR_ERR_MULTIPLE_NONCONTINUOUS                                          692           //    Multiple non-continuous fields are not allowed in the same field.
#define MIR_ALL_BANDS_CELLSIZE_MUST_MATCH                                       693           //    Field Cell size must match for all input rasters selected.
#define MIR_ALL_BANDS_TILEORIGIN_MUST_MATCH                                     694           //    Field Origin must match for all input rasters selected.
#define MIR_POPULATEDATA_FIELDID_D                                              695           //    Populating data into destination raster fieldIndex: %1!%r!.
#define MIR_MATCH_INPUT_GEOMTRY                                                 696           //    Verifying compatibility of all input raster's geometry.
#define MIR_COMBINE_STARTED                                                     697           //    Combine operation Started.
#define MIR_POPULATERASTER_DATA                                                 698           //    Populating data in destination raster.
#define MIR_WRITING_OUTPUT_RASTER_S                                             699           //    Start creating %1!%r! output raster file.
#define MIR_COMBINE_COMPLETED                                                   700           //    Combine operation completed successfully.
#define MIR_DIRECTORY_NOT_WRITEABLE                                             701           //    Directory is not writable.
#define MIR_COPY_RASTER                                                         702           //    Copy Operation Started.
#define MIR_COPYING_FILE_S                                                      703           //    Copying %1!%r! and associated files.
#define MIR_COPY_FAIL                                                           704           //    Failed to copy file.
#define MIR_COPY_COMPLETED                                                      705           //    Copy operation completed successfully.
#define MIR_DELETE_START                                                        706           //    Delete operation started.
#define MIR_DELETE_RASTER                                                       707           //    Deleting raster file...
#define MIR_DELETING_FILE_S                                                     708           //    Deleting %1!%r! and associated files.
#define MIR_DELETE_FAIL1                                                        709           //    Not able to delete file %1!%r! error code: %2!%r!.
#define MIR_DELETE_FAIL2                                                        710           //    Not able to delete file %1!s !with extension %2!%r! from Destination Directory %3!%r!.
#define MIR_DELETE_ASSOCIATED                                                   711           //    Deleting associated files...
#define MIR_DELETE_FAIL3                                                        712           //    Not able to delete file %1!%r! with extension %2!%r! error code: %3!%r!.
#define MIR_DELETE_COMPLETED                                                    713           //    Delete operation completed successfully.
#define MIR_RENAME_START                                                        714           //    Rename operation started.
#define MIR_RENAME_FAIL1                                                        715           //    Not able to rename file %1!%r! to %2!%r!.
#define MIR_NOT_SUPPORTED                                                       716           //    Not a supported operation.
#define MIR_RENAMING_FILE_S                                                     717           //    Renaming %1!%r! and associated files.
#define MIR_RENAME_FAIL2                                                        718           //    Not able to rename file %1!%r! with extension %2!%r! to %3!%r!.
#define MIR_RENAME_COMPLETED                                                    719           //    Rename operation completed successfully.
#define MIR_EXPORT_START                                                        720           //    Export operation started.
#define MIR_OUTPUT_EXISTS2                                                      721           //    Output File %1!%r! exists, exiting as overwrite flag is false.
#define MIR_EXPORT_STARTED                                                      722           //    Grid Export operation started.
#define MIR_EXPORT_COMPLETED                                                    723           //    Grid Export operation completed successfully.
#define MIR_INVALID_DELIMITER                                                   724           //    An invalid delimiter (like '.') has been provided.
#define MIR_EXPORTTOASCII_COMPLETE_S                                            725           //    Export to ASCII %1!%r!% complete.
#define MIR_EXPORTTOTAB_COMPLETED                                               726           //    Export to TAB file operation is completed.
#define MIR_FAIL_COLOR_INFO                                                     727           //    Failed to get Color Information for this Raster.
#define MIR_FILE_NOT_EXIST                                                      728           //    File does not exist.
#define MIR_ELLIS_INIT_FAIL                                                     729           //    Ellis Initialization failed.
#define MIR_COORDSYS_NOT_PRESENT                                                730           //    CoordSys not present in input file, Using CoordSys supplied by user.
#define MIR_EXPORTTOTAB_MEM_FAIL                                                731           //    Operation may fail because of insufficient memory to Run.
#define MIR_NATIVE_TAB_LIMIT                                                    732           //    TAB file creation will fail because of size restriction. Setting it to Extended TAB format.
#define MIR_TAB_METADATA_FAIL                                                   733           //    Failed to set Table Meta Data.
#define MIR_CREATE_TAB_FAIL                                                     734           //    Failed to create the TAB file.
#define MIR_INSERT_TAB_FAIL                                                     735           //    Failed to insert object into the TAB file.
#define MIR_EXPORTTOTAB_COMPLETE_S                                              736           //    Export to TAB %1!%r!% complete.
#define MIR_INVALID_OUTPUT_FIELD_TYPE                                           737           //    Invalid output field type.
#define MIR_FILTERING                                                           738           //    Filter operation started.
#define MIR_FILTERING_COMPLETE_S                                                739           //    Filtering %1!%r!% complete.
#define MIR_FILTER_COMPLETED                                                    740           //    Filter operation completed successfully.
#define MIR_FAIL_READ_KERNEL_FILE                                               741           //    Failed to process input kernel file.
#define MIR_INVALID_EXPORT_FILE                                                 742           //    Input file is not a valid exported file. Please add the standard header before proceeding.
#define MIR_INVALID_EXPORT_FILE_VERSION                                         743           //    Input file is not a valid exported file. Please add the standard header version before proceeding.
#define MIR_INVALID_RASTER_SIZE                                                 744           //    Input file does not have valid rows and columns in header.
#define MIR_IMPORT_START                                                        745           //    Import operation started.
#define MIR_IMPORT_DRIVER_NO_MULTI_FIELD_SUPPORT                                746           //    Selected Driver doesn't provide Multi-field Support to Import.
#define MIR_IMPORT_STARTED                                                      747           //    Grid Import operation started.
#define MIR_IMPORT_COMPLETE_S                                                   748           //    Importing %1!%r!% complete.
#define MIR_IMPORT_COMPLETED                                                    749           //    Grid Import operation completed successfully.
#define MIR_COMP_STATS                                                          750           //    GetComputeStats: 
#define MIR_COMP_STAT_VAL                                                       751           //    Val: %1!%f! IsValid: %2!%r! 
#define MIR_INVALID_ARG_REQD_STAT_MISSING                                       752           //    Select at least one statistics attribute.
#define MIR_UNDEFINED_UNITCODE                                                  753           //    Input raster's unit code is not defined.
#define MIR_WARN_RASTER_COORDSYS                                                754           //    Warning! coordinate system is not present in the raster file %1!%r!, assuming its coordinate system is the same as the input vector file.
#define MIR_COMP_POINTS                                                         755           //    GridPointsValues (x,y) 
#define MIR_COMP_POINTS_VAL                                                     756           //    Point[%1!%g!, %2!%g!] 
#define MIR_LINE_INSPECTION_START                                               757           //    Line Inspection operation started.
#define MIR_START_READ_VECTOR_FILE_S                                            758           //    Start reading %1!%r! vector file.
#define MIR_READFAIL                                                            759           //    Unable to read file.
#define MIR_END_READ_VECTOR_FILE                                                760           //    Vector file reading completed.
#define MIR_NO_LINE_OBJECTS_FOUND                                               761           //    No line objects found in input vector file.
#define MIR_N_VECTOR_OBJECTS_FOUND_D_S                                          762           //    %1!%r! %2!%r! objects found in input vector file.
#define MIR_BEGIN_LINE_INSPECTION                                               763           //    Line Inspection started for %1!%r!.
#define MIR_END_LINE_INSPECTION                                                 764           //    Line inspection completed for %1!%r!.
#define MIR_OUTPUT_VECTOR_WRITE_FAIL                                            765           //    Unable to create output Tab file.
#define MIR_LINE_INSPECTION_COMPLETED                                           766           //    Line Inspection operation completed successfully.
#define MIR_NONSPATIAL_UNITCODE                                                 767           //    Unit code is not a valid spatial unit code.
#define MIR_SAMPLE_POINT_GENERATE_D                                             768           //    Generating %1!%r! sample points between view points.
#define MIR_LINEOFSIGHT_OPERATION_NAME                                          769           //    Line of sight
#define MIR_OPERATION_COMPLETE_S                                                770           //    %1!%r! operation completed successfully.
#define MIR_FAIL_READ_STRUCTURE                                                 771           //    Unable to read structure.
#define MIR_FAIL_RELEASE_RESOURCE                                               772           //    Failed to release the resource.
#define MIR_MERGE_PREPROC_INPUT_GRIDS                                           773           //    Pre-processing input rasters to merge...
#define MIR_MERGE_PREPROC_S                                                     774           //    Pre-processing raster %1!%r!
#define MIR_MERGE_INCOMPATIBLE_FIELDTYPE_S                                      775           //    Incompatible Field type detected in the source raster %1!%r!.
#define MIR_MERGE_INCOMPATIBLE_BANDCOUNT_S                                      776           //    Incompatible Band count detected in the source raster %1!%r!.
#define MIR_MERGE_INCOMPATIBLE_CORDSYS_S                                        777           //    Incompatible coordinate system detected in the source raster %1!%r!.
#define MIR_MERGE_MRT_NONSQUARE_CELL_S                                          778           //    Non-square cells are not supported for multi-resolution output. Choose a source raster with square cells %1!%r!.
#define MIR_MERGE_PREPROC_UPDATE_S                                              779           //    Merge pre-processing %1!%r!% complete.
#define MIR_MERGERECT_NO_OVERLAP                                                780           //    No overlap found between merge rectangle and the union of input rasters' bound.
#define MIR_MERGE_PREPROC_COMPLETE                                              781           //    Merge pre-processing of input rasters completed.
#define MIR_MERGE_STARTED                                                       782           //    Merge operation started.
#define MIR_MERGE_PROC_UPDATE_S                                                 783           //    Merge data processing %1!%r!% completed.
#define MIR_MERGE_COMPLETED                                                     784           //    Merge operation completed successfully.
#define MIR_GET_INTERP_BLOCK_FAILED                                             785           //    Failed to interpolate data block.
#define MIR_CSYS_NULL                                                           786           //    CSYS is Null in Input vector and raster.
#define MIR_DUP_COLNAME                                                         787           //    Column name should be unique. %1!%r! column name exists in input TAB or input column list
#define MIR_BAD_COLNAME                                                         788           //    Column name:%1!%r!, doesn't exists in input TAB
#define MIR_INVALID_COLNAME                                                     789           //    Invalid Column name: %1!%r!. Column name should not be more than 30 char with no spaces and special char.
#define MIR_COL_DATATYPE_NOT_ALLOWED                                            790           //    %1!%r! column data type is incompatible with band data type or the null/no cell values
#define MIR_VECTOR_MODIFY_FAIL                                                  791           //    Vector file modification failure.
#define MIR_COLINFO_NULL                                                        792           //    Output TAB column info is Null
#define MIR_MISSING_COLINFO                                                     793           //    Missing Column info. Output TAB column info should be given for all input raster
#define MIR_INVALID_DEST                                                        794           //    Incorrect destination TAB path
#define MIR_POINT_INSPECTION_START                                              795           //    Point Inspection operation started.
#define MIR_XYBLOCK_COUNT_D_D                                                   796           //    Running operation on total %1!%r! XBlocks %2!%r! YBlocks.
#define MIR_BLOCK_SIZE_G_G_G_G                                                  797           //    Current block size dMinX=%1!%g! dMaxX=%2!%g! dMinY=%3!%g! dMaxY=%4!%g!.
#define MIR_READ_OBJECTS_VECTOR_S                                               798           //    Reading objects from input vector file %1!%r!.
#define MIR_READ_CELLVALUE_RASTER_VECTOR_S                                      799           //    Reading cell values from input raster %1!%r!.
#define MIR_MODIFY_RECORDS_VECTOR                                               800           //    Modifying records in output vector.
#define MIR_POINT_INSPECTION_COMPLETED                                          801           //    Point Inspection operation completed successfully.
#define MIR_PROCESSING_TASK1                                                    802           //    Input source file path: %1!%r!.
#define MIR_PROCESSING_TASK2                                                    803           //    Output file path: %1!%r!.
#define MIR_PROCESSING_TASK3                                                    804           //    Destination driver string: %1!%r!.
#define MIR_PROCESSING_TASK4                                                    805           //    Finalize options delete: %1!%r! Discard: %2!%r! BuildOverViews: %3!%r! ComputeStats %4!%r!, StatsLevel %5!%r!.
#define MIR_PROCESSING_TASK5                                                    806           //    Operation going to be performed: %1!%r!.
#define MIR_REPROJECT_ERR_INPUT_COORDSYS_NOT_FOUND                              807           //    Input raster can not be re-projected, no coordinate system found in input raster.
#define MIR_INVALID_PROJECTION_COMBINATION                                      808           //    The selected projection/reprojection combination is not valid for this data.
#define MIR_REPROJECT1                                                          809           //    Source raster CoordSys: [%1!%r!].
#define MIR_REPROJECT2                                                          810           //    Destination raster requested CoordSys: [%1!%r!].
#define MIR_REPROJECT3                                                          811           //    OldMinX: %1!%g! OldMinY: %2!%g! OldMaxX: %3!%g! OldMaxY: %4!%g!.
#define MIR_REPROJECT4                                                          812           //    NewCellSize: %1!%g! nNewColumns: %2!%r! nNewRows: %3!%r! NewMinX: %4!%g! NewMinY: %5!%g! NewMaxX: %6!%g! NewMaxY: %7!%g!.
#define MIR_REPROJECT_STARTING                                                  813           //    Reproject operation started.
#define MIR_REPROJECT_COMPLETE_S                                                814           //    Reprojecting %1!%r!% complete.
#define MIR_REPROJECT_COMPLETED                                                 815           //    Reproject operation completed successfully.
#define MIR_RESAMPLE_COMPLETE_S                                                 816           //    Resampling %1!%r!% complete.
#define MIR_RESAMPLE_STARTED                                                    817           //    Resample operation started.
#define MIR_RESAMPLE_COMPLETED                                                  818           //    Resample operation completed successfully.
#define MIR_SLOPE_STARTED                                                       819           //    Slope operation started.
#define MIR_SLOPE_COMPLETE_S                                                    820           //    Slope %1!%r!% complete.
#define MIR_SLOPE_COMPLETED                                                     821           //    Slope operation completed successfully.
#define MIR_ASPECT_STARTED                                                      822           //    Aspect operation started.
#define MIR_ASPECT_COMPLETE_S                                                   823           //    Aspect %1!%r!% complete.
#define MIR_ASPECT_COMPLETED                                                    824           //    Aspect operation completed successfully.
#define MIR_CURVATURE_STARTED                                                   825           //    Curvature operation started.
#define MIR_CURVATURE_COMPLETE_S                                                826           //    Curvature %1!%r!% complete.
#define MIR_CURVATURE_COMPLETED                                                 827           //    Curvature operation completed successfully.
#define MIR_VEC_FIELD_TYPE_NOTSUPPORTED                                         828           //    Vector field type is not supported by this operation.
#define MIR_INVALID_VEC_FIELD_INDEX                                             829           //    Invalid vector field index.
#define MIR_VECTORTOGRID_INVALID_FOREGROUNDTYPE                                 830           //    Incompatible foreground value type specified.
#define MIR_VEC2GRID_PREPROC_INPUT_VECTOR                                       831           //    Pre-processing input vector file...
#define MIR_VEC2GRID_PREPROC_COMPLETED                                          832           //    Completed pre-processing input vector file.
#define MIR_VEC2GRID_STARTED                                                    833           //    Rasterize operation started.
#define MIR_VEC2GRID_PROC_UPDATE_S                                              834           //    Rasterize processing %1!%r!% complete..
#define MIR_VEC2GRID_COMPLETED                                                  835           //    Rasterize operation completed successfully.
#define MIR_VECTOR_TO_GRID1                                                     836           //    Start Load %1!%r!/%2!%r! : 
#define MIR_VECTOR_TO_GRID2                                                     837           //    End Load %1!%r!/%2!%r! : %3!%r!
#define MIR_VECTOR_TO_GRID3                                                     838           //    Start Process %1!%r!/%2!%r! : 
#define MIR_VECTOR_TO_GRID4                                                     839           //    End Process %1!%r!/%2!%r! : 
#define MIR_VECTOR_TO_GRID5                                                     840           //    Start Block Write %1!%r!/%2!%r! : %3!%r!
#define MIR_VECTOR_TO_GRID6                                                     841           //    End Block Write %1!%r!/%2!%r! : 
#define MIR_VECTOR_TO_GRID7                                                     842           //    End Load %1!%r!/%2!%r! : %3!%r! %4!%r!
#define MIR_VECTOR_TO_GRID8                                                     843           //    Starting Chunk %1!%r!/%2!%r! : 
#define MIR_VECTOR_TO_GRID9                                                     844           //    Chunk size %1!%r!/%2!%r! : 
#define MIR_VECTOR_TO_GRID10                                                    845           //    Failed AddPoly %1!%r! : 
#define MIR_VECTOR_TO_GRID11                                                    846           //    Failed Process %1!%r! : 
#define MIR_VECTOR_TO_GRID12                                                    847           //    Succeed Process %1!%r!/%2!%r! : %3!%r!/%4!%r! 
#define MIR_VECTORTOGRID_NO_GEOMS_FOUND                                         848           //    No supported geometry found to rasterize.
#define MIR_INVALID_FILE                                                        849           //    Unable to verify file.
#define MIR_VIEWSHED_INVALID_RADIUS                                             850           //    Viewshed radius must be greater than or equal to cell size.
#define MIR_VIEWSHED_INVALID_VIEWPOINTHEIGHT                                    851           //    Viewpoint height must be positive.
#define MIR_VIEWSHED_INVALID_SWEEP                                              852           //    Invalid sweep angle/azimuth parameters
#define MIR_VIEWSHED_INVALID_FILTER                                             853           //    Invalid viewshed filter size.
#define MIR_VIEWSHED_INVALID_CLASSIFICATION                                     854           //    Invalid viewshed classification.
#define MIR_VIEWSHED_INVALID_EARTHCURVATURE                                     855           //    Invalid viewshed earth curvature.
#define MIR_VIEWSHED_INVALID_TOWER_AREA                                         856           //    Invalid viewshed tower area. Check coordinate systems.
#define MIR_VIEWSHED_COMPLEX_CALC_NOT_SUPPORTED_FOR_MULTI_TILE_RASTERS          857           //    The viewshed complex calculation option is not supported for multi-tile rasters.
#define MIR_GENERATING_VIEWSHED                                                 858           //    Viewshed operation started.
#define MIR_VIEWSHED_ORIGIN_IS_NOT_OVER_A_VALID_CELL                            859           //    Viewshed origin is not over a valid cell, so height cannot be determined.
#define MIR_VIEWSHED_COMPLETE_S                                                 860           //    Viewshed %1!%r!% complete.
#define MIR_VIEWSHED_WRITING_UNPROCESSED_TILES                                  861           //    Writing unprocessed tiles.
#define MIR_VIEWSHED_COMPLETED                                                  862           //    Viewshed operation completed successfully.
#define MIR_TABLE_ACCESS_ERROR                                                  863           //    An error occurred accessing the table.
#define MIR_HORIZONTAL_UNIT_MISMATCH                                            864           //    Raster's horizontal unit code must be same.
#define MIR_VERTICAL_UNIT_MISMATCH                                              865           //    Raster's vertical unit code must be same.
#define MIR_VOLUME_OUTPUT_UNIT_UNDEFINED                                        866           //    Not able to deduce the output unit, define a valid output unit.
#define MIR_VOLUME_PLANE_D                                                      867           //    Computing Volume of the raster against constant plane %1!%f!.
#define MIR_VOLUME_SECONDARY_RASTER_S                                           868           //    Computing Volume of the raster against secondary raster %1!%r!.
#define MIR_RASTERVOLUME_OPERATION_NAME                                         869           //    Raster volume
#define MIR_VOLUME_OUTPUT                                                       870           //    Computed volume is %1!%.4f! and cell count is %2!%r!.
#define MIR_XSection_DISTANCE_LT_BASERES                                        871           //    Warning: Calculated distance between points is less than the base resolution cell size.
#define MIR_XSection_DISTANCE_SAMPLES_F_D                                       872           //    Computing cross section with %1!%f! distance between points and %2!%r! total samples ...
#define MIR_CROSS_SECTION_START                                                 873           //    Cross Section operation started.
#define MIR_COMPUTE_XSECTION                                                    874           //    Computing cross section for all lines...
#define MIR_START_CREATE_VECTOR_FILE_S                                          875           //    Start creating %1!%r! output vector file.
#define MIR_END_CREATE_VECTOR_FILE                                              876           //    Vector file creation completed.
#define MIR_XSECTION_COMPLETED                                                  877           //    Cross Section operation completed successfully.
#define MIR_ONLY_CONTINOUS_CLASSIFIED_IMAGE_SUPPORTED                           878           //    Field type is not valid, only Continuous, Classified and Image fields allowed.
#define MIR_INPUT_VECTOR                                                        879           //    Input vector File Path: %1!%r!.
#define MIR_INPUT_SOURCE                                                        880           //    Input Source File Path [%1!%r!]: %2!%r!.
#define MIR_OUTPUT_VECTOR                                                       881           //    Output vector File Path: %1!%r!.
#define MIR_OPERATION_PERFORMED                                                 882           //    Operation going to be performed: %1!%r!.
#define MIR_REGION_INSPECTION_START                                             883           //    Region Inspection operation started.
#define MIR_NO_POLYGON_OBJECTS_FOUND                                            884           //    No polygon objects found in Input vector file.
#define MIR_BEGIN_REGION_INSPECTION                                             885           //    Region inspection started for %1!%r!
#define MIR_END_REGION_INSPECTION                                               886           //    Region inspection completed for %1!%r!.
#define MIR_REGION_INSPECTION_COMPLETED                                         887           //    Region Inspection operation completed successfully.
#define MIR_BEGIN_GRID_REGION_INSPECTION                                        888           //    Begin region inspection %1!%r!
#define MIR_END_GRID_REGION_INSPECTION                                          889           //    End region inspection %1!%r!
#define MIR_BEGION_REGION_STATS_COMPUTE                                         890           //    Begin region statistics computation %1!%r!
#define MIR_END_REGION_STATS_COMPUTE                                            891           //    End region statistics computation %1!%r!
#define MIR_INVALID_BIN_COUNT                                                   892           //    Invalid Number of Bins specified.
#define MIR_INVALID_CLASSIFICATION_TYPE                                         893           //    Invalid classification type. Reclassify operation not allowed.
#define MIR_REQD_CLASS_NAME_MISSING                                             894           //    Required class name missing from input or output classification info.
#define MIR_INVALID_CLASS_VALUE                                                 895           //    Output class value is out of range. Maximum allowed value is 65535.
#define MIR_INVALID_CLASSIFICATION_RANGE                                        896           //    The classification info range is incorrect, lower bound greater than or equal to upper bound.
#define MIR_FAIL_CLASSIFY                                                       897           //    Failed to classify grid.
#define MIR_INVALID_CLASSIFICATION_RANGE_OVERLAP                                898           //    The classification ranges are overlapping.
#define MIR_CLASSIFY_STARTED                                                    899           //    Classify operation started.
#define MIR_CLASSIFY_COMPLETE_S                                                 900           //    Classifying %1!%r!% complete.
#define MIR_CLASSIFY_COMPLETE                                                   901           //    Classify operation completed successfully.
#define MIR_DUPLICATE_CLASS_VALUE                                               902           //    Duplicate input class value are not allowed.
#define MIR_COMPUTE_STATS_COMPLETE_S                                            903           //    Computing statistics %1!%r!% completed.
#define MIR_Align_Incompatible                                                  904           //    Input raster is incompatible to align with primary raster.
#define MIR_RES_INSIDE                                                          905           //    Visible
#define MIR_RES_OUTSIDE                                                         906           //    Invisible
#define MIR_RES_FRINGE                                                          907           //    Fringe
#define MIR_RES_TRUE                                                            908           //    True
#define MIR_RES_FALSE                                                           909           //    False
#define MIR_RES_NULL                                                            910           //    Null
#define MIR_RES_OFFGRID                                                         911           //    Off Raster
#define MIR_RES_POINTS                                                          912           //    point
#define MIR_RES_LINE                                                            913           //    lines
#define MIR_RES_REGIONS                                                         914           //    region
#define MIR_RES_NOCELL                                                          915           //    No Cell
#define MIR_RES_INVALID                                                         916           //    Invalid
#define MIR_RES_VISIBLE                                                         917           //    Visible
#define MIR_RES_DISTANCE                                                        918           //    Distance
#define MIR_RES_OFFSETREQUIRED                                                  919           //    OffsetRequired
#define MIR_RES_NUM_VISIBLE                                                     920           //    NumVisible
#define MIR_RES_XML_READ_FAIL                                                   921           //    XMLReadFail
#define MINT_RRENDER_NOLAYERTYPE                                                922           //    Rendering failure. A layer has no defined type.
#define MINT_RRENDER_NOCOMPTYPE                                                 923           //    Rendering failure. A component has no defined type.
#define MINT_RRENDER_ALGINVALID                                                 924           //    Rendering failure. The algorithm has been incorrectly defined.
#define MINT_RRENDER_ALGFAIL                                                    925           //    Rendering failure. The algorithm failed.
#define MINT_RENDER_FAIL_BADSCENE                                               984           //    Rendering failure. Bad scene parameters supplied.
#define MINT_RENDER_FAIL_BADCOLORTYPE                                           985           //    Rendering failure. Bad color type supplied.
#define MINT_RENDER_FAIL_NOCONTEXT                                              986           //    Rendering failure. Rendering context is invalid or does not exist.
#define MINT_RENDER_FAIL_BADBUFFER                                              987           //    Rendering failure. Bad memory buffer supplied.
#define MINT_RENDER_FAIL_ALGDISABLED                                            988           //    Rendering failure. The algorithm is disabled.
#define MINT_ALGBLD_FAIL_XMLENCODE                                              972           //    Failed to encode information to XML.
#define MINT_ALGBLD_FAIL_NOALG                                                  973           //    Error. No algorithm defined.
#define MINT_ALGBLD_FAIL_NOLAY                                                  974           //    Error. No layer defined.
#define MINT_ALGBLD_FAIL_NOCPT                                                  975           //    Error. No component defined.
#define MINT_ALGBLD_FAIL_NORASSRC                                               976           //    Error. No raster source defined.
#define MINT_ALGBLD_FAIL_NOCOLTAB                                               977           //    Error. No color table defined.
#define MINT_ALGBLD_FAIL_NODATTRAN                                              978           //    Error. No data transform defined.
#define MINT_ALGBLD_FAIL_NOCOLDATCOND                                           979           //    Error. No data conditioning defined.
#define MINT_ALGBLD_FAIL_NORASSRCFILE                                           980           //    Error. Invalid file index.
#define MINT_ALGBLD_FAIL_NOCOLTABCOL                                            981           //    Error. Invalid color index.
#define MINT_ALGBLD_FAIL_NODATTRANIND                                           982           //    Error. Invalid data index.
#define MINT_ALGBLD_FAIL_NOCOLDATCONDIND                                        983           //    Error. Invalid data index.
#define MINT_ALGBLD_FAIL_GHX                                                    989           //    Error. Could not interpret the GHX file.
#define MINT_ALGBLD_FAIL_BADLAYTYPE                                             990           //    Error. Layer type is not defined.
#define MINT_ALGBLD_FAIL_BADCMPTYPE                                             992           //    Error. Component type is not defined.
#define MINT_ALGBLD_FAIL_NOLAYER                                                991           //    Error. No renderable layers are defined.
#define MIR_RESP_COLNAME_VALIDATE_FAIL                                          957           //    Column name validation failed, column name should be unique.
#define MIR_POLYGONISE_IMAGERY_NOT_SUPORTED                                     958           //    Imagery field type is not supported for the selected polygonise mode.
#define MIR_DUPLICATE_CLASS_VALUE_COLOR                                         959           //    Output class values should have unique color values.
#define MINT_SUGGESTINFO_FAIL                                                   960           //    Unable to Suggest Raster Info for provided inputs.
#define MINT_DRIVERFAILEDWARPREQUIRED                                           926           //    Driver failed because a warp is required.
#define MINT_WARP_FAILCONTROLPNTS                                               961           //    Warp failed. An insufficient number of control points were supplied.
#define MINT_WARP_FAILDRIVERIMAGE                                               962           //    Warp failed. The raster driver does not support Image fields.
#define MINT_WARP_FAILTRANSFORM                                                 963           //    Warp failed. The warp could not be performed.
#define MIR_COLNAME_NOT_EXIST                                                   965           //    Column not found, column name should pre-exist in the input TAB.
#define MIR_COLUMN_DATATYPE_MISMATCH                                            966           //    Input TAB Column datatype not compatible with raster data type.
#define MIR_RACLASSIFY_WARNTABLEBANDCOUNT                                       967           //    Warning: The number of declared bands (%1!%r!) in the classification table does not match the expected band count (%2!%r!).
#define MIR_RACLASSIFY_WARNTABLEROWCOUNT                                        968           //    Warning: The number of declared rows (%1!%r!) in the classification table does not match the expected row count (%2!%r!).
#define MIR_GRIDOP_IMPORTMRRPNT                                                 969           //    Gridding: Import to point cache.
#define MIR_GRIDOP_IMPORTVALIDSTATIONCOUNT                                      970           //    Imported a total of %1!%r! valid stations.

//
// Dictionary CUSRDMutable
//
// Do not manually edit this file. It should be re-generated, when required, by running MakeAPICodes.exe.
// This program will also re-generate RasterDictionary.xml.
//
// New error codes should be added to m_vvDictionary in MINTSystem.cpp
//

