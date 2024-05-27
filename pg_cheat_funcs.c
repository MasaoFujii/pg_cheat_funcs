/*-------------------------------------------------------------------------
 *
 * pg_cheat_funcs.c
 *   provides various cheat (but useful) functions
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/clog.h"
#if PG_VERSION_NUM >= 90300
#include "access/htup_details.h"
#endif
#include "access/subtrans.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#if PG_VERSION_NUM >= 150000
#include "access/xlogrecovery.h"
#endif
#include "access/transam.h"
#include "catalog/catalog.h"
#include "catalog/pg_type.h"
#include "commands/prepare.h"
#if PG_VERSION_NUM >= 90500
#include "common/pg_lzcompress.h"
#endif
#if PG_VERSION_NUM >= 90300
#include "common/relpath.h"
#endif
#if PG_VERSION_NUM >= 100000
#include "common/saslprep.h"
#endif
#include "conv/euc_jp_to_utf8.extra"
#if PG_VERSION_NUM >= 100000
#include "conv/euc_jp_to_utf8.radix"
#else
#include "conv/euc_jp_to_utf8.map"
#endif
#include "funcapi.h"
#include "libpq/auth.h"
#include "libpq/libpq-be.h"
#if PG_VERSION_NUM < 90300
#include "libpq/pqsignal.h"
#endif
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#if PG_VERSION_NUM >= 90200
#include "replication/walsender_private.h"
#endif
#include "storage/fd.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#if PG_VERSION_NUM >= 160000
#include "utils/guc.h"
#endif
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#if PG_VERSION_NUM >= 90400
#include "utils/pg_lsn.h"
#endif
#include "utils/timestamp.h"
#include "utils/plancache.h"
#include "utils/varbit.h"
#if PG_VERSION_NUM >= 130000
#include "utils/xid8.h"
#endif

PG_MODULE_MAGIC;

/* GUC variables */
static bool cheat_log_memory_context = false;
static bool cheat_hide_appname = false;
static char *cheat_hidden_appname = NULL;
static bool cheat_log_session_start_options = false;
static int	cheat_scheduling_priority = 0;
static bool cheat_exit_on_segv = false;

/* Saved hook values in case of unload */
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;
static ClientAuthentication_hook_type prev_ClientAuthentication = NULL;

static int	ExitOnSegvErrorLevel = FATAL;

#if PG_VERSION_NUM >= 90500
/* The information at the start of the compressed data */
typedef struct pglz_header
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	int32		rawsize;
}			pglz_header;

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
#endif							/* PG_VERSION_NUM >= 90500 */

/*
 * pg_stat_get_memory_context function is available only in
 * between 9.6 and 13. pg_stat_print_memory_context function
 * is available only in 13 or before.
 */
#if PG_VERSION_NUM < 140000
#if PG_VERSION_NUM >= 90600
PG_FUNCTION_INFO_V1(pg_stat_get_memory_context);
#endif							/* PG_VERSION_ NUM >= 90600 */
PG_FUNCTION_INFO_V1(pg_stat_print_memory_context);
#endif							/* PG_VERSION_NUM < 140000 */

#if PG_VERSION_NUM >= 90200
PG_FUNCTION_INFO_V1(pg_cached_plan_source);
#endif
PG_FUNCTION_INFO_V1(pg_signal_process);
PG_FUNCTION_INFO_V1(pg_get_priority);
PG_FUNCTION_INFO_V1(pg_set_priority);
PG_FUNCTION_INFO_V1(pg_segmentation_fault);
PG_FUNCTION_INFO_V1(pg_process_config_file);
#if PG_VERSION_NUM >= 90400
PG_FUNCTION_INFO_V1(pg_xlogfile_name);
#if PG_VERSION_NUM < 130000
PG_FUNCTION_INFO_V1(pg_lsn_larger);
PG_FUNCTION_INFO_V1(pg_lsn_smaller);
#endif
PG_FUNCTION_INFO_V1(pg_stat_get_syncrep_waiters);
PG_FUNCTION_INFO_V1(pg_wait_syncrep);
PG_FUNCTION_INFO_V1(pg_refresh_snapshot);
#endif
PG_FUNCTION_INFO_V1(pg_set_next_xid);
PG_FUNCTION_INFO_V1(pg_xid_assignment);
#if PG_VERSION_NUM >= 130000
PG_FUNCTION_INFO_V1(pg_xid_to_xid8);
#endif
PG_FUNCTION_INFO_V1(pg_set_next_oid);
PG_FUNCTION_INFO_V1(pg_oid_assignment);
#if PG_VERSION_NUM < 160000
PG_FUNCTION_INFO_V1(pg_advance_vacuum_cleanup_age);
#endif
PG_FUNCTION_INFO_V1(pg_checkpoint);
#if PG_VERSION_NUM < 120000
PG_FUNCTION_INFO_V1(pg_promote);
#endif
PG_FUNCTION_INFO_V1(pg_recovery_settings);
PG_FUNCTION_INFO_V1(pg_show_primary_conninfo);
PG_FUNCTION_INFO_V1(pg_postmaster_pid);
PG_FUNCTION_INFO_V1(pg_backend_start_time);
PG_FUNCTION_INFO_V1(pg_tablespace_version_directory);
PG_FUNCTION_INFO_V1(pg_file_write_binary);
#if PG_VERSION_NUM >= 90400
PG_FUNCTION_INFO_V1(pg_file_fsync);
#endif							/* PG_VERSION_NUM >= 90400 */
PG_FUNCTION_INFO_V1(to_octal32);
PG_FUNCTION_INFO_V1(to_octal64);
PG_FUNCTION_INFO_V1(pg_text_to_hex);
PG_FUNCTION_INFO_V1(pg_hex_to_text);
PG_FUNCTION_INFO_V1(pg_chr);
PG_FUNCTION_INFO_V1(pg_eucjp);
PG_FUNCTION_INFO_V1(pg_euc_jp_to_utf8);
#if PG_VERSION_NUM >= 100000
PG_FUNCTION_INFO_V1(pg_cheat_saslprep);
#endif
PG_FUNCTION_INFO_V1(pg_advisory_xact_unlock_int8);
PG_FUNCTION_INFO_V1(pg_advisory_xact_unlock_shared_int8);
PG_FUNCTION_INFO_V1(pg_advisory_xact_unlock_int4);
PG_FUNCTION_INFO_V1(pg_advisory_xact_unlock_shared_int4);

/*
 * The function prototypes are created as a part of PG_FUNCTION_INFO_V1
 * macro since 9.4, and hence the declaration of the function prototypes
 * here is necessary only for 9.3 or before.
 */
