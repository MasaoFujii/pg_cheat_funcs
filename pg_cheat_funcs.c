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
#include "miscadmin.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/lwlock.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/pg_lsn.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_stat_get_memory_context);
PG_FUNCTION_INFO_V1(pg_signal_process);
PG_FUNCTION_INFO_V1(pg_xlogfile_name);
PG_FUNCTION_INFO_V1(pg_set_next_xid);
PG_FUNCTION_INFO_V1(pg_xid_assignment);
PG_FUNCTION_INFO_V1(pg_show_primary_conninfo);

static void
PutMemoryContextStatsTupleStore(Tuplestorestate *tupstore,
								TupleDesc tupdesc, MemoryContext context,
								MemoryContext parent, int level);
static int GetSignalByName(char *signame);
static bool IsWalSenderPid(int pid);
static bool IsWalReceiverPid(int pid);

/*
 * Return statistics about all memory contexts.
 */
Datum
pg_stat_get_memory_context(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	PutMemoryContextStatsTupleStore(tupstore, tupdesc,
									TopMemoryContext, NULL, 1);

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

static void
PutMemoryContextStatsTupleStore(Tuplestorestate *tupstore,
								TupleDesc tupdesc, MemoryContext context,
								MemoryContext parent, int level)
{
#define PG_STAT_GET_MEMORY_CONTEXT_COLS	8
	Datum		values[PG_STAT_GET_MEMORY_CONTEXT_COLS];
	bool		nulls[PG_STAT_GET_MEMORY_CONTEXT_COLS];
	MemoryContextCounters stat;
	MemoryContext child;

	if (context == NULL)
		return;

	/* Examine the context itself */
	memset(&stat, 0, sizeof(stat));
	(*context->methods->stats) (context, level, false, &stat);

	memset(nulls, 0, sizeof(nulls));
	values[0] = CStringGetTextDatum(context->name);
	if (parent == NULL)
		nulls[1] = true;
	else
		values[1] = CStringGetTextDatum(parent->name);
	values[2] = Int32GetDatum(level);
	values[3] = Int64GetDatum(stat.totalspace);
	values[4] = Int64GetDatum(stat.nblocks);
	values[5] = Int64GetDatum(stat.freespace);
	values[6] = Int64GetDatum(stat.freechunks);
	values[7] = Int64GetDatum(stat.totalspace - stat.freespace);
	tuplestore_putvalues(tupstore, tupdesc, values, nulls);

	for (child = context->firstchild; child != NULL; child = child->nextchild)
	{
		PutMemoryContextStatsTupleStore(tupstore, tupdesc,
										child, context, level + 1);
	}
}

/*
 * Send a signal to PostgreSQL server process.
 */
Datum
pg_signal_process(PG_FUNCTION_ARGS)
{
	int		pid = PG_GETARG_INT32(0);
	char	*signame = text_to_cstring(PG_GETARG_TEXT_P(1));
	int		sig = GetSignalByName(signame);

	if (PostmasterPid != pid && !IsBackendPid(pid) &&
		!IsWalSenderPid(pid) && !IsWalReceiverPid(pid))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("PID %d is not a PostgreSQL server process", pid))));

	if (kill(pid, sig))
		ereport(ERROR,
				(errmsg("could not send signal to process %d: %m", pid)));

	PG_RETURN_VOID();
}

/*
 * Return signal corresponding to the given signal name.
 */
static int
GetSignalByName(char *signame)
{
	if (strcmp(signame, "HUP") == 0)
		return SIGHUP;
	else if (strcmp(signame, "INT") == 0)
		return SIGINT;
	else if (strcmp(signame, "QUIT") == 0)
		return SIGQUIT;
	else if (strcmp(signame, "ABRT") == 0)
		return SIGABRT;
	else if (strcmp(signame, "KILL") == 0)
		return SIGKILL;
	else if (strcmp(signame, "TERM") == 0)
		return SIGTERM;
	else if (strcmp(signame, "USR1") == 0)
		return SIGUSR1;
	else if (strcmp(signame, "USR2") == 0)
		return SIGUSR2;
	else if (strcmp(signame, "CONT") == 0)
		return SIGCONT;
	else if (strcmp(signame, "STOP") == 0)
		return SIGSTOP;
	else
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("unrecognized signal name \"%s\"", signame),
				  errhint("Valid signal names are \"HUP\", \"INT\", \"QUIT\", \"ABRT\", \"KILL\", \"TERM\", \"USR1\", \"USR2\", \"CONT\", and \"STOP\"."))));
}

/*
 * Is a given pid a running walsender?
 */
static bool
IsWalSenderPid(int pid)
{
	int	i;

	if (pid == 0)
		return false;

	for (i = 0; i < max_wal_senders; i++)
	{
		WalSnd *walsnd = &WalSndCtl->walsnds[i];

		if (walsnd->pid == pid)
			return true;
	}

	return false;
}

/*
 * Is a given pid a running walreceiver?
 */
static bool
IsWalReceiverPid(int pid)
{
	WalRcvData *walrcv = WalRcv;

	if (pid == 0)
		return false;

	return (walrcv->pid == pid);
}

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
pg_set_next_xid(PG_FUNCTION_ARGS)
{
	TransactionId xid = PG_GETARG_UINT32(0);

	if (RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("recovery is in progress"),
		 errhint("pg_set_next_xid() cannot be executed during recovery.")));

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
