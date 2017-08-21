/******************************************************************************
 * Project:  Selafin importer
 * Purpose:  Definition of functions for reading records in Selafin files
 * Author:   François Hissel, francois.hissel@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2014,  François Hissel <francois.hissel@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef  IO_SELAFIN_H_INC
#define  IO_SELAFIN_H_INC

#include "cpl_quad_tree.h"
#include "cpl_vsi.h"

namespace Selafin {
    /**
     * \brief Data structure holding general information about a Selafin file
     */
class Header {
    private:
        int nHeaderSize; //!< Size (in bytes) of the header of the file (seeking to this location brings to the first "feature")
        int nStepSize;    //!< Size (in bytes) of one feature in the file
        int nMinxIndex;    //!< Index of the point at the western border of the bounding box
        int nMaxxIndex;    //!< Index of the point at the eastern border of the bounding box
        int nMinyIndex;    //!< Index of the point at the southern border of the bounding box
        int nMaxyIndex;    //!< Index of the point at the northern border of the bounding box
        bool bTreeUpdateNeeded;  //!< Tell if the quad tree has to be updated
    public:
        vsi_l_offset nFileSize;  //!< Size (in bytes) of the file
        //size_t nRefCount;   //!< Number of references to this object
        VSILFILE *fp;   //!< Pointer to the file with the layers
        char *pszFilename;  //!< Name of the Selafin file
        char *pszTitle; //!< Title of the simulation
        int nVar;   //!< Number of variables
        char **papszVariables;  //!< Name of the variables
        int nPoints;    //!< Total number of points
        int nElements;  //!< Total number of elements
        int nPointsPerElement;  //!< Number of points per element
        int *panConnectivity;   //!< Connectivity table of elements: first nPointsPerElement elements are the indices of the points making the first element, and so on. In the Selafin file, the first point has index 1.
        double *paadfCoords[2]; //!< Table of coordinates of points: x then y
        CPLQuadTree *poTree;    //!< Quad-tree for spatially indexing points in the array paadfCoords. The tree will mostly contain a Null value, until a request is made to find a closest neighbour, in which case it will be built and maintained
        double adfOrigin[2];  //!< Table of coordinates of the origin of the axis
        int *panBorder;    //!< Array of integers defining border points (0 for an inner point, and the index of the border point otherwise). This table is not used by the driver but stored to allow to rewrite the header if needed
        int *panStartDate;    //!< Table with the starting date of the simulation (may be 0 if date is not defined). Date is registered as a set of six elements: year, month, day, hour, minute, second.
        int nSteps;    //!< Number of steps in the Selafin file
        int nEpsg; //!< EPSG of the file
        int anUnused[7];   //!< Array of integers for storing the eight values read from the header but not actually used by the driver. These values are merely saved to rewrite them in the file if it is changed.

        Header(); //!< Standard constructor
        ~Header();  //!< Destructor of structure

        /**
         * \brief Return the position of a particular data in the Selafin file
         *
         * This function returns the position in the Selafin file of a particular element, characterized by the number of the time step, the index of the feature and the index of the attribute. If both nFeature and nAttribute are equal to -1 (default value), the function returns the position of the start of this time step. If nFeature is -1 but nAttribute is greater than 0, the function returns the position of the table for the given attribute (compatible with Selafin::read_floatarray)
         * \param nStep Number of the time step, starting with 0
         * \param nFeature Index of the feature (point), starting with 0
         * \param nAttribute Index of the attribute, starting with 0
         * \return Position (in bytes) from the start of the file
         */
        int getPosition(int nStep,int nFeature=-1,int nAttribute=-1) const; // {return nHeaderSize+nStep*nStepSize+(nFeature!=-1)?(12+nAttribute*(nPoints+2)*4+4+nFeature*4):0;}

        /**
         * \brief Return the bounding box of the points
         *
         * This function returns the bounding box of the set of points. The bounding box is stored in memory for quicker access.
         * \return Pointer to the bounding box. The calling program is responsible for destroying it
         */
        CPLRectObj *getBoundingBox() const;