#if PG_VERSION_NUM < 90400
Datum		pg_stat_print_memory_context(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90200
Datum		pg_cached_plan_source(PG_FUNCTION_ARGS);
#endif
Datum		pg_signal_process(PG_FUNCTION_ARGS);
Datum		pg_get_priority(PG_FUNCTION_ARGS);
Datum		pg_set_priority(PG_FUNCTION_ARGS);
Datum		pg_segmentation_fault(PG_FUNCTION_ARGS);
Datum		pg_process_config_file(PG_FUNCTION_ARGS);
Datum		pg_set_next_xid(PG_FUNCTION_ARGS);
Datum		pg_xid_assignment(PG_FUNCTION_ARGS);
Datum		pg_set_next_oid(PG_FUNCTION_ARGS);
Datum		pg_oid_assignment(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM < 160000
Datum		pg_advance_vacuum_cleanup_age(PG_FUNCTION_ARGS);
#endif
Datum		pg_checkpoint(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM < 120000
Datum		pg_promote(PG_FUNCTION_ARGS);
#endif
Datum		pg_recovery_settings(PG_FUNCTION_ARGS);
Datum		pg_show_primary_conninfo(PG_FUNCTION_ARGS);
Datum		pg_postmaster_pid(PG_FUNCTION_ARGS);
Datum		pg_backend_start_time(PG_FUNCTION_ARGS);
Datum		pg_tablespace_version_directory(PG_FUNCTION_ARGS);
Datum		pg_file_write_binary(PG_FUNCTION_ARGS);
Datum		to_octal32(PG_FUNCTION_ARGS);
Datum		to_octal64(PG_FUNCTION_ARGS);
Datum		pg_text_to_hex(PG_FUNCTION_ARGS);
Datum		pg_hex_to_text(PG_FUNCTION_ARGS);
Datum		pg_chr(PG_FUNCTION_ARGS);
Datum		pg_eucjp(PG_FUNCTION_ARGS);
Datum		pg_euc_jp_to_utf8(PG_FUNCTION_ARGS);
Datum		pg_advisory_xact_unlock_int8(PG_FUNCTION_ARGS);
Datum		pg_advisory_xact_unlock_shared_int8(PG_FUNCTION_ARGS);
Datum		pg_advisory_xact_unlock_int4(PG_FUNCTION_ARGS);
Datum		pg_advisory_xact_unlock_shared_int4(PG_FUNCTION_ARGS);
#endif

void		_PG_init(void);
void		_PG_fini(void);

static void CheatExecutorEnd(QueryDesc *queryDesc);
static void CheatClientAuthentication(Port *port, int status);

#if PG_VERSION_NUM < 140000
#if PG_VERSION_NUM >= 90600
static void PutMemoryContextStatsTupleStore(Tuplestorestate *tupstore,
											TupleDesc tupdesc, MemoryContext context,
											MemoryContext parent, int level);
#endif							/* PG_VERSION_NUM >= 90600 */
static void PrintMemoryContextStats(MemoryContext context, int level);
#endif							/* PG_VERSION_NUM < 140000 */

static int	GetSignalByName(char *signame);
static ReturnSetInfo *InitReturnSetFunc(FunctionCallInfo fcinfo);
#if PG_VERSION_NUM >= 90400
static const char *SyncRepGetWaitModeString(int mode);
#endif
static int	GetProcessPriority(int pid, int elevel);
static void SetProcessPriority(int pid, int priority, int elevel);
static void ExitOnSegvHandler(SIGNAL_ARGS);
static void CheckPostgresPid(int pid);
static bool IsWalSenderPid(int pid);
static bool IsWalReceiverPid(int pid);
#if PG_VERSION_NUM < 120000
static void CreateEmptyFile(const char *filepath);
#endif
static text *Bits8GetText(bits8 b1, bits8 b2, bits8 c3, int len);

static void assign_scheduling_priority(int newval, void *extra);
static const char *show_scheduling_priority(void);
static void assign_exit_on_segv(bool newval, void *extra);

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
							 &cheat_log_memory_context,
							 false,
							 PGC_SUSET,
							 0,
							 NULL,
							 NULL,
							 NULL);

	DefineCustomBoolVariable("pg_cheat_funcs.hide_appname",
							 "Hide client's application_name from view.",
							 NULL,
							 &cheat_hide_appname,
							 false,
							 PGC_SIGHUP,
							 0,
							 NULL,
							 NULL,
							 NULL);

	DefineCustomStringVariable("pg_cheat_funcs.hidden_appname",
							   "Hidden client's application_name.",
							   NULL,
							   &cheat_hidden_appname,
							   "",
							   PGC_USERSET,
							   0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomBoolVariable("pg_cheat_funcs.log_session_start_options",
							 "Log options sent to the server at connection start.",
							 NULL,
							 &cheat_log_session_start_options,
							 false,
#if PG_VERSION_NUM >= 90500
							 PGC_SU_BACKEND,
#else
							 PGC_BACKEND,
#endif
							 0,
							 NULL,
							 NULL,
							 NULL);

	DefineCustomIntVariable("pg_cheat_funcs.scheduling_priority",
							"Set the scheduling priority of PostgreSQL server process.",
							NULL,
							&cheat_scheduling_priority,
							0,
							-20,
							19,
							PGC_USERSET,
							0,
							NULL,
							assign_scheduling_priority,
							show_scheduling_priority);

	DefineCustomBoolVariable("pg_cheat_funcs.exit_on_segv",
							 "Terminate session on segmentation fault.",
							 NULL,
							 &cheat_exit_on_segv,
							 false,
							 PGC_USERSET,
							 0,
							 NULL,
							 assign_exit_on_segv,
							 NULL);

	EmitWarningsOnPlaceholders("pg_cheat_funcs");

	/* Install hooks. */
	prev_ExecutorEnd = ExecutorEnd_hook;
	ExecutorEnd_hook = CheatExecutorEnd;
	prev_ClientAuthentication = ClientAuthentication_hook;
	ClientAuthentication_hook = CheatClientAuthentication;
}

/*
 * Module unload callback
 */
void
_PG_fini(void)
{
	/* Uninstall hooks. */
	ExecutorEnd_hook = prev_ExecutorEnd;
	ClientAuthentication_hook = prev_ClientAuthentication;
}

/*
 * ExecutorEnd hook
 */
static void
CheatExecutorEnd(QueryDesc *queryDesc)
{
	if (prev_ExecutorEnd)
		prev_ExecutorEnd(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);

	/* Print statistics about TopMemoryContext and all its descendants */
	if (cheat_log_memory_context)
#if PG_VERSION_NUM >= 140000
		HandleLogMemoryContextInterrupt();
#else
		PrintMemoryContextStats(TopMemoryContext, 0);
#endif
}

#if PG_VERSION_NUM >= 130000
#define	CHEAT_LNEXT(l, c)	lnext(l, c)
#else
#define	CHEAT_LNEXT(l, c)	lnext(c)
#endif

/*
 * ClientAuthentication hook
 */
static void
CheatClientAuthentication(Port *port, int status)
{
	if (prev_ClientAuthentication)
		prev_ClientAuthentication(port, status);

	/* Hide client's application_name from view */
	if (cheat_hide_appname)
	{
		List	   *gucopts = port->guc_options;
		ListCell   *cell;
		ListCell   *next;

		for (cell = list_head(gucopts); cell != NULL; cell = next)
		{
			char	   *name = lfirst(cell);
			char	   *value;

			next = CHEAT_LNEXT(port->guc_options, cell);

			if (strcmp(name, "application_name") != 0)
			{
				next = CHEAT_LNEXT(port->guc_options, next);
				continue;
			}

			cell = next;
			next = CHEAT_LNEXT(port->guc_options, next);
			value = lfirst(cell);
			SetConfigOption("pg_cheat_funcs.hidden_appname", value,
							PGC_USERSET, PGC_S_CLIENT);
			strcpy(value, "");	/* reset application_name to an empty */
		}
	}

	/* Log options sent to the server at connection start */
	if (cheat_log_session_start_options)
	{
		ListCell   *cell = list_head(port->guc_options);

		while (cell)
		{
			char	   *name;
			char	   *value;

			name = lfirst(cell);
			cell = CHEAT_LNEXT(port->guc_options, cell);

			value = lfirst(cell);
			cell = CHEAT_LNEXT(port->guc_options, cell);

			ereport(LOG,
					(errmsg("session-start options: %s = %s", name, value)));
		}
	}
}

/*
 * Initialize function returning a tuplestore (multiple rows).
 */
static ReturnSetInfo *
InitReturnSetFunc(FunctionCallInfo fcinfo)
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

	return rsinfo;
}

#if PG_VERSION_NUM < 140000
#if PG_VERSION_NUM >= 90600
/*
 * Return statistics about all memory contexts.
 */
Datum
pg_stat_get_memory_context(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;

	rsinfo = InitReturnSetFunc(fcinfo);
	tupdesc = rsinfo->setDesc;
	tupstore = rsinfo->setResult;

	PutMemoryContextStatsTupleStore(tupstore, tupdesc,
									TopMemoryContext, NULL, 0);

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
#if PG_VERSION_NUM >= 110000
	(*context->methods->stats) (context, NULL, (void *) &level, &stat);
#else
	(*context->methods->stats) (context, level, false, &stat);
#endif

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
#endif							/* PG_VERSION_NUM >= 90600 */

/*
 * Print statistics about TopMemoryContext and all its descendants.
 */
Datum
pg_stat_print_memory_context(PG_FUNCTION_ARGS)
{
	PrintMemoryContextStats(TopMemoryContext, 0);

	PG_RETURN_VOID();
}

#if  PG_VERSION_NUM >= 110000
/*
 * Simple version of MemoryContextStatsPrint() added in PostgreSQL 11.
 */
static void
SimpleMemoryContextStatsPrint(MemoryContext context, void *passthru,
							  const char *stats_string)
{
	int			level = *(int *) passthru;
	const char *name = context->name;
	int			i;

	for (i = 0; i < level; i++)
		fprintf(stderr, "  ");
	fprintf(stderr, "%s: %s\n", name, stats_string);
}
#endif							/* PG_VERSION_NUM >= 110000 */

/*
 * Print statistics about the named context and all its descendants.
 */
static void
PrintMemoryContextStats(MemoryContext context, int level)
{
	MemoryContext child;

	if (context == NULL)
		return;

#if  PG_VERSION_NUM >= 110000
	(*context->methods->stats) (context, SimpleMemoryContextStatsPrint,
								(void *) &level, NULL);
#elif PG_VERSION_NUM >= 90600
	(*context->methods->stats) (context, level, true, NULL);
#else
	(*context->methods->stats) (context, level);
#endif

	for (child = context->firstchild; child != NULL; child = child->nextchild)
		PrintMemoryContextStats(child, level + 1);
}
#endif							/* PG_VERSION_NUM < 140000 */

#if PG_VERSION_NUM >= 90200
/*
 * Return information about the cached plan of the specified
 * prepared statement.
 */
Datum
pg_cached_plan_source(PG_FUNCTION_ARGS)
{
	char	   *stmt_name = text_to_cstring(PG_GETARG_TEXT_P(0));
	TupleDesc	tupdesc;
	Datum		values[5];
	bool		nulls[5];
	PreparedStatement *stmt;
	CachedPlanSource *plansource;

	/* Look it up in the hash table */
	stmt = FetchPreparedStatement(stmt_name, true);
	plansource = stmt->plansource;

	/* Initialize values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Initialize attributes information in the tuple descriptor */
#if PG_VERSION_NUM >= 120000
	tupdesc = CreateTemplateTupleDesc(5);
#else
	tupdesc = CreateTemplateTupleDesc(5, false);
#endif
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "generic_cost",
					   FLOAT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "total_custom_cost",
					   FLOAT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "num_custom_plans",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "force_generic",
					   BOOLOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5, "force_custom",
					   BOOLOID, -1, 0);

	BlessTupleDesc(tupdesc);

	/* Fill values and NULLs */
	values[0] = Float8GetDatum(plansource->generic_cost);
	values[1] = Float8GetDatum(plansource->total_custom_cost);
	values[2] = Int32GetDatum(plansource->num_custom_plans);
	if (plansource->cursor_options & CURSOR_OPT_GENERIC_PLAN)
		values[3] = true;
	if (plansource->cursor_options & CURSOR_OPT_CUSTOM_PLAN)
		values[4] = true;

	/* Returns the record as Datum */
	PG_RETURN_DATUM(HeapTupleGetDatum(
									  heap_form_tuple(tupdesc, values, nulls)));
}
#endif							/* PG_VERSION_NUM >= 90200 */

/*
 * Send a signal to PostgreSQL server process.
 */
Datum
pg_signal_process(PG_FUNCTION_ARGS)
{
	int			pid = PG_GETARG_INT32(0);
	char	   *signame = text_to_cstring(PG_GETARG_TEXT_P(1));
	int			sig = GetSignalByName(signame);

	CheckPostgresPid(pid);
	if (kill(pid, sig))
		ereport(ERROR,
				(errmsg("could not send signal to process %d: %m", pid)));

	PG_RETURN_VOID();
}

/*
 * Check to see if a given pid is a running postmaster, backend, walsender
 * or walreceiver.
 */
static void
CheckPostgresPid(int pid)
{
	if (PostmasterPid != pid && !IsBackendPid(pid) &&
		!IsWalSenderPid(pid) && !IsWalReceiverPid(pid))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("PID %d is not a PostgreSQL server process", pid))));
}

