CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT chr(88) = pg_chr(88);
SELECT chr(12354) = pg_chr(12354);
SELECT chr(57000);
SELECT pg_chr(57000);
SELECT chr(2000000);
SELECT pg_chr(2000000);

DROP EXTENSION pg_cheat_funcs;
