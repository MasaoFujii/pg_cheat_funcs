# pg_cheat_funcs
This extension provides cheat (but useful) functions on PostgreSQL.

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