/*
 * Get the scheduling priority of PostgreSQL server process.
 */
Datum
pg_get_priority(PG_FUNCTION_ARGS)
{
	int			pid = PG_GETARG_INT32(0);
	int			priority;

	CheckPostgresPid(pid);
	priority = GetProcessPriority(pid, ERROR);
	PG_RETURN_INT32(priority);
}

/*
 * Get the scheduling priority of the specified process.
 */
static int
GetProcessPriority(int pid, int elevel)
{
	int			priority = 0;
	int			save_errno = errno;

	errno = 0;
	priority = getpriority(PRIO_PROCESS, pid);
	if (errno != 0)
		ereport(elevel,
				(errmsg("could not get the scheduling priority of process %d: %m", pid)));
	errno = save_errno;

	return priority;
}

/*
 * Set the scheduling priority of PostgreSQL server process.
 */
Datum
pg_set_priority(PG_FUNCTION_ARGS)
{
	int			pid = PG_GETARG_INT32(0);
	int			priority = PG_GETARG_INT32(1);

	CheckPostgresPid(pid);
	SetProcessPriority(pid, priority, ERROR);
	PG_RETURN_VOID();
}

/*
 * Set the scheduling priority of the specified process.
 */
static void
SetProcessPriority(int pid, int priority, int elevel)
{
	int			save_errno = errno;

	if (setpriority(PRIO_PROCESS, pid, priority) != 0)
		ereport(elevel,
				(errmsg("could not set the scheduling priority of process %d to %d: %m", pid, priority)));
	errno = save_errno;
}

