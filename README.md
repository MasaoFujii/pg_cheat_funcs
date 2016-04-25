# pg_cheat_funcs
This extension provides cheat (but useful) functions on PostgreSQL.

## text pg_xlogfile_name(location pg_lsn, recovery boolean)
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