        /**
         * \brief Update the bounding box of the points
         *
         * This function calculates the bounding box of the set of points and stores it in the class.
         */
        void updateBoundingBox();

        /**
         * \brief Return the index of the point closest to the given coordinates
         *
         * This function searches the point in the array which is the closest to the one given by the user in arguments, but no farther than distance dfMax.
         * \param dfx x-coordinate of the reference point
         * \param dfy y-coordinate of the reference point
         * \param dfMax Maximum distance allowed to the point
         * \return Index of the closest point in the array, -1 if there was no point at all in the array closer than distance dfMax
         */
        int getClosestPoint(const double &dfx,const double &dfy,const double &dfMax);

        /**
         * \brief Tells that the header has been changed
         *
         * This function must be used whenever one of the member of the structure was changed. It forces the recalculation of some variables (header size and step size).
         */
        void setUpdated();

        /**
         * \brief Add a new point at the end of point list
         *
         * This function add a new point at the end of the array. If the arrays of coordinates are too small, they may be resized. The bounding box is updated.
         * \param dfx x-coordinate of the new point
         * \param dfy y-coordinate of the new point
         */
        void addPoint(const double &dfx,const double &dfy);

        /**
         * \brief Remove a point from the point list
         *
         * This function removes a point at position nIndex from the array of points. The bounding box is updated. All the elements which referenced this point are also removed.
         * \param nIndex Index of the point which has to be removed
         */
        void removePoint(int nIndex);

        void UpdateFileSize();
};

#ifdef notdef
/**
 * \brief Data structure holding the attributes of all nodes for one time step
 */
class TimeStep {
    private:
        //int nRecords;  //!< Number of records (first index) in the TimeStep::papadfData array, which should correspond to the number of features (either points or elements) in a Selafin layer
        int nFields;   //!< Number of fields for each record (second index) in the TimeStep::papadfData array, which should correspond to the number of attributes in a Selafin layer
    public:
        double dfDate;  //!< Date of the time step (usually in seconds after the starting date)
        double **papadfData;    //!< Double-indexed array with values of all attributes for all features. The first index is the number of the attribute, the second is the number of the feature.

        /**
         * \brief Constructor of the structure
         *
         * This function allocates the TimeStep::papadfData array based on the dimensions provided in argument.
         * \param nRecords Number of records (first index) in the TimeStep::papadfData array, which should correspond to the number of features (either points or elements) in a Selafin layer
         * \param nFields Number of fields for each record (second index) in the TimeStep::papadfData array, which should correspond to the number of attributes in a Selafin layer
         */
        TimeStep(int nRecordsP,int nFieldsP);

        ~TimeStep();    //!< Standard destructor
};

/**
 * \brief Structure holding a chained list of time steps (of class TimeStep)
 */
class TimeStepList {
    public:
        TimeStep *poStep;   //!< Pointer to the time step structure
        TimeStepList *poNext;   //!< Pointer to the next element in the list

        TimeStepList(TimeStep *poStepP,TimeStepList *poNextP):poStep(poStepP),poNext(poNextP) {}    //!< Standard constructor
        ~TimeStepList();    //!< Standard destructor
};
#endif

/**
 * \brief Read an integer from a Selafin file
 *
 * This function reads an integer from an opened file. In Selafin files, integers are stored on 4 bytes in big-endian format (most significant byte first).
 * \param fp Pointer to an open file
 * \param nData After the execution, contains the value read
 * \param bDiscard If true, the function does not attempt to save the value read in the variable nData, but only advances in the file as it should. Default value is false.
 * \return 1 if the integer was successfully read, 0 otherwise
 */
int read_integer(VSILFILE *fp,int &nData,bool bDiscard=false);

/**
 * \brief Write an integer to a Selafin file
 *
 * This function writes an integer to an opened file. See also Selafin::read_integer for additional information about how integers are stored in a Selafin file.
 * \param fp Pointer to an open file
 * \param nData Value to be written to the file
 * \return 1 if the integer was successfully written, 0 otherwise
 */
int write_integer(VSILFILE *fp,int nData);