static void
assign_scheduling_priority(int newval, void *extra)
{
	/* See comments in assign_tcp_keepalives_idle in PostgreSQL source */
	SetProcessPriority(getpid(), newval, WARNING);
}

static const char *
show_scheduling_priority(void)
{
	/* See comments in assign_tcp_keepalives_idle in PostgreSQL source */
	static char nbuf[16];

	snprintf(nbuf, sizeof(nbuf), "%d", GetProcessPriority(getpid(), WARNING));
	return nbuf;
}

/*
 * Cause segmentation fault.
 *
 * If treat_fatal_as_error is true, make the SEGV handler report ERROR
 * instead of FATAL. This is intended for testing.
 */
Datum
pg_segmentation_fault(PG_FUNCTION_ARGS)
{
	bool		treat_fatal_as_error = PG_GETARG_BOOL(0);
	int		   *ptr = NULL;

	if (treat_fatal_as_error)
		ExitOnSegvErrorLevel = ERROR;

	PG_TRY();
	{
		*ptr = 10;
	}
	PG_CATCH();
	{
		ExitOnSegvErrorLevel = FATAL;
		PG_RE_THROW();
	}
	PG_END_TRY();

	PG_RETURN_VOID();
}

static void
ExitOnSegvHandler(SIGNAL_ARGS)
{
	ereport(ExitOnSegvErrorLevel,
			(errcode(ERRCODE_INTERNAL_ERROR),
			 errmsg("terminating PostgreSQL server process due to segmentation fault")));
}

static void
assign_exit_on_segv(bool newval, void *extra)
{
	if (newval)
		pqsignal(SIGSEGV, ExitOnSegvHandler);
	else
		pqsignal(SIGSEGV, SIG_DFL);
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

	return 0;					/* keep compiler quiet */
}

/*
 * Is a given pid a running walsender?
 */
static bool
IsWalSenderPid(int pid)
{
	int			i;

	if (pid == 0)
		return false;

	for (i = 0; i < max_wal_senders; i++)
	{
		WalSnd	   *walsnd = &WalSndCtl->walsnds[i];

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
 * Read and process the configuration file.
 */
Datum
pg_process_config_file(PG_FUNCTION_ARGS)
{
	ProcessConfigFile(PGC_SIGHUP);
	PG_RETURN_VOID();
}

#if PG_VERSION_NUM >= 90400
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
#if PG_VERSION_NUM >= 150000
	TimeLineID	ThisTimeLineID = 0;
#endif

	if (!recovery && RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("recovery is in progress"),
				 errhint("pg_xlogfile_name() cannot be executed during recovery.")));

#if PG_VERSION_NUM >= 150000
	/* Use the currently-replaying timeline while in recovery */
	if (RecoveryInProgress())
		(void) GetXLogReplayRecPtr(&ThisTimeLineID);
	else
		ThisTimeLineID = GetWALInsertionTimeLine();
#endif

#if PG_VERSION_NUM >= 110000
	XLByteToPrevSeg(locationpoint, xlogsegno, wal_segment_size);
	XLogFileName(xlogfilename, ThisTimeLineID, xlogsegno, wal_segment_size);
#else
	XLByteToPrevSeg(locationpoint, xlogsegno);
	XLogFileName(xlogfilename, ThisTimeLineID, xlogsegno);
#endif

	PG_RETURN_TEXT_P(cstring_to_text(xlogfilename));
}

/*
 * Create min() and max() aggregates for pg_lsn only in 12 or before.
 * They are supported in core since 13.
 */
#if PG_VERSION_NUM < 130000
/*
 * Return larger pg_lsn value.
 */
Datum
pg_lsn_larger(PG_FUNCTION_ARGS)
{
	XLogRecPtr	lsn1 = PG_GETARG_LSN(0);
	XLogRecPtr	lsn2 = PG_GETARG_LSN(1);
	XLogRecPtr	result;

	result = ((lsn1 > lsn2) ? lsn1 : lsn2);

	PG_RETURN_LSN(result);
}

/*
 * Return smaller pg_lsn value.
 */
Datum
pg_lsn_smaller(PG_FUNCTION_ARGS)
{
	XLogRecPtr	lsn1 = PG_GETARG_LSN(0);
	XLogRecPtr	lsn2 = PG_GETARG_LSN(1);
	XLogRecPtr	result;

	result = ((lsn1 < lsn2) ? lsn1 : lsn2);

	PG_RETURN_LSN(result);
}
#endif							/* PG_VERSION_NUM < 130000 */

/*
 * Return statistics about all syncrep waiters.
 */
