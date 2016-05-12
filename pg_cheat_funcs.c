/*-------------------------------------------------------------------------
 *
 * pg_cheat_funcs.c
 *   provides various cheat (but useful) functions
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/clog.h"
#include "access/htup_details.h"
#include "access/subtrans.h"
#include "access/xlog_internal.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#if PG_VERSION_NUM >= 90500
#include "common/pg_lzcompress.h"
#endif
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

/* GUC variables */
static bool	pgcf_log_memory_context = false;

/* Saved hook values in case of unload */
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

#if PG_VERSION_NUM >= 90500
/* The information at the start of the compressed data */
typedef struct pglz_header
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	int32		rawsize;
} pglz_header;

#define PGLZ_HDRSZ		((int32) sizeof(pglz_header))
#define PGLZ_RAWSIZE(ptr) (((pglz_header *) (ptr))->rawsize & 0x3FFFFFFF)
#define PGLZ_RAWDATA(ptr) (((char *) (ptr)) + PGLZ_HDRSZ)
#define PGLZ_SET_RAWSIZE(ptr, len) \
	(((pglz_header *) (ptr))->rawsize = ((len) & 0x3FFFFFFF))
#define PGLZ_SET_RAWSIZE_COMPRESSED(ptr, len) \
	(((pglz_header *) (ptr))->rawsize = (((len) & 0x3FFFFFFF) | 0x40000000))
#define PGLZ_IS_COMPRESSED(ptr) \
	((((pglz_header *) (ptr))->rawsize & 0xC0000000) == 0x40000000)

PG_FUNCTION_INFO_V1(pglz_compress_text);
PG_FUNCTION_INFO_V1(pglz_compress_bytea);
PG_FUNCTION_INFO_V1(pglz_decompress_text);
PG_FUNCTION_INFO_V1(pglz_decompress_bytea);

static struct varlena *PGLZCompress(struct varlena *source);
static struct varlena *PGLZDecompress(struct varlena *source);
#endif	/* PG_VERSION_NUM >= 90500 */

/* pg_stat_get_memory_context function is available only in 9.6 or later */
#if PG_VERSION_NUM >= 90600
PG_FUNCTION_INFO_V1(pg_stat_get_memory_context);
#endif
PG_FUNCTION_INFO_V1(pg_stat_print_memory_context);
PG_FUNCTION_INFO_V1(pg_signal_process);
PG_FUNCTION_INFO_V1(pg_xlogfile_name);
PG_FUNCTION_INFO_V1(pg_set_next_xid);
PG_FUNCTION_INFO_V1(pg_xid_assignment);
PG_FUNCTION_INFO_V1(pg_show_primary_conninfo);

void		_PG_init(void);
void		_PG_fini(void);

static void pgcf_ExecutorEnd(QueryDesc *queryDesc);

#if PG_VERSION_NUM >= 90600
static void
PutMemoryContextStatsTupleStore(Tuplestorestate *tupstore,
								TupleDesc tupdesc, MemoryContext context,
								MemoryContext parent, int level);
#endif
static void PrintMemoryContextStats(MemoryContext context, int level);
static int GetSignalByName(char *signame);
static bool IsWalSenderPid(int pid);
static bool IsWalReceiverPid(int pid);

/*
 * Module load callback
 */
void
_PG_init(void)
{
	/* Define custom GUC variables. */
	DefineCustomBoolVariable("pg_cheat_funcs.log_memory_context",
							 "Cause statistics about all memory contexts to be logged.",
							 NULL,
							 &pgcf_log_memory_context,
							 false,
							 PGC_SUSET,
							 0,
							 NULL,
							 NULL,
							 NULL);

	EmitWarningsOnPlaceholders("pg_cheat_funcs");

	/* Install hooks. */
	prev_ExecutorEnd = ExecutorEnd_hook;
	ExecutorEnd_hook = pgcf_ExecutorEnd;
}

/*
 * Module unload callback
 */
void
_PG_fini(void)
{
	/* Uninstall hooks. */
	ExecutorEnd_hook = prev_ExecutorEnd;
}

/*
 * ExecutorEnd hook
 */
static void
pgcf_ExecutorEnd(QueryDesc *queryDesc)
{
	if (prev_ExecutorEnd)
		prev_ExecutorEnd(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);

	/* Print statistics about TopMemoryContext and all its descendants */
	if (pgcf_log_memory_context)
		PrintMemoryContextStats(TopMemoryContext, 0);
}

