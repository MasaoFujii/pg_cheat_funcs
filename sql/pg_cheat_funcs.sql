CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_stat_print_memory_context();

SELECT pg_signal_process(pg_postmaster_pid(), 'HUP');

SELECT pg_eucjp('xa4', 'xa2');

SELECT pg_set_next_xid(next_xid) = next_xid FROM pg_xid_assignment();

SELECT pg_text_to_hex('PostgreSQL');
SELECT pg_hex_to_text('506f737467726553514c');
SELECT pg_hex_to_text(upper('506f737467726553514c'));

DROP EXTENSION pg_cheat_funcs;
