/*-------------------------------------------------------------------------
 *
 * pg_cheat_funcs.c
 *   provides various cheat (but useful) functions
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/htup_details.h"
#include "access/subtrans.h"
#include "access/xlog_internal.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "replication/walreceiver.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_xlogfile_name);
PG_FUNCTION_INFO_V1(pg_set_nextxid);
PG_FUNCTION_INFO_V1(pg_xid_assignment);
PG_FUNCTION_INFO_V1(pg_show_primary_conninfo);

/*
 * Compute an xlog file name given a WAL location.
 */
Datum
pg_xlogfile_name(PG_FUNCTION_ARGS)
{
	XLogSegNo	xlogsegno;
	XLogRecPtr	locationpoint = PG_GETARG_LSN(0);
	char		xlogfilename[MAXFNAMELEN];
	bool		recovery = PG_GETARG_BOOL(1);

	if (!recovery && RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("recovery is in progress"),
		 errhint("pg_xlogfile_name() cannot be executed during recovery.")));

	XLByteToPrevSeg(locationpoint, xlogsegno);
	XLogFileName(xlogfilename, ThisTimeLineID, xlogsegno);

	PG_RETURN_TEXT_P(cstring_to_text(xlogfilename));
}

/*
 * Set and return the next transaction ID.
 */
Datum
pg_set_nextxid(PG_FUNCTION_ARGS)
{
	TransactionId xid = PG_GETARG_UINT32(0);

	if (RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("recovery is in progress"),
		 errhint("pg_set_nextxid() cannot be executed during recovery.")));

	LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
	ShmemVariableCache->nextXid = xid;
	LWLockRelease(XidGenLock);

	/*
	 * Make sure that CLOG has room for the given next XID.
	 * These macros are borrowed from src/backend/access/transam/clog.c.
	 */
#define CLOG_XACTS_PER_BYTE 4
#define CLOG_XACTS_PER_PAGE (BLCKSZ * CLOG_XACTS_PER_BYTE)
#define TransactionIdToPgIndex(xid) ((xid) % (TransactionId) CLOG_XACTS_PER_PAGE)
	if (TransactionIdToPgIndex(xid) != 0 &&
		!TransactionIdEquals(xid, FirstNormalTransactionId))
		ExtendCLOG(xid - TransactionIdToPgIndex(xid));

	/*
	 * Make sure that SUBTRANS has room for the given next XID.
	 * These macros are borrowed from src/backend/access/transam/subtrans.c.
	 */
#define SUBTRANS_XACTS_PER_PAGE (BLCKSZ / sizeof(TransactionId))
#define TransactionIdToEntry(xid) ((xid) % (TransactionId) SUBTRANS_XACTS_PER_PAGE)
	if (TransactionIdToEntry(xid) != 0 &&
		!TransactionIdEquals(xid, FirstNormalTransactionId))
		ExtendSUBTRANS(xid - TransactionIdToEntry(xid));

	PG_RETURN_UINT32(xid);
}

/*
 * Return information about XID assignment state.
 */
Datum
pg_xid_assignment(PG_FUNCTION_ARGS)
{
#define PG_XID_ASSIGNMENT_COLS	7
	TupleDesc	tupdesc;
	Datum		values[PG_XID_ASSIGNMENT_COLS];
	bool		nulls[PG_XID_ASSIGNMENT_COLS];
	TransactionId nextXid;
	TransactionId oldestXid;
	TransactionId xidVacLimit;
	TransactionId xidWarnLimit;
	TransactionId xidStopLimit;
	TransactionId xidWrapLimit;
	Oid			oldestXidDB;

	/* Initialise values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Initialise attributes information in the tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(PG_XID_ASSIGNMENT_COLS, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "next_xid",
					   XIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "oldest_xid",
					   XIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "xid_vac_limit",
					   XIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "xid_warn_limit",
					   XIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5, "xid_stop_limit",
					   XIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 6, "xid_wrap_limit",
					   XIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 7, "oldest_xid_db",
					   OIDOID, -1, 0);

	BlessTupleDesc(tupdesc);

	/* Take a lock to ensure value consistency */
	LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
	nextXid = ShmemVariableCache->nextXid;
	oldestXid = ShmemVariableCache->oldestXid;
	xidVacLimit = ShmemVariableCache->xidVacLimit;
	xidWarnLimit = ShmemVariableCache->xidWarnLimit;
	xidStopLimit = ShmemVariableCache->xidStopLimit;
	xidWrapLimit = ShmemVariableCache->xidWrapLimit;
	oldestXidDB = ShmemVariableCache->oldestXidDB;
	LWLockRelease(XidGenLock);

	/* Fetch values */
	values[0] = TransactionIdGetDatum(nextXid);
	values[1] = TransactionIdGetDatum(oldestXid);
	values[2] = TransactionIdGetDatum(xidVacLimit);
	values[3] = TransactionIdGetDatum(xidWarnLimit);
	values[4] = TransactionIdGetDatum(xidStopLimit);
	values[5] = TransactionIdGetDatum(xidWrapLimit);
	values[6] = ObjectIdGetDatum(oldestXidDB);

	/* Returns the record as Datum */
	PG_RETURN_DATUM(HeapTupleGetDatum(
						heap_form_tuple(tupdesc, values, nulls)));
}

/*
 * Return the connection string that walreceiver uses to connect with
 * the primary.
 */
Datum
pg_show_primary_conninfo(PG_FUNCTION_ARGS)
{
	char		conninfo[MAXCONNINFO];
	WalRcvData *walrcv = WalRcv;

	SpinLockAcquire(&walrcv->mutex);
	strlcpy(conninfo, (char *) walrcv->conninfo, MAXCONNINFO);
	SpinLockRelease(&walrcv->mutex);

	if (conninfo[0] == '\0')
		PG_RETURN_NULL();
	PG_RETURN_TEXT_P(cstring_to_text(conninfo));
}
