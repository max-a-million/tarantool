#include <stdio.h>
#include <assert.h>

#include "box/sql.h"
#include "box/sql/sqliteInt.h"

#ifdef SWAP
    #undef SWAP
#endif
#ifdef likely
    #undef likely
#endif
#ifdef unlikely
    #undef unlikely
#endif

#include "trivia/util.h"
#include "unit.h"

#define bitvec_test(a, b) sqlite3BitvecBuiltinTest((a), (b))

void
test_errors()
{
        plan(2);

        int test_1[] = { 5, 1, 1, 1, 0 };
        is(1, bitvec_test(400, test_1), "error test");
        int test_2[] = { 5, 1, 234, 1, 0 };
        is(234, bitvec_test(400, test_2), "error test");

        check_plan();
}

void
test_various_sizes()
{
        plan(4);

        int i = 0;
        int sz_args[] = { 400, 4000, 40000, 400000 };

        int test_args[][5] = {
                { 1, 400, 1, 1, 0 },
                { 1, 4000, 1, 1, 0 },
                { 1, 40000, 1, 1, 0 },
                { 1, 400000, 1, 1, 0 }
        };

        for (i = 0; i < 4; i++) {
               is(0, bitvec_test(sz_args[i], test_args[i]), "various sizes");
        }

        check_plan();
}


void
test_larger_increments()
{
        plan(4);

        int i = 0;
        int sz_args[] = { 400, 4000, 40000, 400000 };

        int test_args[][5] = {
                { 1, 400, 1, 7, 0},
                { 1, 4000, 1, 7, 0},
                { 1,  40000, 1, 7, 0},
                { 1, 400000, 1, 7, 0}
        };


        for (i = 0; i < 4; i++) {
                is(0, bitvec_test(sz_args[i], test_args[i]), "larger increments");
        }

        check_plan();
}

void
test_clearing_mechanism()
{
        plan(9);

        int i = 0;
        int sz_args[] = { 400, 4000, 40000, 400000, 400, 4000, 40000, 400000, 5000 };

        int test_args[][9] = {
                {1, 400, 1, 1, 2, 400, 1, 1, 0},
                {1, 4000, 1, 1, 2, 4000, 1, 1, 0},
                {1, 40000, 1, 1, 2, 40000, 1, 1, 0},
                {1, 400000, 1, 1, 2, 400000, 1, 1, 0},
                {1, 400, 1, 1, 2, 400, 1, 7, 0},
                {1, 4000, 1, 1, 2, 4000, 1, 7, 0},
                {1, 40000, 1, 1, 2, 40000, 1, 7, 0},
                {1, 400000, 1, 1, 2, 400000, 1, 7, 0},
                {1, 5000, 100000, 1, 2, 400000, 1, 37, 0}
        };

        for (i = 0; i < 9; i++) {
                is(0, bitvec_test(sz_args[i], test_args[i]), "clearing mechanism");
        }

        check_plan();
}

void
test_hashing_collisions()
{
        plan(10);

        int start_values[] = { 1, 8, 1 };
        int incr_values[] = { 124, 125, 1 };
        int i, j;

        for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                        int test_20[] = { 1, 60, i, j, 2, 5000, 1, 1, 0 };
                        is(0, bitvec_test(5000, test_20), "hashing collisions");
                }
        }

        int test_30[] = {1, 17000000, 1, 1, 2, 17000000, 1, 1, 0};
        is(0, bitvec_test(17000000, test_30), "hashing collisions");

        check_plan();
}

void
test_random_subsets()
{
        plan(7);

        int test_31[] = {3, 2000, 4, 2000, 0};
        is(0, bitvec_test(4000, test_31), "random subsets");

        int test_32[] = {3, 1000, 4, 1000, 3, 1000, 4, 1000, 3, 1000, 4,
                         1000, 3, 1000, 4, 1000, 3, 1000, 4, 1000, 3, 1000, 4, 1000, 0};
        is(0, bitvec_test(4000, test_32), "random subsets");

        int test_33[] = {3, 10, 0};
        is(0, bitvec_test(400000, test_33), "random subsets");

        int test_34[] = {3, 10, 2, 4000, 1, 1, 0};
        is(0, bitvec_test(4000, test_34), "random subsets");

        int test_35[] = {3, 20, 2, 5000, 1, 1, 0};
        is(0, bitvec_test(5000, test_35), "random subsets");

        int test_36[] = {3, 60, 2, 50000, 1, 1, 0};
        is(0, bitvec_test(50000, test_36), "random subsets");

        int test_37[] = {1, 25, 121, 125, 1, 50, 121, 125, 2, 25, 121, 125, 0};
        is(0, bitvec_test(5000, test_37), "random subsets");

        check_plan();
}

/*
 * This is malloc tcl test - needs to be converted
 *
 * -- This procedure runs sqlite3BitvecBuiltinTest with argments "n" and
-- "program".  But it also causes a malloc error to occur after the
-- "failcnt"-th malloc.  The result should be "0" if no malloc failure
-- occurs or "-1" if there is a malloc failure.
--
-- MUST_WORK_TEST sqlite3_memdebug_fail func was removed (with test_malloc.c)
if 0>0 then
local function bitvec_malloc_test(label, failcnt, n, program)
--    do_test $label [subst {
--    sqlite3_memdebug_fail $failcnt
--    set x \[sqlite3BitvecBuiltinTest $n [list $program]\]
--    set nFail \[sqlite3_memdebug_fail -1\]
--    if {\$nFail==0} {
--        set ::go 0
--        set x -1
--        }
--        set x
--        }] -1
end

-- Make sure malloc failures are handled sanily.
--
-- ["unset","-nocomplain","n"]
-- ["unset","-nocomplain","go"]
go = 1
X(177, "X!cmd", [=[["save_prng_state"]]=])
for _ in X(0, "X!for", [=[["set n 0","$go","incr n"]]=]) do
    X(180, "X!cmd", [=[["restore_prng_state"]]=])
    bitvec_malloc_test("bitvec-3.1."..n, n, 5000, [[
      3 60 2 5000 1 1 3 60 2 5000 1 1 3 60 2 5000 1 1 0
  ]])
end
go = 1
for _ in X(0, "X!for", [=[["set n 0","$go","incr n"]]=]) do
    X(187, "X!cmd", [=[["restore_prng_state"]]=])
    bitvec_malloc_test("bitvec-3.2."..n, n, 5000, [[
      3 600 2 5000 1 1 3 600 2 5000 1 1 3 600 2 5000 1 1 0
  ]])
end
go = 1
for _ in X(0, "X!for", [=[["set n 1","$go","incr n"]]=]) do
    bitvec_malloc_test("bitvec-3.3."..n, n, 50000, "1 50000 1 1 0")
en
end
*
*/

void
run_tests(void)
{
        header();

        test_errors();
        test_various_sizes();
        test_larger_increments();
        test_clearing_mechanism();
        test_random_subsets();

        footer();
}

int
main(void)
{
        sqlite3MutexInit();
        sqlite3MallocInit();

        run_tests();

        sqlite3MallocEnd();
        sqlite3MutexEnd();

        return 0;
}
