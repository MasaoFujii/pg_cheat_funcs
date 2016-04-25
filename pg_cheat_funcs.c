/*-------------------------------------------------------------------------
 *
 * pg_cheat_funcs.c
 *   provides various cheat (but useful) functions
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xlog_internal.h"
#include "replication/walreceiver.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_xlogfile_name);
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
