
#include "test_runner.h"

test_entry_t  g_tests[MAX_TESTS];
int           g_test_count = 0;
int           g_pass = 0, g_fail = 0;
const char   *g_cur_suite = "";
const char   *g_cur_test  = "";
int           g_cur_failed = 0;
