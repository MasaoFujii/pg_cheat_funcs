# pg_cheat_funcs
This extension provides cheat (but useful) functions on PostgreSQL.

## Test Status
[![Build Status](https://travis-ci.org/MasaoFujii/pg_cheat_funcs.svg?branch=master)](https://travis-ci.org/MasaoFujii/pg_cheat_funcs)

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
This function is available only in PostgreSQL 9.4 or later.

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

### text pg_show_primary_conninfo()
Return the current value of primary_conninfo recovery parameter.
If it's not set yet, NULL is returned.
This function is restricted to superusers by default,
but other users can be granted EXECUTE to run the function.
For details of primary_conninfo parameter, please see [PostgreSQL document](http://www.postgresql.org/docs/devel/static/standby-settings.html).

### integer pg_postmaster_pid()
Return the Process ID of the postmaster process.

### bigint pg_file_write_binary(filepath text, data bytea)
Write bytea data to the file.
This function creates the file if it does not exist,
and truncates it to zero length otherwise.
Then this function writes the bytea data from the beginning of the file,
and returns the number of bytes written.
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
The extra map contains the following mapping that ordinary map doesn't have.

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

### pg_cheat_functions.log_memory_context (boolean)
Cause statistics about all memory contexts to be logged at the end of query execution.
For details of log format, please see [pg_stat_print_memory_context()](#void-pg_stat_print_memory_context)
This parameter is off by default. Only superusers can change this setting.