/**
 * \brief Read a string from a Selafin file
 *
 * This function reads a string from an opened file. In Selafin files, strings are stored in three parts:
 *   - an integer \a n (therefore on 4 bytes) with the length of the string
 *   - the \a n bytes of the string, preferably in ASCII format
 *   - once again the integer \a n (on 4 bytes)
 * The function does not check if the last integer is the same as the first one.
 * \param fp Pointer to an open file
 * \param pszData After the execution, contains the value read. The structure is allocated by the function.
 * \param bDiscard If true, the function does not attempt to save the value read in the variable nData, but only advances in the file as it should. Default value is false.
 * \return Number of characters in string read
 */
int read_string(VSILFILE *fp,char *&pszData,vsi_l_offset nFileSize,bool bDiscard=false);

/**
 * \brief Write a string to a Selafin file
 *
 * This function writes a string to an opened file. See also Selafin::read_string for additional information about how strings are stored in a Selafin file.
 * \param fp Pointer to an open file
 * \param pszData String to be written to the file
 * \param nLength Length of the string. If the value is 0, the length is automatically calculated. It is recommended to provide this value to avoid an additional strlen call. However, providing a value larger than the actual size of the string will result in an overflow read access and most likely in a segmentation fault.
 * \return 1 if the string was successfully written, 0 otherwise
 */
int write_string(VSILFILE *fp,char *pszData,size_t nLength=0);

/**
 * \brief Read an array of integers from a Selafin file
 *
 * This function reads an array of integers from an opened file. In Selafin files, arrays of integers are stored in three parts:
 *   - an integer \a n with the \e size of the array (therefore 4 times the number of elements)
 *   - the \f$ \frac{n}{4} \f$ elements of the array, each on 4 bytes
 *   - once again the integer \a n (on 4 bytes)
 * The function does not check if the last integer is the same as the first one.
 * \param fp Pointer to an open file
 * \param panData After the execution, contains the array read. The structure is allocated by the function.
 * \param bDiscard If true, the function does not attempt to save the value read in the variable nData, but only advances in the file as it should. Default value is false.
 * \return Number of elements in array read, -1 if an error occurred
 */
int read_intarray(VSILFILE *fp,int *&panData,vsi_l_offset nFileSize,bool bDiscard=false);

/**
 * \brief Write an array of integers to a Selafin file
 *
 * This function writes an array of integers to an opened file. See also Selafin::read_intarray for additional information about how arrays of integers are stored in a Selafin file.
 * \param fp Pointer to an open file
 * \param panData Array to be written to the file
 * \param nLength Number of elements in the array
 * \return 1 if the array was successfully written, 0 otherwise
 */
int write_intarray(VSILFILE *fp,int *panData,size_t nLength);

/**
 * \brief Read a floating point number from a Selafin file
 *
 * This function reads a floating point value from an opened file. In Selafin files, floats are stored on 4 bytes in big-endian format (most significant byte first).
 * \param fp Pointer to an open file
 * \param dfData After the execution, contains the value read
 * \param bDiscard If true, the function does not attempt to save the value read in the variable nData, but only advances in the file as it should. Default value is false.
 * \return 1 if the floating point number was successfully read, 0 otherwise
 */
int read_float(VSILFILE *fp,double &dfData,bool bDiscard=false);

/**
 * \brief Write a floating point number to a Selafin file
 *
 * This function writes a floating point value from an opened file. See also Selafin::read_float for additional information about how floating point numbers are stored in a Selafin file.
 * \param fp Pointer to an open file
 * \param dfData Floating point number to be written to the file
 * \return 1 if the floating point number was successfully written, 0 otherwise
 */
int write_float(VSILFILE *fp,double dfData);