Datum
pg_stat_get_syncrep_waiters(PG_FUNCTION_ARGS)
{
#define PG_STAT_GET_SYNCREP_WAITERS_COLS	3
	ReturnSetInfo *rsinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	int			i;

	rsinfo = InitReturnSetFunc(fcinfo);
	tupdesc = rsinfo->setDesc;
	tupstore = rsinfo->setResult;

	LWLockAcquire(SyncRepLock, LW_SHARED);
	for (i = 0; i < NUM_SYNC_REP_WAIT_MODE; i++)
	{
#if PG_VERSION_NUM >= 160000
		dlist_iter	iter;

		dlist_foreach(iter, &WalSndCtl->SyncRepQueue[i])
		{
			PGPROC	   *proc = dlist_container(PGPROC, syncRepLinks, iter.cur);
#else
		PGPROC	   *proc = NULL;

		proc = (PGPROC *) SHMQueueNext(&(WalSndCtl->SyncRepQueue[i]),
									   &(WalSndCtl->SyncRepQueue[i]),
									   offsetof(PGPROC, syncRepLinks));
		while (proc)
		{
#endif
			Datum		values[PG_STAT_GET_SYNCREP_WAITERS_COLS];
			bool		nulls[PG_STAT_GET_SYNCREP_WAITERS_COLS];

			memset(nulls, 0, sizeof(nulls));
			values[0] = Int32GetDatum(proc->pid);
			values[1] = LSNGetDatum(proc->waitLSN);
			values[2] = CStringGetTextDatum(SyncRepGetWaitModeString(i));
			tuplestore_putvalues(tupstore, tupdesc, values, nulls);

#if PG_VERSION_NUM < 160000
			proc = (PGPROC *) SHMQueueNext(&(WalSndCtl->SyncRepQueue[i]),
										   &(proc->syncRepLinks),
										   offsetof(PGPROC, syncRepLinks));
#endif
		}
	}
	LWLockRelease(SyncRepLock);

	return (Datum) 0;
}

/*
 * Return a string constant representing the SyncRepWaitMode.
 */
static const char *
SyncRepGetWaitModeString(int mode)
{
	switch (mode)
	{
		case SYNC_REP_NO_WAIT:
			return "no wait";
		case SYNC_REP_WAIT_WRITE:
			return "write";
		case SYNC_REP_WAIT_FLUSH:
			return "flush";
#if PG_VERSION_NUM >= 90600
		case SYNC_REP_WAIT_APPLY:
			return "apply";
#endif
	}
	return "unknown";
}

/*
 * Wait for synchronous replication.
 */
Datum
pg_wait_syncrep(PG_FUNCTION_ARGS)
{
	XLogRecPtr	recptr = PG_GETARG_LSN(0);

#if PG_VERSION_NUM >= 90600
	SyncRepWaitForLSN(recptr, true);
#else
	SyncRepWaitForLSN(recptr);
#endif

	PG_RETURN_VOID();
}

/*
 * Forcibly refresh the current snapshot.
 */
Datum
pg_refresh_snapshot(PG_FUNCTION_ARGS)
{
	int			save_XactIsoLevel;
	bool		XactIsoLevelNeedsReset = false;

	if (FirstSnapshotSet && IsolationUsesXactSnapshot())
	{
		save_XactIsoLevel = XactIsoLevel;
		XactIsoLevel = XACT_READ_COMMITTED;
		XactIsoLevelNeedsReset = true;
	}

	GetTransactionSnapshot();

	if (XactIsoLevelNeedsReset)
		XactIsoLevel = save_XactIsoLevel;

	PG_RETURN_VOID();
}
#endif							/* PG_VERSION_NUM >= 90400 */

/*
 * ShmemVariableCache was renamed to TransamVariables in v17
 * (commit b31ba5310b).
 */
#if PG_VERSION_NUM >= 170000
#define ShmemVariableCache TransamVariables
#endif							/* PG_VERSION_NUM >= 170000 */

/*
 * Set and return the next transaction ID.
 */
Datum
pg_set_next_xid(PG_FUNCTION_ARGS)
{
	TransactionId xid = PG_GETARG_UINT32(0);
#if PG_VERSION_NUM >= 120000
	uint32		epoch;
#endif

	if (RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("recovery is in progress"),
				 errhint("pg_set_next_xid() cannot be executed during recovery.")));

	LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
#if PG_VERSION_NUM >= 140000
	epoch = EpochFromFullTransactionId(ShmemVariableCache->nextXid);
	ShmemVariableCache->nextXid
		= FullTransactionIdFromEpochAndXid(epoch, xid);
#elif PG_VERSION_NUM >= 120000
	epoch = EpochFromFullTransactionId(ShmemVariableCache->nextFullXid);
	ShmemVariableCache->nextFullXid
		= FullTransactionIdFromEpochAndXid(epoch, xid);
#else
	ShmemVariableCache->nextXid = xid;
#endif
	LWLockRelease(XidGenLock);

	/*
	 * Make sure that CLOG has room for the given next XID. These macros are
	 * borrowed from src/backend/access/transam/clog.c.
	 */
#define CLOG_XACTS_PER_BYTE 4
#define CLOG_XACTS_PER_PAGE (BLCKSZ * CLOG_XACTS_PER_BYTE)
#define TransactionIdToPgIndex(xid) ((xid) % (TransactionId) CLOG_XACTS_PER_PAGE)
	if (TransactionIdToPgIndex(xid) != 0 &&
		!TransactionIdEquals(xid, FirstNormalTransactionId))
		ExtendCLOG(xid - TransactionIdToPgIndex(xid));

	/*
	 * Make sure that SUBTRANS has room for the given next XID. These macros
	 * are borrowed from src/backend/access/transam/subtrans.c.
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
#if PG_VERSION_NUM >= 120000
	FullTransactionId full_xid;
#endif

	/* Initialise values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Initialise attributes information in the tuple descriptor */
#if PG_VERSION_NUM >= 120000
	tupdesc = CreateTemplateTupleDesc(PG_XID_ASSIGNMENT_COLS);
#else
	tupdesc = CreateTemplateTupleDesc(PG_XID_ASSIGNMENT_COLS, false);
#endif
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
	LWLockAcquire(XidGenLock, LW_SHARED);
#if PG_VERSION_NUM >= 140000
	full_xid = ShmemVariableCache->nextXid;
	nextXid = XidFromFullTransactionId(full_xid);
#elif PG_VERSION_NUM >= 120000
	full_xid = ShmemVariableCache->nextFullXid;
	nextXid = XidFromFullTransactionId(full_xid);
#else
	nextXid = ShmemVariableCache->nextXid;
#endif
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

#if PG_VERSION_NUM >= 130000
/*
 * This is copy-pasted from widen_snapshot_xid in PostgreSQL source.
 */
static FullTransactionId
widen_snapshot_xid(TransactionId xid, FullTransactionId next_fxid)
{
	TransactionId next_xid = XidFromFullTransactionId(next_fxid);
	uint32		epoch = EpochFromFullTransactionId(next_fxid);

	if (!TransactionIdIsNormal(xid))
		return FullTransactionIdFromEpochAndXid(0, xid);

	if (xid > next_xid)
		epoch--;

	return FullTransactionIdFromEpochAndXid(epoch, xid);
}

/*
 * Convert the specified TransactionId (with 32-bit type xid) to
 * FullTransactionId (with 64-bit type xid8).
 */
Datum
pg_xid_to_xid8(PG_FUNCTION_ARGS)
{
#if PG_VERSION_NUM >= 140000
	TransactionId xid = PG_GETARG_TRANSACTIONID(0);
#else
	TransactionId xid = PG_GETARG_UINT32(0);
#endif
	FullTransactionId fxid;

	fxid = widen_snapshot_xid(xid, ReadNextFullTransactionId());
	PG_RETURN_FULLTRANSACTIONID(fxid);
}
#endif							/* PG_VERSION_NUM >= 130000 */

/*
 * Set and return the next object ID.
 */
Datum
pg_set_next_oid(PG_FUNCTION_ARGS)
{
	Oid			oid = PG_GETARG_OID(0);
	Oid			result;

	if (RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("recovery is in progress"),
				 errhint("pg_set_next_oid() cannot be executed during recovery.")));

#define VAR_OID_PREFETCH		8192
	LWLockAcquire(OidGenLock, LW_EXCLUSIVE);
	ShmemVariableCache->nextOid =
		(oid < ((Oid) FirstNormalObjectId)) ? FirstNormalObjectId : oid;
	XLogPutNextOid(ShmemVariableCache->nextOid + VAR_OID_PREFETCH);
	ShmemVariableCache->oidCount = VAR_OID_PREFETCH;
	result = ShmemVariableCache->nextOid;
	LWLockRelease(OidGenLock);

	PG_RETURN_OID(result);
}

/*
 * Return information about OID assignment state.
 */
Datum
pg_oid_assignment(PG_FUNCTION_ARGS)
{
#define PG_OID_ASSIGNMENT_COLS	2
	TupleDesc	tupdesc;
	Datum		values[PG_OID_ASSIGNMENT_COLS];
	bool		nulls[PG_OID_ASSIGNMENT_COLS];
	Oid			nextOid;
	uint32		oidCount;

	/* Initialise values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Initialise attributes information in the tuple descriptor */
#if PG_VERSION_NUM >= 120000
	tupdesc = CreateTemplateTupleDesc(PG_OID_ASSIGNMENT_COLS);
#else
	tupdesc = CreateTemplateTupleDesc(PG_OID_ASSIGNMENT_COLS, false);
#endif
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "next_oid",
					   OIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "oid_count",
					   INT4OID, -1, 0);

	BlessTupleDesc(tupdesc);

	/* Take a lock to ensure value consistency */
	LWLockAcquire(OidGenLock, LW_SHARED);
	nextOid = ShmemVariableCache->nextOid;
	oidCount = ShmemVariableCache->oidCount;
	LWLockRelease(OidGenLock);

	/* Fetch values */
	values[0] = ObjectIdGetDatum(nextOid);
	values[1] = UInt32GetDatum(oidCount);

	/* Returns the record as Datum */
	PG_RETURN_DATUM(HeapTupleGetDatum(
									  heap_form_tuple(tupdesc, values, nulls)));
}