#if PG_VERSION_NUM >= 90600
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
									TopMemoryContext, NULL, 0);

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
#endif	/* PG_VERSION_NUM >= 90600 */

/*
 * Print statistics about TopMemoryContext and all its descendants.
 */
Datum
pg_stat_print_memory_context(PG_FUNCTION_ARGS)
{
	PrintMemoryContextStats(TopMemoryContext, 0);

	PG_RETURN_VOID();
}

/*
 * Print statistics about the named context and all its descendants.
 */
static void
PrintMemoryContextStats(MemoryContext context, int level)
{
	MemoryContext child;

	if (context == NULL)
		return;

#if PG_VERSION_NUM >= 90600
	(*context->methods->stats) (context, level, true, NULL);
#else
	(*context->methods->stats) (context, level);
#endif

	for (child = context->firstchild; child != NULL; child = child->nextchild)
		PrintMemoryContextStats(child, level + 1);
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

#if PG_VERSION_NUM >= 90500
/*
 * Create a compressed version of a text datum
 */
Datum
pglz_compress_text(PG_FUNCTION_ARGS)
{
	text		*source = PG_GETARG_TEXT_P(0);
	PG_RETURN_BYTEA_P(PGLZCompress((struct varlena *) source));
}

/*
 * Create a compressed version of a bytea datum
 */
Datum
pglz_compress_bytea(PG_FUNCTION_ARGS)
{
	bytea	*source = PG_GETARG_BYTEA_P(0);
	PG_RETURN_BYTEA_P(PGLZCompress((struct varlena *) source));
}

/*
 * Create a compressed version of a varlena datum
 */
static struct varlena *
PGLZCompress(struct varlena *source)
{
	struct varlena	*dest;
	int32	orig_len = VARSIZE(source) - VARHDRSZ;
	int32	len;

	dest = (struct varlena *) palloc(PGLZ_MAX_OUTPUT(orig_len) + PGLZ_HDRSZ);

	/*
	 * We recheck the actual size even if pglz_compress() reports success,
	 * because it might be satisfied with having saved as little as one byte
	 * in the compressed data.
	 */
	len = pglz_compress(VARDATA(source), orig_len,
						PGLZ_RAWDATA(dest), PGLZ_strategy_default);
	if (len >= 0 && len < orig_len)
	{
		/* successful compression */
		PGLZ_SET_RAWSIZE_COMPRESSED(dest, orig_len);
		SET_VARSIZE(dest, len + PGLZ_HDRSZ);
	}
	else
	{
		/* incompressible data */
		PGLZ_SET_RAWSIZE(dest, orig_len);
		SET_VARSIZE(dest, orig_len + PGLZ_HDRSZ);
		memcpy(PGLZ_RAWDATA(dest), VARDATA(source), orig_len);
	}

	return dest;
}

/*
 * Decompress a compressed version of bytea into text.
 */
Datum
pglz_decompress_text(PG_FUNCTION_ARGS)
{
	bytea	*source = PG_GETARG_BYTEA_P(0);
	PG_RETURN_TEXT_P(PGLZDecompress((struct varlena *) source));
}

/*
 * Decompress a compressed version of bytea into bytea.
 */
Datum
pglz_decompress_bytea(PG_FUNCTION_ARGS)
{
	bytea	*source = PG_GETARG_BYTEA_P(0);
	PG_RETURN_BYTEA_P(PGLZDecompress((struct varlena *) source));
}

/*
 * Decompress a compressed version of a varlena datum.
 */
static struct varlena *
PGLZDecompress(struct varlena *source)
{
	struct varlena	*dest;
	int32	orig_len = PGLZ_RAWSIZE(source);

	dest = (struct varlena *) palloc(orig_len + VARHDRSZ);
	SET_VARSIZE(dest, orig_len + VARHDRSZ);

	if (!PGLZ_IS_COMPRESSED(source))
		memcpy(VARDATA(dest), PGLZ_RAWDATA(source), orig_len);
	else
	{
		if (pglz_decompress(PGLZ_RAWDATA(source),
							VARSIZE(source) - PGLZ_HDRSZ,
							VARDATA(dest), orig_len) < 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 (errmsg("specified compressed data is corrupted"),
					  errhint("Make sure compressed data that pglz_compress or pglz_compress_bytea created is specified."))));
	}

	return dest;
}
#endif	/* PG_VERSION_NUM >= 90500 */