/**
 * \brief Read an array of floats from a Selafin file
 *
 * This function reads an array of floats from an opened file. In Selafin files, arrays of floats are stored in three parts:
 *   - an integer \a n with the \e size of the array (therefore 4 times the number of elements)
 *   - the \f$ \frac{n}{4} \f$ elements of the array, each on 4 bytes
 *   - once again the integer \a n (on 4 bytes)
 * The function does not check if the last integer is the same as the first one.
 * \param fp Pointer to an open file
 * \param papadfData After the execution, contains the array read. The structure is allocated by the function.
 * \param bDiscard If true, the function does not attempt to save the value read in the variable nData, but only advances in the file as it should. Default value is false.
 * \return Number of elements in array read, -1 if an error occurred
 */
int read_floatarray(VSILFILE *fp,double **papadfData,vsi_l_offset nFileSize,bool bDiscard=false);

/**
 * \brief Write an array of floats to a Selafin file
 *
 * This function writes an array of floats from an opened file. See also Selafin::read_floatarray for additional information about how arrays of floating point numbers are stored in a Selafin file.
 * \param fp Pointer to an open file
 * \param padfData Pointer to the array of floating point numbers to be written to the file
 * \param nLength Number of elements in the array
 * \return 1 if the array was successfully written, 0 otherwise
 */
int write_floatarray(VSILFILE *fp,double *padfData,size_t nLength);

/**
 * \brief Read the header of a Selafin file
 *
 * This function reads the whole header of a Selafin file (that is everything before the first time step) and stores it in a Selafin::Header structure. The file pointer is moved at the beginning of the file before reading.
 * \param fp Pointer to an open file
 * \param pszFilename Name of the file
 * \return Pointer to a newly-allocated header structure, 0 if the function failed to read the whole header
 */
Header *read_header(VSILFILE *fp,const char* pszFilename);

/**
 * \brief Write the header to a file
 *
 * This function writes the header to an opened file. The file pointer is moved at the beginning of the file and the content is overwritten.
 * \param fp Pointer to an open file with write access
 * \return 1 if the header was successfully written, 0 otherwise
 */
int write_header(VSILFILE *fp,Header *poHeader);

#ifdef notdef
/**
 * \brief Read one time step from a Selafin file
 *
 * This function reads a single time step, with all the field values, from an open Selafin file at the current position.
 * \param fp Pointer to an open file
 * \param poHeader Pointer to the header structure. This should be read before the call to the function because some of its members are used to determine the format of the time step.
 * \param poStep After the execution, holds the data for the current step. The variable is allocated by the function
 * \return 1 if the time step was successfully read, 0 otherwise
 */
int read_step(VSILFILE *fp,const Header *poHeader,TimeStep *&poStep);

/**
 * \brief Write one time step to a Selafin file
 *
 * This function writes a single time step, with all the field values, to an open Selafin file at the current position.
 * \param fp Pointer to an open file with write access
 * \param poHeader Pointer to the header structure. This should be read before the call to the function because some of its members are used to determine the format of the time step.
 * \param poStep Data of current step
 * \return 1 if the time step was successfully written, 0 otherwise
 */
int write_step(VSILFILE *fp,const Header *poHeader,const TimeStep *poStep);

/**
 * \brief Read all time steps from a Selafin file
 *
 * This function reads all time steps, with all the field values, from an open Selafin file at the current position.
 * \param fp Pointer to an open file
 * \param poHeader Pointer to the header structure. This should be read before the call to the function because some of its members are used to determine the format of the time step.
 * \param poSteps After the execution, holds the list of time steps. The variable is allocated by the function
 * \return 1 if the time steps were successfully read, 0 otherwise
 */
int read_steps(VSILFILE *fp,const Header *poHeader,TimeStepList *&poSteps);

/**
 * \brief Write one time step to a Selafin file
 *
 * This function writes a single time step, with all the field values, to an open Selafin file at the current position.
 * \param fp Pointer to an open file with write access
 * \param poHeader Pointer to the header structure. This should be read before the call to the function because some of its members are used to determine the format of the time step.
 * \param poSteps List of all steps
 * \return 1 if the time steps were successfully written, 0 otherwise
 */
int write_steps(VSILFILE *fp,const Header *poHeader,const TimeStepList *poSteps);
#endif

}  // namespace Selafin

#endif   /* ----- #ifndef IO_SELAFIN_H_INC  ----- */
