# pg_cheat_funcs
This extension provides cheat (but useful) functions on PostgreSQL.

## Functions

### record pg_stat_get_memory_context()
Return statistics about all memory contexts.
This function returns a record, shown in the table below.

| Column Name   | Data Type | Description                                    |
|---------------|-----------|------------------------------------------------|
| name          | text      | context name                                   |
| parent        | text      | name of parent context                         |
| level         | integer   | distance from TopMemoryContext in context tree |
| total_bytes   | bigint    | total bytes requested from malloc              |
| total_nblocks | bigint    | total number of malloc blocks                  |
| free_bytes    | bigint    | free space in bytes                            |
| free_chunks   | bigint    | number of free chunks                          |
| used_bytes    | bigint    | used space in bytes                            |

This function is available only in PostgreSQL 9.6 or later.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_stat_print_memory_context()
Cause statistics about all memory contexts to be logged.
The format of log message for each memory context is:

    [name]: [total_bytes] total in [total_nblocks] blocks; [free_bytes] free ([free_chunks] chunks); [used_bytes] used

For descriptions of the above fields, please see [pg_stat_get_memory_context()](#record-pg_stat_get_memory_context).
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_signal_process(pid int, signame text)
Send a signal to PostgreSQL server process.
This function can signal to only postmaster, backend, walsender and walreceiver process.
Valid signal names are HUP, INT, QUIT, ABRT, KILL, TERM, USR1, USR2, CONT, and STOP.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

For example, terminate walreceiver process:

    -- Note that pg_stat_wal_receiver view is available in 9.6 or later
    =# SELECT pg_signal_process(pid, 'TERM') FROM pg_stat_wal_receiver;

### text pg_xlogfile_name(location pg_lsn, recovery boolean)
Convert transaction log location string to file name.
This function is almost the same as pg_xlogfile_name() which PostgreSQL core provides.
The difference of them is whether there is a second parameter of type boolean.
If false, this function always fails with an error during recovery.
This is the same behavior as the core version of pg_xlogfile_name().
If true, this function can be executed even during recovery.
But note that the first 8 digits of returned WAL filename
(which represents the timeline ID) can be completely incorrect.
That is, this function can return bogus WAL file name.
For details of this conversion, please see [PostgreSQL document](http://www.postgresql.org/docs/devel/static/functions-admin.html#FUNCTIONS-ADMIN-BACKUP).

### xid pg_set_next_xid(transactionid xid)
Set and return the next transaction ID.
Note that this function doesn't check if it's safe to assign
the given transaction ID to the next one.
The caller must carefully choose the safe transaction ID,
e.g., which doesn't cause a transaction ID wraparound problem.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### record pg_xid_assignment()
Return information about transaction ID assignment state.
This function returns a record, shown in the table below.

| Column Name    | Data Type | Description                                       |
|----------------|-----------|---------------------------------------------------|
| next_xid       | xid       | next transaction ID to assign                     |
| oldest_xid     | xid       | cluster-wide minimum datfrozenxid                 |
| xid_vac_limit  | xid       | start forcing autovacuums here                    |
| xid_warn_limit | xid       | start complaining here                            |
| xid_stop_limit | xid       | refuse to advance next transaction ID beyond here |
| xid_wrap_limit | xid       | where the world ends                              |
| oldest_xid_db  | oid       | database with minimum datfrozenxid                |

This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### text pg_show_primary_conninfo()
Return the current value of primary_conninfo recovery parameter.
If it's not set yet, NULL is returned.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.
For details of primary_conninfo parameter, please see [PostgreSQL document](http://www.postgresql.org/docs/devel/static/standby-settings.html).

## Configuration Parameters

### pg_cheat_functions.log_memory_context (boolean)
Cause statistics about all memory contexts to be logged at the end of query execution.
For details of log format, please see [pg_stat_print_memory_context()](#void-pg_stat_print_memory_context)
This parameter is off by default. Only superusers can change this setting.
