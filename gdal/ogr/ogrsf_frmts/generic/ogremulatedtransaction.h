/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRDataSourceWithTransaction class
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef _OGREMULATEDTRANSACTION_H_INCLUDED
#define _OGREMULATEDTRANSACTION_H_INCLUDED

#include "ogrsf_frmts.h"

/** IOGRTransactionBehaviour is an interface that a driver must implement
 *  to provide emulation of transactions.
 *
 * @since GDAL 2.0
 */
class CPL_DLL IOGRTransactionBehaviour
{
    public:
        virtual ~IOGRTransactionBehaviour();

        /** Start a transaction.
        *
        * The implementation may update the poDSInOut reference by closing
        * and reopening the datasource (or assigning it to NULL in case of error).
        * In which case bOutHasReopenedDS must be set to TRUE.
        *
        * The implementation can for example backup the existing files/directories
        * that compose the current datasource.
        *
        * @param poDSInOut datasource handle that may be modified
        * @param bOutHasReopenedDS output boolean to indicate if datasource has been closed
        * @return OGRERR_NONE in case of success
        */
       virtual OGRErr StartTransaction(OGRDataSource*& poDSInOut,
                                       int& bOutHasReopenedDS) = 0;

        /** Commit a transaction.
        *
        * The implementation may update the poDSInOut reference by closing
        * and reopening the datasource (or assigning it to NULL in case of error).
        * In which case bOutHasReopenedDS must be set to TRUE.
        *
        * The implementation can for example remove the backup it may have done
        * at StartTransaction() time.
        *
        * @param poDSInOut datasource handle that may be modified
        * @param bOutHasReopenedDS output boolean to indicate if datasource has been closed
        * @return OGRERR_NONE in case of success
        */
       virtual OGRErr CommitTransaction(OGRDataSource*& poDSInOut,
                                        int& bOutHasReopenedDS) = 0;

        /** Rollback a transaction.
        *
        * The implementation may update the poDSInOut reference by closing
        * and reopening the datasource (or assigning it to NULL in case of error).
        * In which case bOutHasReopenedDS must be set to TRUE.
        *
        * The implementation can for example restore the backup it may have done
        * at StartTransaction() time.
        *
        * @param poDSInOut datasource handle that may be modified
        * @param bOutHasReopenedDS output boolean to indicate if datasource has been closed
        * @return OGRERR_NONE in case of success
        */
       virtual OGRErr RollbackTransaction(OGRDataSource*& poDSInOut,
                                          int& bOutHasReopenedDS) = 0;
};


/** Returns a new datasource object that adds transactional behaviour to an existing datasource.
 * 
 * The provided poTransactionBehaviour object should implement driver-specific
 * behaviour for transactions.
 *
 * The generic mechanisms offered by the wrapper class do not cover concurrent
 * updates (though different datasource connections) to the same datasource files.
 *
 * There are restrictions on what can be accomplished. For example it is not
 * allowed to have a unreleased layer returned by ExecuteSQL() before calling
 * StartTransaction(), CommitTransaction() or RollbackTransaction().
 *
 * Layer structural changes are not allowed after StartTransaction() if the
 * layer definition object has been returned previously with GetLayerDefn().
 *
 * @param poBaseDataSource the datasource to which to add transactional behaviour.
 * @param poTransactionBehaviour an implementation of the IOGRTransactionBehaviour interface.
 * @param bTakeOwnershipDataSource whether the returned object should own the
 *                                 passed poBaseDataSource (and thus destroy it
 *                                 when it is destroyed itself).
 * @param bTakeOwnershipTransactionBehaviour whether the returned object should own
 *                                           the passed poTransactionBehaviour
 *                                           (and thus destroy it when
 *                                           it is destroyed itself).
 * @return a new datasource handle
 * @since GDAL 2.0
 */
OGRDataSource CPL_DLL* OGRCreateEmulatedTransactionDataSourceWrapper(
                                OGRDataSource* poBaseDataSource,
                                IOGRTransactionBehaviour* poTransactionBehaviour,
                                int bTakeOwnershipDataSource,
                                int bTakeOwnershipTransactionBehaviour);

#endif // _OGREMULATEDTRANSACTION_H_INCLUDED