#if PG_VERSION_NUM < 160000
/*
 * Specify the number of transactions by which VACUUM and HOT updates
 * will advance cleanup of dead row versions.
 */
Datum
pg_advance_vacuum_cleanup_age(PG_FUNCTION_ARGS)
{
	static bool advanced = false;
	static int	save_cleanup_age = 0;
	static int	orig_cleanup_age = 0;

	if (!advanced || vacuum_defer_cleanup_age != save_cleanup_age)
		orig_cleanup_age = vacuum_defer_cleanup_age;

	if (PG_ARGISNULL(0))
	{
		vacuum_defer_cleanup_age = orig_cleanup_age;
		advanced = false;
	}
	else
	{
		vacuum_defer_cleanup_age = -PG_GETARG_INT32(0);
		advanced = true;
	}
	save_cleanup_age = vacuum_defer_cleanup_age;

	PG_RETURN_INT32(-vacuum_defer_cleanup_age);
}
#endif							/* PG_VERSION_NUM < 160000 */

/*
 * Perform a checkpoint.
 */
Datum
pg_checkpoint(PG_FUNCTION_ARGS)
{
	bool		fast = PG_GETARG_BOOL(0);
	bool		wait = PG_GETARG_BOOL(1);
	bool		force = PG_GETARG_BOOL(2);

	RequestCheckpoint((fast ? CHECKPOINT_IMMEDIATE : 0) |
					  (wait ? CHECKPOINT_WAIT : 0) |
					  (force ? CHECKPOINT_FORCE : 0));

	PG_RETURN_VOID();
}

/*
 * Create pg_promote() only in 11 or before.
 * It's supported in core since 12.
 */
#if PG_VERSION_NUM < 120000
/*
 * Promote the standby server.
 */
Datum
pg_promote(PG_FUNCTION_ARGS)
{
#if PG_VERSION_NUM >= 90300
	bool		fast = PG_GETARG_BOOL(0);
#endif

#define PROMOTE_SIGNAL_FILE		"promote"
#define FALLBACK_PROMOTE_SIGNAL_FILE "fallback_promote"

	if (!RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("server is not in standby mode")));

#if PG_VERSION_NUM >= 90300
	CreateEmptyFile(fast ? PROMOTE_SIGNAL_FILE :
					FALLBACK_PROMOTE_SIGNAL_FILE);
#else
	CreateEmptyFile(PROMOTE_SIGNAL_FILE);
#endif

	if (kill(PostmasterPid, SIGUSR1))
		ereport(ERROR,
				(errmsg("could not send signal to postmaster: %m")));

	PG_RETURN_VOID();
}

/*
 * Create empty file.
 */
static void
CreateEmptyFile(const char *filepath)
{
	FILE	   *fd;

	if ((fd = AllocateFile(filepath, "w")) == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create file \"%s\": %m", filepath)));

	if (FreeFile(fd))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write file \"%s\": %m", filepath)));
}
#endif							/* PG_VERSION_NUM < 120000 */

/*
 * Return a table of all parameter settings in recovery.conf.
 */
Datum
pg_recovery_settings(PG_FUNCTION_ARGS)
{
#define PG_RECOVERY_SETTINGS_COLS 2
#define RECOVERY_COMMAND_FILE	"recovery.conf"
	ReturnSetInfo *rsinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	FILE	   *fd;
	ConfigVariable *item,
			   *head = NULL,
			   *tail = NULL;

	rsinfo = InitReturnSetFunc(fcinfo);
	tupdesc = rsinfo->setDesc;
	tupstore = rsinfo->setResult;

	fd = AllocateFile(RECOVERY_COMMAND_FILE, "r");
	if (fd == NULL)
	{
		if (errno != ENOENT)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open recovery command file \"%s\": %m",
							RECOVERY_COMMAND_FILE)));
	}
	else
	{
		(void) ParseConfigFp(fd, RECOVERY_COMMAND_FILE, 0, ERROR, &head, &tail);
		FreeFile(fd);
	}

	for (item = head; item; item = item->next)
	{
		Datum		values[PG_RECOVERY_SETTINGS_COLS];
		bool		nulls[PG_RECOVERY_SETTINGS_COLS];

		memset(nulls, 0, sizeof(nulls));
		values[0] = CStringGetTextDatum(item->name);
		values[1] = CStringGetTextDatum(item->value);
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	return (Datum) 0;
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

/*
 * Return the PID of the postmaster process.
 */
Datum
pg_postmaster_pid(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(PostmasterPid);
}

/*
 * Return the time when this backend process was started.
 */
Datum
pg_backend_start_time(PG_FUNCTION_ARGS)
{
	if (MyProcPort)
#if PG_VERSION_NUM >= 120000
		PG_RETURN_TIMESTAMPTZ(MyStartTimestamp);
#else
		PG_RETURN_TIMESTAMPTZ(MyProcPort->SessionStartTime);
#endif
	else
		PG_RETURN_NULL();
}

/*
 * Return the name of major-version-specific tablespace subdirectory
 */
Datum
pg_tablespace_version_directory(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(TABLESPACE_VERSION_DIRECTORY));
}

/*
 * Write bytea data to the file.
 */
Datum
pg_file_write_binary(PG_FUNCTION_ARGS)
{
	FILE	   *f;
	char	   *filename;
	bytea	   *data;
	int64		count = 0;

	filename = text_to_cstring(PG_GETARG_TEXT_P(0));
	canonicalize_path(filename);
	data = PG_GETARG_BYTEA_P(1);

	if (!(f = fopen(filename, "wb")))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for writing: %m",
						filename)));

	if (VARSIZE(data) != 0)
	{
		count = fwrite(VARDATA(data), 1, VARSIZE(data) - VARHDRSZ, f);

		if (count != VARSIZE(data) - VARHDRSZ)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write file \"%s\": %m", filename)));
	}
	fclose(f);

	PG_RETURN_INT64(count);
}

#if PG_VERSION_NUM >= 90400
/*
 * Fsync a file or directory.
 */
Datum
pg_file_fsync(PG_FUNCTION_ARGS)
{
	char	   *filename;
	struct stat statbuf;

	filename = text_to_cstring(PG_GETARG_TEXT_P(0));
	canonicalize_path(filename);

	if (stat(filename, &statbuf) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat file \"%s\": %m", filename)));

	fsync_fname(filename, S_ISDIR(statbuf.st_mode));

	PG_RETURN_VOID();
}
#endif							/* PG_VERSION_NUM >= 90400 */

#define OCTALBASE 8
/*
 * Convert an int32 to a string containing a base 8 (octal) representation of
 * the number.
 */
