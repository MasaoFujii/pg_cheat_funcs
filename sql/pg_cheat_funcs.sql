CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_stat_print_memory_context();

SELECT pg_signal_process(pg_postmaster_pid(), 'HUP');

SELECT pg_eucjp('xa4', 'xa2');

DROP EXTENSION pg_cheat_funcs;
