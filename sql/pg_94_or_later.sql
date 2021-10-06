CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

CREATE TABLE test_lsn (lsn pg_lsn);
INSERT INTO test_lsn VALUES ('0/A'), ('0/1000'), ('DA3/15A4D10'), ('E4/A0422B68'), ('447/F6D166E8');
SELECT min(lsn), max(lsn) FROM test_lsn;

SELECT pg_file_fsync('global');
SELECT pg_file_fsync('global/pg_control');

BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT pg_refresh_snapshot();
SELECT 1;
SELECT pg_refresh_snapshot();
COMMIT;

DROP EXTENSION pg_cheat_funcs;