Datum
to_octal32(PG_FUNCTION_ARGS)
{
	uint32		value = (uint32) PG_GETARG_INT32(0);
	char	   *ptr;
	const char *digits = "012345678";
	char		buf[32];		/* bigger than needed, but reasonable */

	ptr = buf + sizeof(buf) - 1;
	*ptr = '\0';

	do
	{
		*--ptr = digits[value % OCTALBASE];
		value /= OCTALBASE;
	} while (ptr > buf && value);

	PG_RETURN_TEXT_P(cstring_to_text(ptr));
}

/*
 * Convert an int64 to a string containing a base 8 (octal) representation of
 * the number.
 */
Datum
to_octal64(PG_FUNCTION_ARGS)
{
	uint64		value = (uint64) PG_GETARG_INT64(0);
	char	   *ptr;
	const char *digits = "012345678";
	char		buf[32];		/* bigger than needed, but reasonable */

	ptr = buf + sizeof(buf) - 1;
	*ptr = '\0';

	do
	{
		*--ptr = digits[value % OCTALBASE];
		value /= OCTALBASE;
	} while (ptr > buf && value);

	PG_RETURN_TEXT_P(cstring_to_text(ptr));
}

/*
 * Convert text to its equivalent hexadecimal representation.
 */
Datum
pg_text_to_hex(PG_FUNCTION_ARGS)
{
	text	   *s = PG_GETARG_TEXT_P(0);
	uint8	   *sp = (uint8 *) VARDATA(s);
	int			len = VARSIZE(s) - VARHDRSZ;
	int			i;
	char	   *result;
	char	   *r;

#define HEXDIG(z)	 ((z)<10 ? ((z)+'0') : ((z)-10+'a'))

	result = (char *) palloc(len * 2 + 1);
	r = result;
	for (i = 0; i < len; i++, sp++)
	{
		*r++ = HEXDIG((*sp) >> 4);
		*r++ = HEXDIG((*sp) & 0xF);
	}
	*r = '\0';

	PG_RETURN_TEXT_P(cstring_to_text(result));
}

/*
 * Convert hexadecimal representation to its equivalent text.
 */
Datum
pg_hex_to_text(PG_FUNCTION_ARGS)
{
	text	   *s = PG_GETARG_TEXT_P(0);
	uint8	   *sp = (uint8 *) VARDATA(s);
	int			len = VARSIZE(s) - VARHDRSZ;
	int			i;
	int			bc;
	char	   *result;
	char	   *r;
	uint8		x;

	result = (char *) palloc0(len / 2 + 1 + 1);
	r = result;
	for (bc = 0, i = 0; i < len; i++, sp++)
	{
		if (*sp >= '0' && *sp <= '9')
			x = (uint8) (*sp - '0');
		else if (*sp >= 'A' && *sp <= 'F')
			x = (uint8) (*sp - 'A') + 10;
		else if (*sp >= 'a' && *sp <= 'f')
			x = (uint8) (*sp - 'a') + 10;
		else
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					 errmsg("\"%c\" is not a valid hexadecimal digit",
							*sp)));
		if (bc)
		{
			*r++ |= x;
			bc = 0;
		}
		else
		{
			*r = x << 4;
			bc = 1;
		}
	}

	PG_RETURN_TEXT_P(cstring_to_text(result));
}

/*
 * Return the character with the given code.
 */
Datum
pg_chr(PG_FUNCTION_ARGS)
{
	Datum		res;

	PG_TRY();
	{
		res = chr	(fcinfo);
	}
	PG_CATCH();
	{
		FlushErrorState();
		PG_RETURN_NULL();
	}
	PG_END_TRY();

	PG_RETURN_DATUM(res);
}

/*
 * Return text representation for three bits8.
 */
static text *
Bits8GetText(bits8 b1, bits8 b2, bits8 b3, int len)
{
	text	   *result;
	bits8	   *tmp;

	result = (text *) palloc(VARHDRSZ + len);
	SET_VARSIZE(result, VARHDRSZ + len);
	tmp = (bits8 *) VARDATA(result);

	tmp[0] = b1;
	if (len > 1)
		tmp[1] = b2;
	if (len > 2)
		tmp[2] = b3;

	return result;
}

/*
 * Return EUC-JP character with the given codes.
 */
Datum
pg_eucjp(PG_FUNCTION_ARGS)
{
	bits8		b1 = *(VARBITS(PG_GETARG_VARBIT_P(0)));
	bits8		b2 = *(VARBITS(PG_GETARG_VARBIT_P(1)));
	bits8		b3 = *(VARBITS(PG_GETARG_VARBIT_P(2)));
	int			len;

	if (GetDatabaseEncoding() != PG_EUC_JP)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("database encoding is %s", GetDatabaseEncodingName()),
				 errhint("pg_eucjp() can be executed only under EUC_JP encoding.")));

#define IS_EUC_RANGE_VALID(b)	((b) >= 0xa1 && (b) <= 0xfe)

	switch (b1)
	{
		case SS2:				/* JIS X 0201 */
			if (b2 < 0xa1 || b2 > 0xdf || b3 != 0x00)
				PG_RETURN_NULL();
			len = 2;
			break;

		case SS3:				/* JIS X 0212 */
			if (!IS_EUC_RANGE_VALID(b2) || !IS_EUC_RANGE_VALID(b3))
				PG_RETURN_NULL();
			len = 3;
			break;

		default:
			if (IS_HIGHBIT_SET(b1)) /* JIS X 0208? */
			{
				if (!IS_EUC_RANGE_VALID(b1) || !IS_EUC_RANGE_VALID(b2) ||
					b3 != 0x00)
					PG_RETURN_NULL();
				len = 2;
			}
			else				/* must be ASCII */
			{
				if (b2 != 0x00 || b3 != 0x00)
					PG_RETURN_NULL();
				len = 1;
			}
			break;
	}

	PG_RETURN_TEXT_P(Bits8GetText(b1, b2, b3, len));
}

#if PG_VERSION_NUM >= 90500
/*
 * Comparison routine to bsearch() for local code -> UTF-8.
 */
static int
compare_local_to_utf(const void *p1, const void *p2)
{
	uint32		v1,
				v2;

	v1 = *(const uint32 *) p1;
	v2 = ((const pg_local_to_utf *) p2)->code;
	return (v1 > v2) ? 1 : ((v1 == v2) ? 0 : -1);
}

/*
 * Perform extra mapping of EUC_JP ranges to UTF-8.
 */
static uint32
extra_euc_jp_to_utf8(uint32 code)
{
	const		pg_local_to_utf *p;

	p = bsearch(&code, ExtraLUmapEUC_JP, lengthof(ExtraLUmapEUC_JP),
				sizeof(pg_local_to_utf), compare_local_to_utf);
	return p ? p->utf : 0;
}
#endif							/* PG_VERSION_NUM >= 90500 */

/*
 * Convert string from EUC_JP to UTF-8.
 */
Datum
pg_euc_jp_to_utf8(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *) PG_GETARG_CSTRING(2);
	unsigned char *dest = (unsigned char *) PG_GETARG_CSTRING(3);
	int			len = PG_GETARG_INT32(4);
