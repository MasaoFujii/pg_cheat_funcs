# pg_cheat_funcs
This extension provides cheat (but useful) functions on PostgreSQL.

pg_cheat_funcs is released under the [PostgreSQL License](https://opensource.org/licenses/postgresql), a liberal Open Source license, similar to the BSD or MIT licenses.

## Test Status
[![Build Status](https://github.com/MasaoFujii/pg_cheat_funcs/actions/workflows/test.yml/badge.svg)](https://github.com/MasaoFujii/pg_cheat_funcs/actions/workflows/test.yml)

## Install

Download the source archive of pg_cheat_funcs from
[here](https://github.com/MasaoFujii/pg_cheat_funcs),
and then build and install it.

    $ cd pg_cheat_funcs
    $ make USE_PGXS=1 PG_CONFIG=/opt/pgsql-X.Y.Z/bin/pg_config
    $ su
    # make USE_PGXS=1 PG_CONFIG=/opt/pgsql-X.Y.Z/bin/pg_config install
    # exit

USE_PGXS=1 must be always specified when building this extension.
The path to [pg_config](http://www.postgresql.org/docs/devel/static/app-pgconfig.html)
(which exists in the bin directory of PostgreSQL installation)
needs be specified in PG_CONFIG.
However, if the PATH environment variable contains the path to pg_config,
PG_CONFIG doesn't need to be specified.

## Functions

Note that **CREATE EXTENSION pg_cheat_funcs** needs to be executed
in all the databases that you want to execute the functions that
this extension provides.

    =# CREATE EXTENSION pg_cheat_funcs;

### SETOF record pg_stat_get_memory_context()
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

This function is available only in between PostgreSQL 9.6 and 13.
Since 14, PostgreSQL core provides the view
[pg_backend_memory_contexts](https://www.postgresql.org/docs/devel/view-pg-backend-memory-contexts.html)
that displays all the memory contexts of the server process
attached to the current session.

This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_stat_print_memory_context()
Cause statistics about all memory contexts to be logged.
The format of log message for each memory context is:

    [name]: [total_bytes] total in [total_nblocks] blocks; [free_bytes] free ([free_chunks] chunks); [used_bytes] used

For descriptions of the above fields, please see [pg_stat_get_memory_context()](#setof-record-pg_stat_get_memory_context).

This function is available only in PostgreSQL 13 or before.
Since 14, PostgreSQL core provides the function
[pg_log_backend_memory_contexts](https://www.postgresql.org/docs/devel/functions-admin.html#FUNCTIONS-ADMIN-SIGNAL)
that requests to log the memory contexts of the backend with
the specified process ID.

This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### record pg_cached_plan_source(stmt text)
Return information about cached plan source of the specified prepared statement.
This function returns a record, shown in the table below.

| Column Name       | Data Type        | Description                              |
|-------------------|------------------|------------------------------------------|
| generic_cost      | double precision | cost of generic plan, or -1 if not known |
| total_custom_cost | double precision | total cost of custom plans so far        |
| num_custom_plans  | integer          | number of plans included in total        |
| force_generic     | boolean          | force use of generic plan?               |
| force_custom      | boolean          | force use of custom plan?                |

This function is available only in PostgreSQL 9.2 or later.

### void pg_signal_process(pid int, signame text)
Send a signal to PostgreSQL server process.
This function can signal to only postmaster, backend, walsender and walreceiver process.
Valid signal names are HUP, INT, QUIT, ABRT, KILL, TERM, USR1, USR2, CONT, and STOP.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

For example, terminate walreceiver process:

    -- Note that pg_stat_wal_receiver view is available in 9.6 or later
    =# SELECT pg_signal_process(pid, 'TERM') FROM pg_stat_wal_receiver;

### integer pg_get_priority(pid int)
Return the scheduling priority ("nice") of the specified PostgreSQL server process.
This function can get the priority of only postmaster, backend, walsender and
walreceiver process.
See getpriority(2) man page for details about a scheduling priority.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_set_priority(pid int, priority int)
Set the scheduling priority ("nice") of the specified PostgreSQL server process to the specified value.
This function can change the priority of only postmaster, backend, walsender and
walreceiver process.
See getpriority(2) man page for details about a scheduling priority.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_segmentation_fault(treat_fatal_as_error boolean)
Cause segmentation fault.
If pg_cheat_funcs.exit_on_segv is enabled and treat_fatal_as_error is true,
segmentation fault that this function causes will lead to ERROR instead of
FATAL error. This is intended mainly for testing.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_process_config_file()
Read and process the configuration file.
Note that, if an error occurs, it's logged with DEBUG2 level.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### text pg_xlogfile_name(location pg_lsn, recovery boolean)
Convert transaction log location string to file name.
This function is almost the same as pg_walfile_name()
(or pg_xlogfile_name in 9.6 or before) which PostgreSQL core provides.
The difference of them is whether there is a second parameter of type boolean.
If false, this function always fails with an error during recovery.
This is the same behavior as the core version of pg_walfile_name().
If true, this function can be executed even during recovery.
But note that the first 8 digits of returned WAL filename
(which represents the timeline ID) can be completely incorrect.
That is, this function can return bogus WAL file name.
For details of this conversion, please see [PostgreSQL document](http://www.postgresql.org/docs/devel/static/functions-admin.html#FUNCTIONS-ADMIN-BACKUP).
This function is available only in PostgreSQL 9.4 or later.

### pg_lsn max(SETOF pg_lsn)
This is max() aggregate function for pg_lsn data type.
This aggregate function computes and returns a maximum pg_lsn value
from a set of pg_lsn input values.
This function is available only in  between PostgreSQL 9.4 and 12.
Since 13, this is supported in PostgreSQL core.

### pg_lsn min(SETOF pg_lsn)
This is min() aggregate function for pg_lsn data type.
This aggregate function computes and returns a minimum pg_lsn value
from a set of pg_lsn input values.
This function is available only in between PostgreSQL 9.4 and 12.
Since 13, this is supported in PostgreSQL core.

### SETOF record pg_stat_get_syncrep_waiters()
Return statistics about all server processes waiting for
synchronous replication.
This function returns a record per server process waiting for
synchronous replication, shown in the table below.

| Column Name | Data Type | Description                          |
|-------------|-----------|--------------------------------------|
| pid         | integer   | Process ID of a server process       |
| wait_lsn    | pg_lsn    | Transaction log position to wait for |
| wait_mode   | text      | Wait mode of this server process     |

Possible values of wait_mode are write, flush and apply
(only in PostgreSQL 9.6 or later).

This function is available only in PostgreSQL 9.4 or later.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_wait_syncrep(location pg_lsn)
Wait for synchronous replication.
This function waits until the given transaction log location is acknowledged
by synchronous standbys, based on the setting of
[synchronous_commit](http://www.postgresql.org/docs/devel/static/runtime-config-wal.html#GUC-SYNCHRONOUS-COMMIT).

This function is available only in PostgreSQL 9.4 or later.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_refresh_snapshot()
Forcibly refresh the current snapshot whatever transaction isolation
level is used.

Note that this is extremely dangerous function and can easily break
transaction isolation.
This function must not be used for a purpose other than debug.

This function is available only in PostgreSQL 9.4 or later.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### xid pg_set_next_xid(transactionid xid)
Set and return the next transaction ID.
Note that this function doesn't check if it's safe to assign
the given transaction ID to the next one.
The caller must carefully choose the safe transaction ID,
e.g., which doesn't cause a transaction ID wraparound problem.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### SETOF record pg_xid_assignment()
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

### xid8 pg_xid_to_xid8 (transactionid xid)
Convert the specified transaction ID with 32-bit type xid to
the transaction ID with 64-bit type xid8.
The specified transaction ID must be from the same epoch as
the latest transaction ID, or the epoch before.
For example, the currently used xid like pg_stat_activity.backend_xid
or pg_locks.transactionid can be specified in this function.

This function is available only in PostgreSQL 13 or later.

### xid pg_set_next_oid(objectid oid)
Set and return the next object ID (OID).
Note that the next OID is set to 16384 (FirstNormalObjectId)
when the given OID is less than that number.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### SETOF record pg_oid_assignment()
Return information about object ID (OID) assignment state.
This function returns a record, shown in the table below.

| Column Name | Data Type | Description                            |
|-------------|-----------|----------------------------------------|
| next_oid    | oid       | next object ID to assign               |
| oid_count   | integer   | OIDs available before must do WAL work |

This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### integer pg_advance_vacuum_cleanup_age(integer)
Specify the number of transactions by which VACUUM and HOT updates
will advance cleanup of dead row versions.
[vacuum_defer_cleanup_age](https://www.postgresql.org/docs/devel/static/runtime-config-replication.html#GUC-VACUUM-DEFER-CLEANUP-AGE)
in the session calling this function is set to
the negative value of that specified number. If the argument is omitted
or NULL is specified, vacuum_defer_cleanup_age is reset to
its original setting value specified in the configuration file.
This function returns the vacuum cleanup age.

By advancing the cleanup age, VACUUM and HOT updates can clean up
even dead tuples that were produced since oldest transaction had started.
So this function is helpful to prevent the database from bloating due to
unremovable dead tuples while long transaction is running.

Note that this is extremely dangerous function and can easily corrupt
the database. Any important data may disappear and data consistency
may be lost completely.
This function must not be used for a purpose other than debug.

This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

This function is available only in PostgreSQL 15 or earlier,
as the vacuum_defer_cleanup_age that it depends on was removed in 16.

### void pg_checkpoint(fast bool, wait bool, force bool)
Perform a checkpoint.
If fast is true (default), a checkpoint will finish as soon as possible.
Otherwise, I/O required for a checkpoint will be spread out over
a period of time, to minimize the impact on query processing.
If wait is true (default), this function waits for a checkpoint to complete
before returning. Otherwise, it just signals checkpointer to do it and returns.
If force is true (default), this function forces a checkpoint even if no WAL
activity has occurred since the last one.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_promote(fast bool)
Promote the standby server.
If fast is true (default), this function requests for fast promotion.
In fast mode, standby promotion only creates a lightweight
end-of-recovery record instead of a full checkpoint.
A checkpoint is requested later, after we're fully out of recovery mode
and already accepting queries.
If fast is false, this function requests for fallback version of promotion.
In this mode, a full checkpoint is created at the end of recovery.
Note that fallback version of promotion is performed
in PostgreSQL 9.2 or before whether fast is true or false
because fast promotion is available only in 9.3 or later.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

This function is available only in 11 or before.
Since 12, this is supported in PostgreSQL core.

### SETOF record pg_recovery_settings()
Return information about all parameter settings in recovery.conf.
This function returns a record, shown in the table below.

| Column Name | Data Type | Description                  |
|-------------|-----------|------------------------------|
| name        | text      | configuration parameter name |
| setting     | text      | value of the parameter       |

This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### text pg_show_primary_conninfo()
Return the current value of primary_conninfo recovery parameter.
If it's not set yet, NULL is returned.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.
For details of primary_conninfo parameter, please see [PostgreSQL document](http://www.postgresql.org/docs/devel/static/standby-settings.html).

### integer pg_postmaster_pid()
Return the Process ID of the postmaster process.

### timestamp with time zone pg_backend_start_time()
Return the time when the server process attached to the current session
was started.

### SETOF text pg_list_relation_filepath(relation regclass)
List the file path names of the specified relation.
Note that this function returns only the path name of
the first segment file in PostgreSQL 9.4 or before.

### text pg_tablespace_version_direcotry()
Return the name of major-version-specific tablespace subdirectory.

### bigint pg_file_write_binary(filepath text, data bytea)
Write bytea data to the file.
This function creates the file if it does not exist,
and truncates it to zero length otherwise.
Then this function writes the bytea data from the beginning of the file,
and returns the number of bytes written.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### void pg_file_fsync(filepath text)
Try to fsync the file or directory.
This function is available only in PostgreSQL 9.4 or later.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.

### text to_octal(integer or bigint)
Convert number to its equivalent octal representation.

### text pg_text_to_hex(str text)
Convert text to its equivalent hexadecimal representation.

Here is an example of the conversion from text to hex:

    =# SELECT pg_text_to_hex('PostgreSQL');
        pg_text_to_hex    
    ----------------------
     506f737467726553514c
    (1 row)

### text pg_hex_to_text(hex text)
Convert hexadecimal representation to its equivalent text.

Here is an example of the conversion from hex to text:

    =# SELECT pg_hex_to_text('506f737467726553514c');
     pg_hex_to_text 
    ----------------
     PostgreSQL
    (1 row)

### text pg_chr(code integer)
Return the character with the given code.
This function is almost the same as chr() which PostgreSQL core provides.
The difference of them is that this function returns NULL instead of
throwing an error when the requested character is too large or not valid.
Note that valid Unicode code point stops at U+10FFFF (1114111),
even though 4-byte UTF8 sequences can hold values up to U+1FFFFF.
Therefore this function returns NULL whenever the given code is larger
than 1114111.

### text pg_utf8(code integer)
Alias of [pg_chr](https://github.com/MasaoFujii/pg_cheat_funcs#text-pg_chrcode-integer) function.

### SETOF record pg_all_utf8()
Return all valid UTF-8 characters.
This function returns a record, shown in the table below.

| Column Name | Data Type | Description       |
|-------------|-----------|-------------------|
| code        | text      | code of character |
| utf8        | text      | UTF-8 character   |

This function can be executed only under UTF-8 database encoding.

### text pg_eucjp(code1 bit(8), code2 bit(8), code3 bit(8))
Return EUC_JP character with the given codes.
The following table shows the valid combination of the codes.

| code1     | code2     | code3     |
|-----------|-----------|-----------|
| x00 - x7f | -         | -         |
| x8e       | xa1 - xdf | -         |
| xa1 - xfe | xa1 - xfe | -         |
| x8f       | xa1 - xfe | xa1 - xfe |

For example, return EUC_JP character with 'a1fa' (BLACK STAR):

    =# SELECT pg_eucjp('xa1', 'xfa');

This function returns NULL when the requested character is invalid for EUC_JP.
This function can be executed only under EUC_JP database encoding.

### SETOF record pg_all_eucjp()
Return all valid EUC_JP characters.
This function returns a record, shown in the table below.

| Column Name | Data Type | Description                                                     |
|-------------|-----------|-----------------------------------------------------------------|
| code1       | text      | first byte of character                                         |
| code2       | text      | second byte of character (x00 means non-existence of this byte) |
| code3       | text      | third byte of character (x00 means non-existence of this byte)  |
| eucjp       | text      | EUC_JP character                                                |

This function can be executed only under EUC_JP database encoding.

### bytea pglz_compress(data text)
Create and return a compressed version of text data.
This function uses PGLZ (an implementation of LZ compression for PostgreSQL)
for the compression.
If the compression fails (e.g., the compressed result is actually
bigger than the original), this function returns the original data.
Note that the return data may be 4-bytes bigger than the original
even when the compression fails because 4-bytes extra information
like the length of original data is always stored in it.
The bytea data that this function returns needs to be decompressed
by using pg_lz_decompress() to obtain the original text data.
This function is available only in PostgreSQL 9.5 or later.

### bytea pglz_compress_bytea(data bytea)
Create and return a compressed version of bytea data.
This function uses PGLZ (an implementation of LZ compression for PostgreSQL)
for the compression.
If the compression fails (e.g., the compressed result is actually
bigger than the original), this function returns the original data.
Note that the return data may be 4-bytes bigger than the original
even when the compression fails because 4-bytes extra information
like the length of original data is always stored in it.
The bytea data that this function returns needs to be decompressed
by using pg_lz_decompress_bytea() to obtain the original bytea data.
This function is available only in PostgreSQL 9.5 or later.

### text pglz_decompress(data bytea)
Decompress a compressed version of bytea data into text.
Note that the input of this function must be the bytea data that
pg_lz_compress() or pg_lz_compress_bytea() returned.
Otherwise this function may return a corrupted data.
This function is available only in PostgreSQL 9.5 or later.

### bytea pglz_decompress_bytea(data bytea)
Decompress a compressed version of bytea data into bytea.
Note that the input of this function must be the bytea data that
pg_lz_compress() or pg_lz_compress_bytea() returned.
Otherwise this function may return a corrupted data.
This function is available only in PostgreSQL 9.5 or later.

### text pg_saslprep(input text)
Normalize an input string with SASLprep.
SASLprep normalization is basically used to process a user-supplied
password into canonical form for SCRAM authentication.
Note that an error is raised if the input is not a valid UTF-8 string or
the normalized string contains characters prohibited by the SASLprep profile.
This function is available only in PostgreSQL 10 or later.

### pg_advisory_xact_unlock (bigint)
Release a previously-acquired exclusive transaction-level advisory lock.
Return true if the lock is successfully released. If the lock was not held,
false is returned, and in addition, an SQL warning will be reported
by the server.

### pg_advisory_xact_unlock_shared (bigint)
Release a previously-acquired shared transaction-level advisory lock.
Return true if the lock is successfully released. If the lock was not held,
false is returned, and in addition, an SQL warning will be reported
by the server.

### pg_advisory_xact_unlock (integer, integer)
Same as pg_advisory_xact_unlock(bigint).

### pg_advisory_xact_unlock_shared (integer, integer)
Same as pg_advisory_xact_unlock_shared(bigint).

## Encoding Conversions

### pg_euc_jp_to_utf8
This is an encoding conversion from EUC_JP to UTF-8.
It uses two conversion maps; ordinary map and extra map.
They are defined in `conv/euc_jp_to_utf8.map` and `conv/euc_jp_to_utf8.extra`,
respectively.
For each character, ordinary map is consulted first.
If no match is found, extra map is consulted next.
If still no match, an error is raised.

The content of ordinary map is the same as the map that euc_jp_to_utf8
(default conversion map from EUC_JP to UTF-8 that PostgreSQL provides) uses.
The extra map contains some mappings (e.g., the following mappings for
Roman numerals and full-width symbols) that ordinary map doesn't have.

| EUC_JP | UTF-8           | Description               |
|--------|-----------------|---------------------------|
| fcf1   | e285b0 (U+2170) | SMALL ROMAN NUMERAL ONE   |
| fcf2   | e285b1 (U+2171) | SMALL ROMAN NUMERAL TWO   |
| fcf3   | e285b2 (U+2172) | SMALL ROMAN NUMERAL THREE |
| fcf4   | e285b3 (U+2173) | SMALL ROMAN NUMERAL FOUR  |
| fcf5   | e285b4 (U+2174) | SMALL ROMAN NUMERAL FIVE  |
| fcf6   | e285b5 (U+2175) | SMALL ROMAN NUMERAL SIX   |
| fcf7   | e285b6 (U+2176) | SMALL ROMAN NUMERAL SEVEN |
| fcf8   | e285b7 (U+2177) | SMALL ROMAN NUMERAL EIGHT |
| fcf9   | e285b8 (U+2178) | SMALL ROMAN NUMERAL NINE  |
| fcfa   | e285b9 (U+2179) | SMALL ROMAN NUMERAL TEN   |
| fcfb   | efbfa2 (U+FFE2) | FULLWIDTH NOT SIGN        |
| fcfc   | efbfa4 (U+FFE4) | FULLWIDTH BROKEN BAR      |
| fcfd   | efbc87 (U+FF07) | FULLWIDTH APOSTROPHE      |
| fcfe   | efbc82 (U+FF02) | FULLWIDTH QUOTATION MARK  |

In order to use pg_euc_jp_to_utf8 as the default conversion from EUC_JP to
UTF-8, its [pg_conversion](http://www.postgresql.org/docs/devel/static/catalog-pg-conversion.html).condefault
needs to be enabled. Also condefault for euc_jp_to_utf8 (built-in conversion
from EUC_JP to UTF-8) needs to be disabled.
Here is an example of these catalog updates:

    =# BEGIN;
    =# UPDATE pg_conversion SET condefault = 'f' WHERE conname = 'euc_jp_to_utf8';
    =# UPDATE pg_conversion SET condefault = 't' WHERE conname = 'pg_euc_jp_to_utf8';
    =# COMMIT;

It's possible to use the customized conversion map by modifying the map files
directly and rebuilding pg_cheat_funcs module.
Note that entries in a map file must be sorted in ascending order.

## Configuration Parameters

Note that [shared_preload_libraries](http://www.postgresql.org/docs/devel/static/runtime-config-client.html#GUC-SHARED-PRELOAD-LIBRARIES)
or [session_preload_libraries](http://www.postgresql.org/docs/devel/static/runtime-config-client.html#GUC-SESSION-PRELOAD-LIBRARIES)
(available in PostgreSQL 9.4 or later) must be set to 'pg_cheat_funcs'
in postgresql.conf
if you want to use the configuration parameters which this extension provides.

### pg_cheat_funcs.log_memory_context (boolean)
Cause statistics about the memory contexts to be logged at the end of query execution.
For details of log format, please see [pg_stat_print_memory_context()](#void-pg_stat_print_memory_context)
This parameter is off by default. Only superusers can change this setting.

### pg_cheat_funcs.hide_appname (boolean)
If true, client's application_name is ignored and its setting value is
stored in pg_cheat_funcs.hidden_appname parameter.
By default, this is set to false, so that the string that client specifies
will be used as application_name.
This parameter can only be set in the postgresql.conf
file or on the server command line.

### pg_cheat_funcs.hidden_appname (string)
Report client's application_name hidden from view.
The default is an empty string.
Any users can change this setting.

### pg_cheat_funcs.log_session_start_options (boolean)
Log options sent to the server at connection start.
This parameter is off by default.
Only superusers (in PostgreSQL 9.5 or later) or any users (in 9.4 or before)
can change this parameter at session start,
and it cannot be changed at all within a session.

### pg_cheat_funcs.scheduling_priority (integer)
Specify the scheduling priority ("nice") of PostgreSQL server process.
Valid values are between -20 and 19.
Lower values cause more favorable scheduling.
The default value is zero.
Any users can change this setting.
See getpriority(2) man page for details about a scheduling priority.

### pg_cheat_funcs.exit_on_segv (boolean)
If off, which is the default, segmentation fault will lead to the server crash.
If on, only the current session causing segmentation fault will be terminated.
Any users can change this setting.
