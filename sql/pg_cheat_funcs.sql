CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_stat_print_memory_context();

SELECT pg_signal_process(pg_postmaster_pid(), 'HUP');

SELECT pg_eucjp('xa4', 'xa2');

SELECT pg_set_next_xid(next_xid) = next_xid FROM pg_xid_assignment();

DROP EXTENSION pg_cheat_funcs;
