#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(11)

--!./tcltestrunner.lua
-- 2013-06-14
--
-- The author disclaims copyright to this source code.  In place of
-- a legal notice, here is a blessing:
--
--    May you do good and not evil.
--    May you find forgiveness for yourself and forgive others.
--    May you share freely, never taking more than you give.
--
-------------------------------------------------------------------------
-- This file implements regression tests for SQLite library.  The
-- focus of this file is testing that the optimizations that disable
-- ORDER BY clauses work correctly
--
-- ["set","testdir",[["file","dirname",["argv0"]]]]
-- ["source",[["testdir"],"\/tester.tcl"]]
testprefix = "orderby5"
-- Generate test data for a join.  Verify that the join gets the
-- correct answer.
--
test:do_execsql_test(
    1.1,
    [[
        CREATE TABLE t1(id primary key,a,b,c);
        CREATE INDEX t1bc ON t1(b,c);

        EXPLAIN QUERY PLAN
        SELECT DISTINCT a, b, c FROM t1 WHERE a=0;
    ]], {
        -- <1.1>
        "~/B-TREE/"
        -- </1.1>
    })

test:do_execsql_test(
    "1.2.1",
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT a, c, b FROM t1 WHERE a=0;
    ]], {
        -- <1.2.1>
        "~/B-TREE/"
        -- </1.2.1>
    })

test:do_execsql_test(
    "1.2.2",
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT a, c, b FROM t1 WHERE a='xyz' COLLATE nocase;
    ]], {
        -- <1.2.2>
        "/B-TREE/"
        -- </1.2.2>
    })

test:do_execsql_test(
    "1.2.3",
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT a COLLATE nocase, c, b FROM t1 WHERE a='xyz';
    ]], {
        -- <1.2.3>
        "/B-TREE/"
        -- </1.2.3>
    })

test:do_execsql_test(
    "1.2.4",
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT a COLLATE nocase, c, b FROM t1 WHERE a='xyz' COLLATE nocase;
    ]], {
        -- <1.2.4>
        "~/B-TREE/"
        -- </1.2.4>
    })

test:do_execsql_test(
    1.3,
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT b, a, c FROM t1 WHERE a=0;
    ]], {
        -- <1.3>
        "~/B-TREE/"
        -- </1.3>
    })

test:do_execsql_test(
    1.4,
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT b, c, a FROM t1 WHERE a=0;
    ]], {
        -- <1.4>
        "~/B-TREE/"
        -- </1.4>
    })

test:do_execsql_test(
    1.5,
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT c, a, b FROM t1 WHERE a=0;
    ]], {
        -- <1.5>
        "~/B-TREE/"
        -- </1.5>
    })

test:do_execsql_test(
    1.6,
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT c, b, a FROM t1 WHERE a=0;
    ]], {
        -- <1.6>
        "~/B-TREE/"
        -- </1.6>
    })

test:do_execsql_test(
    1.7,
    [[
        EXPLAIN QUERY PLAN
        SELECT DISTINCT c, b, a FROM t1 WHERE +a=0;
    ]], {
        -- <1.7>
        "/B-TREE/"
        -- </1.7>
    })

-- MUST_WORK_TEST
-- # In some cases, it is faster to do repeated index lookups than it is to
-- # sort.  But in other cases, it is faster to sort than to do repeated index
-- # lookups.
-- #
-- do_execsql_test 2.1a {
--   CREATE TABLE t2(a,b,c);
--   CREATE INDEX t2bc ON t2(b,c);
--   ANALYZE;
--   INSERT INTO sqlite_stat1 VALUES('t1','t1bc','1000000 10 9');
--   INSERT INTO sqlite_stat1 VALUES('t2','t2bc','100 10 5');
--   ANALYZE sqlite_master;
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t2 WHERE a=0 ORDER BY a, b, c;
-- } {~/B-TREE/}
-- do_execsql_test 2.1b {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE likelihood(a=0, 0.05) ORDER BY a, b, c;
-- } {/B-TREE/}
-- do_execsql_test 2.2 {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE +a=0 ORDER BY a, b, c;
-- } {/B-TREE/}
-- do_execsql_test 2.3 {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE a=0 ORDER BY b, a, c;
-- } {~/B-TREE/}
-- do_execsql_test 2.4 {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE a=0 ORDER BY b, c, a;
-- } {~/B-TREE/}
-- do_execsql_test 2.5 {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE a=0 ORDER BY a, c, b;
-- } {/B-TREE/}
-- do_execsql_test 2.6 {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE a=0 ORDER BY c, a, b;
-- } {/B-TREE/}
-- do_execsql_test 2.7 {
--   EXPLAIN QUERY PLAN
--   SELECT * FROM t1 WHERE a=0 ORDER BY c, b, a;
-- } {/B-TREE/}
test:do_execsql_test(
    3.0,
    [[
        CREATE TABLE t3(a INTEGER PRIMARY KEY, b, c, d, e, f);
        CREATE INDEX t3bcde ON t3(b, c, d, e);
        EXPLAIN QUERY PLAN
        SELECT a FROM t3 WHERE b=2 AND c=3 ORDER BY d DESC, e DESC, b, c, a DESC;
    ]], {
        -- <3.0>
        "~/B-TREE/"
        -- </3.0>
    })

-- MUST_WORK_TEST
-- do_execsql_test 3.1 {
--   DROP TABLE t3;
--   CREATE TABLE t3(a INTEGER PRIMARY KEY, b, c, d, e, f);
--   CREATE INDEX t3bcde ON t3(b, c, d, e);
--   EXPLAIN QUERY PLAN
--   SELECT a FROM t3 WHERE b=2 AND c=3 ORDER BY d DESC, e DESC, b, c, a DESC;
-- } {~/B-TREE/}
test:finish_test()