#if PG_VERSION_NUM >= 140000
	bool		noError = PG_GETARG_BOOL(5);
#endif

#if PG_VERSION_NUM < 90500
	static pg_local_to_utf_combined cmap[lengthof(ExtraLUmapEUC_JP)];
	static bool first_time = true;
#endif

	CHECK_ENCODING_CONVERSION_ARGS(PG_EUC_JP, PG_UTF8);

#if PG_VERSION_NUM >= 140000
	LocalToUtf(src, len, dest,
			   &euc_jp_to_unicode_tree,
			   NULL, 0,
			   extra_euc_jp_to_utf8,
			   PG_EUC_JP, noError);
#elif PG_VERSION_NUM >= 100000
	LocalToUtf(src, len, dest,
			   &euc_jp_to_unicode_tree,
			   NULL, 0,
			   extra_euc_jp_to_utf8,
			   PG_EUC_JP);
#elif PG_VERSION_NUM >= 90500
	LocalToUtf(src, len, dest,
			   LUmapEUC_JP, lengthof(LUmapEUC_JP),
			   NULL, 0,
			   extra_euc_jp_to_utf8,
			   PG_EUC_JP);
#else

	/*
	 * The first time through, we create the extra conversion map in
	 * pg_local_to_utf_combined struct so that LocalToUtf() can use it as
	 * "secondary" conversion map which is consulted after no match is found
	 * in ordinary map.
	 */
	if (first_time)
	{
		int			i;
		const		pg_local_to_utf *p = ExtraLUmapEUC_JP;
		pg_local_to_utf_combined *cp = cmap;

		for (i = 0; i < lengthof(ExtraLUmapEUC_JP); i++)
		{
			cp->code = p->code;
			cp->utf1 = p->utf;
			cp->utf2 = 0;
			p++;
			cp++;
		}
		first_time = false;
	}

	LocalToUtf(src, dest,
			   LUmapEUC_JP, cmap,
			   lengthof(LUmapEUC_JP),
			   lengthof(ExtraLUmapEUC_JP),
			   PG_EUC_JP, len);
#endif

	PG_RETURN_VOID();
}

#if PG_VERSION_NUM >= 90500
/*
 * Create a compressed version of a text datum
 */
Datum
pglz_compress_text(PG_FUNCTION_ARGS)
{
	text	   *source = PG_GETARG_TEXT_P(0);

	PG_RETURN_BYTEA_P(PGLZCompress((struct varlena *) source));
}

/*
 * Create a compressed version of a bytea datum
 */
Datum
pglz_compress_bytea(PG_FUNCTION_ARGS)
{
	bytea	   *source = PG_GETARG_BYTEA_P(0);

	PG_RETURN_BYTEA_P(PGLZCompress((struct varlena *) source));
}

/*
 * Create a compressed version of a varlena datum
 */
static struct varlena *
PGLZCompress(struct varlena *source)
{
	struct varlena *dest;
	int32		orig_len = VARSIZE(source) - VARHDRSZ;
	int32		len;

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
	bytea	   *source = PG_GETARG_BYTEA_P(0);

	PG_RETURN_TEXT_P(PGLZDecompress((struct varlena *) source));
}

/*
 * Decompress a compressed version of bytea into bytea.
 */
Datum
pglz_decompress_bytea(PG_FUNCTION_ARGS)
{
	bytea	   *source = PG_GETARG_BYTEA_P(0);

	PG_RETURN_BYTEA_P(PGLZDecompress((struct varlena *) source));
}

/*
 * Decompress a compressed version of a varlena datum.
 */
static struct varlena *
PGLZDecompress(struct varlena *source)
{
	struct varlena *dest;
	int32		orig_len = PGLZ_RAWSIZE(source);

	dest = (struct varlena *) palloc(orig_len + VARHDRSZ);
	SET_VARSIZE(dest, orig_len + VARHDRSZ);

	if (!PGLZ_IS_COMPRESSED(source))
		memcpy(VARDATA(dest), PGLZ_RAWDATA(source), orig_len);
	else
	{
#if PG_VERSION_NUM >= 120000
		if (pglz_decompress(PGLZ_RAWDATA(source),
							VARSIZE(source) - PGLZ_HDRSZ,
							VARDATA(dest), orig_len, true) < 0)
#else
		if (pglz_decompress(PGLZ_RAWDATA(source),
							VARSIZE(source) - PGLZ_HDRSZ,
							VARDATA(dest), orig_len) < 0)
#endif
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 (errmsg("specified compressed data is corrupted"),
					  errhint("Make sure compressed data that pglz_compress or pglz_compress_bytea created is specified."))));
	}

	return dest;
}
#endif							/* PG_VERSION_NUM >= 90500 */

#if PG_VERSION_NUM >= 100000
/*
 * Normalize the input string with SASLprep.
 */
Datum
pg_cheat_saslprep(PG_FUNCTION_ARGS)
{
	char	   *input = text_to_cstring(PG_GETARG_TEXT_P(0));
	char	   *output = NULL;
	pg_saslprep_rc rc;

	rc = pg_saslprep(input, &output);
	if (rc == SASLPREP_INVALID_UTF8)
		ereport(ERROR,
				(errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
				 (errmsg("input is not a valid UTF-8 string"))));
	else if (rc == SASLPREP_PROHIBITED)
		ereport(ERROR,
				(errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
				 (errmsg("normalized string contains prohibited characters"))));

	PG_RETURN_TEXT_P(cstring_to_text(output));
}
#endif

/*
 * Functions for manipulating advisory locks.
 * These functions are borrowed from src/backend/utils/adt/lockfuncs.c.
 */
#define SET_LOCKTAG_INT64(tag, key64) \
	SET_LOCKTAG_ADVISORY(tag, \
						 MyDatabaseId, \
						 (uint32) ((key64) >> 32), \
						 (uint32) (key64), \
						 1)
#define SET_LOCKTAG_INT32(tag, key1, key2) \
	SET_LOCKTAG_ADVISORY(tag, MyDatabaseId, key1, key2, 2)

/*
 * Release xact scoped exclusive lock an int8 key.
 *
 * Return true if successful, false if lock was not held.
 */
Datum
pg_advisory_xact_unlock_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT64(tag, key);

	res = LockRelease(&tag, ExclusiveLock, false);

	PG_RETURN_BOOL(res);
}

/*
 * Release xact scoped share lock on an int8 key.
 *
 * Return true if successful, false if lock was not held.
 */
Datum
pg_advisory_xact_unlock_shared_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT64(tag, key);

	res = LockRelease(&tag, ShareLock, false);

	PG_RETURN_BOOL(res);
}

/*
 * Release xact scoped exclusive lock on 2 int4 keys.
 *
 * Return true if successful, false if lock was not held.
*/
Datum
pg_advisory_xact_unlock_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT32(tag, key1, key2);

	res = LockRelease(&tag, ExclusiveLock, false);

	PG_RETURN_BOOL(res);
}

/*
 * Release xact scoped share lock on 2 int4 keys.
 *
 * Return true if successful, false if lock was not held.
 */
Datum
pg_advisory_xact_unlock_shared_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT32(tag, key1, key2);

	res = LockRelease(&tag, ShareLock, false);

	PG_RETURN_BOOL(res);
}
