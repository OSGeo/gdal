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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGREMULATEDTRANSACTION_H_INCLUDED
#define OGREMULATEDTRANSACTION_H_INCLUDED

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
     * @param bOutHasReopenedDS output boolean to indicate if datasource has
     * been closed
     * @return OGRERR_NONE in case of success
     */
    virtual OGRErr StartTransaction(GDALDataset *&poDSInOut,
                                    int &bOutHasReopenedDS) = 0;

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
     * @param bOutHasReopenedDS output boolean to indicate if datasource has
     * been closed
     * @return OGRERR_NONE in case of success
     */
    virtual OGRErr CommitTransaction(GDALDataset *&poDSInOut,
                                     int &bOutHasReopenedDS) = 0;

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
     * @param bOutHasReopenedDS output boolean to indicate if datasource has
     * been closed
     * @return OGRERR_NONE in case of success
     */
    virtual OGRErr RollbackTransaction(GDALDataset *&poDSInOut,
                                       int &bOutHasReopenedDS) = 0;
};

/** Returns a new datasource object that adds transactional behavior to an
 * existing datasource.
 *
 * The provided poTransactionBehaviour object should implement driver-specific
 * behavior for transactions.
 *
 * The generic mechanisms offered by the wrapper class do not cover concurrent
 * updates (though different datasource connections) to the same datasource
 * files.
 *
 * There are restrictions on what can be accomplished. For example it is not
 * allowed to have a unreleased layer returned by ExecuteSQL() before calling
 * StartTransaction(), CommitTransaction() or RollbackTransaction().
 *
 * Layer structural changes are not allowed after StartTransaction() if the
 * layer definition object has been returned previously with GetLayerDefn().
 *
 * @param poBaseDataSource the datasource to which to add transactional
 * behavior.
 * @param poTransactionBehaviour an implementation of the
 * IOGRTransactionBehaviour interface.
 * @param bTakeOwnershipDataSource whether the returned object should own the
 *                                 passed poBaseDataSource (and thus destroy it
 *                                 when it is destroyed itself).
 * @param bTakeOwnershipTransactionBehavior whether the returned object should
 * own the passed poTransactionBehaviour (and thus destroy it when it is
 * destroyed itself).
 * @return a new datasource handle
 * @since GDAL 2.0
 */
GDALDataset CPL_DLL *OGRCreateEmulatedTransactionDataSourceWrapper(
    GDALDataset *poBaseDataSource,
    IOGRTransactionBehaviour *poTransactionBehaviour,
    int bTakeOwnershipDataSource, int bTakeOwnershipTransactionBehavior);

#endif  // OGREMULATEDTRANSACTION_H_INCLUDED
