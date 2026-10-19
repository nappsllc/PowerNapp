/* Unit tests for the app's own logic (command line, migration, layout invariants): the whole unity
   build with its entry point renamed. Built by tests\unit\build.bat. Calls nothing that touches
   Task Scheduler, the registry or the INI: the migration test runs on fake Task Scheduler calls. */
#define wmain powernapp_wmain
#include "../../powernapp.c"
#undef wmain

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* --task spellings: the names, PowerOff 1.x's aliases ("poweroff" = shut down), not the app's name */
static void task_aliases(void)
{
    Task t = T_LOCK;
    CHECK(task_from_arg(L"shutdown", &t) && t == T_SHUTDOWN);
    CHECK(task_from_arg(L"poweroff", &t) && t == T_SHUTDOWN);
    CHECK(task_from_arg(L"Power-Off", &t) && t == T_SHUTDOWN);
    CHECK(task_from_arg(L"reboot", &t) && t == T_RESTART);
    CHECK(task_from_arg(L"sign_out", &t) && t == T_LOGOFF);
    CHECK(task_from_arg(L"monitor-off", &t) && t == T_DISPLAY);
    CHECK(task_from_arg(L"suspend", &t) && t == T_SLEEP);
    CHECK(!task_from_arg(L"powernapp", &t));
    CHECK(!task_from_arg(L"bogus", &t));
}

int main(void)
{
    task_aliases();
    printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
    return g_fail;
}
