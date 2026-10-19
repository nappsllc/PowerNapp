/* String table: X(ID, "file.key", L"English text").  Placeholders (%ls %d %lld) in a translation must
   match the English text's, or the translation is ignored.  Escapes in lang files: \n and \\.
   lang\en.txt is generated from this list by tools\gen-lang-template.ps1 (one entry per line).
   Ranges index-aligned with an enum: TASK_* with Task, MODE_* with GUI_MODES, RM_* with Task. */
#define STRINGS(X) \
    X(TASK_SHUTDOWN,     "task.shutdown",      L"Shut down") \
    X(TASK_RESTART,      "task.restart",       L"Restart") \
    X(TASK_SLEEP,        "task.sleep",         L"Sleep") \
    X(TASK_HIBERNATE,    "task.hibernate",     L"Hibernate") \
    X(TASK_LOGOFF,       "task.logoff",        L"Sign out") \
    X(TASK_LOCK,         "task.lock",          L"Lock") \
    X(TASK_DISPLAY,      "task.display",       L"Turn off display") \
    X(MODE_DAILY,        "mode.daily",         L"At a specific time") \
    X(MODE_COUNTDOWN,    "mode.countdown",     L"After a countdown") \
    X(MODE_WEEKLY,       "mode.weekly",        L"On a weekly schedule") \
    X(MODE_IDLE,         "mode.idle",          L"When the PC is idle") \
    X(TIME_MINUTE,       "time.minute",        L"%d minute") \
    X(TIME_MINUTES,      "time.minutes",       L"%d minutes") \
    X(TIME_HOUR,         "time.hour",          L"%d hour") \
    X(TIME_HOURS,        "time.hours",         L"%d hours") \
    X(TIME_HM,           "time.hm",            L"%d h %d min") \
    X(DAY_TODAY,         "day.today",          L"Today") \
    X(DAY_TOMORROW,      "day.tomorrow",       L"Tomorrow") \
    X(DAY_AT,            "day.at",             L"%ls, %ls") \
    X(DAYS_EVERY,        "days.every",         L"every day") \
    X(DAYS_WEEKDAYS,     "days.weekdays",      L"weekdays") \
    X(DAYS_WEEKENDS,     "days.weekends",      L"weekends") \
    X(DAYS_NONE,         "days.none",          L"no days") \
    X(PLAN_AT,           "plan.at",            L"%ls at %ls") \
    X(PLAN_AT_ON,        "plan.at_on",         L"%ls at %ls on %ls") \
    X(PLAN_ONCE,         "plan.once",          L"%ls once at %ls %ls") \
    X(PLAN_IN,           "plan.in",            L"%ls in %ls") \
    X(PLAN_IDLE,         "plan.idle",          L"%ls after %ls idle") \
    X(PLAN_EVERY,        "plan.every",         L"%ls every %ls min") \
    X(ERR_HOUR,          "err.hour",           L"Hour must be 0-23 and minute 0-59") \
    X(ERR_DAYS,          "err.days",           L"Select at least one day of the week") \
    X(ERR_DATE,          "err.date",           L"Date must be YYYY-MM-DD") \
    X(ERR_TIME,          "err.time",           L"Time must be HH:MM (24h)") \
    X(ERR_PAST,          "err.past",           L"That date/time is in the past.") \
    X(ERR_COUNTDOWN,     "err.countdown",      L"Choose a countdown duration") \
    X(ERR_IDLE,          "err.idle",           L"Choose an idle duration") \
    X(ERR_INTERVAL,      "err.interval",       L"Interval minutes must be > 0.") \
    X(ERR_WAKE,          "err.wake",           L"Couldn't create the wake task in Task Scheduler.") \
    X(ERR_EXEC,          "err.exec",           L"Failed to execute '%ls'. (Sleep/Hibernate may be disabled on this PC.)") \
    X(HERO_OFF,          "hero.off",           L"Not scheduled") \
    X(HERO_IDLE,         "hero.idle",          L"Scheduled \x00B7 waiting for %ls of inactivity") \
    X(HERO_LEFT,         "hero.left",          L"Scheduled for %ls \x00B7 %ls left") \
    X(BTN_START,         "btn.start",          L"Start") \
    X(BTN_CANCEL,        "btn.cancel",         L"Cancel") \
    X(HDR_SCHEDULE,      "hdr.schedule",       L"Schedule") \
    X(HDR_TASK,          "hdr.task",           L"Task") \
    X(HDR_OPTIONS,       "hdr.options",        L"Options") \
    X(ACTION_TITLE,      "action.title",       L"Action") \
    X(ACTION_SUB,        "action.sub",         L"What happens when the time comes") \
    X(SCHED_TITLE,       "sched.title",        L"Schedule") \
    X(SCHED_SUB,         "sched.sub",          L"Choose what triggers the action") \
    X(TIME_TITLE,        "time.title",         L"Time") \
    X(TIME_SUB_WEEKLY,   "time.sub_weekly",    L"Runs at this time on the days below") \
    X(TIME_SUB_CD,       "time.sub_countdown", L"Runs %ls from Start") \
    X(TIME_SUB_IDLE,     "time.sub_idle",      L"After %ls with no mouse or keyboard") \
    X(TIME_SUB_NEXT,     "time.sub_next",      L"Next: %ls") \
    X(DAYS_TITLE,        "days.title",         L"Repeat on") \
    X(DAYS_SUB,          "days.sub",           L"Repeats %ls") \
    X(OPT_REMIND,        "opt.remind",         L"Remind me 1 minute before") \
    X(OPT_FORCE,         "opt.force",          L"Force apps to close") \
    X(OPT_AUTO,          "opt.auto",           L"Start with Windows") \
    X(OPT_TRAY,          "opt.tray",           L"Tray icon") \
    X(OPT_WAKE,          "opt.wake",           L"Wake up") \
    X(OPT_WAKE_TIME,     "opt.wake_time",      L"Wake time") \
    X(OPT_ON,            "opt.on",             L"On") \
    X(OPT_OFF,           "opt.off",            L"Off") \
    X(LANGUAGE_TITLE,    "language.title",     L"Language") \
    X(LANGUAGE_DEFAULT,  "language.default",   L"Windows default (%ls)") \
    X(LINK_ABOUT,        "link.about",         L"About") \
    X(ACC_COUNTDOWN,     "acc.countdown",      L"Countdown") \
    X(ACC_IDLE,          "acc.idle",           L"Idle time") \
    X(ACC_TIME,          "acc.time",           L"Opens the time picker") \
    X(ACC_WAKE_TIME,     "acc.wake_time",      L"Not from shut down: the PC must be asleep") \
    X(ACC_REMIND,        "acc.remind",         L"1 minute countdown before the action, to stop or snooze") \
    X(ACC_FORCE,         "acc.force",          L"Apps with unsaved work won't stop the action") \
    X(ACC_AUTO,          "acc.auto",           L"Run in the system tray when you sign in") \
    X(ACC_TRAY,          "acc.tray",           L"Off: close exits; Windows runs the task") \
    X(ACC_WAKE,          "acc.wake",           L"Wake the PC from sleep every day") \
    X(ACC_HERO,          "acc.hero",           L"%ls. %ls") \
    X(ACC_ABOUT,         "acc.about",          L"%ls, version %ls") \
    X(TRAY_DELEGATED,    "tray.delegated",     L"Scheduled in Windows Task Scheduler") \
    X(TRAY_SHOW,         "tray.show",          L"Show window") \
    X(TRAY_START,        "tray.start",         L"Start task") \
    X(TRAY_CANCEL,       "tray.cancel",        L"Cancel task") \
    X(TRAY_SLEEP_NOW,    "tray.sleep_now",     L"Sleep now") \
    X(TRAY_EXIT,         "tray.exit",          L"Exit") \
    X(TRAY_EXIT_KEEP,    "tray.exit_keep",     L"Exit (Windows keeps the task)") \
    X(RM_SHUTDOWN,       "rm.shutdown",        L"PowerNapp will shut down the PC when the countdown ends.") \
    X(RM_RESTART,        "rm.restart",         L"PowerNapp will restart the PC when the countdown ends.") \
    X(RM_SLEEP,          "rm.sleep",           L"PowerNapp will put the PC to sleep when the countdown ends.") \
    X(RM_HIBERNATE,      "rm.hibernate",       L"PowerNapp will hibernate the PC when the countdown ends.") \
    X(RM_LOGOFF,         "rm.logoff",          L"PowerNapp will sign you out when the countdown ends.") \
    X(RM_LOCK,           "rm.lock",            L"PowerNapp will lock the PC when the countdown ends.") \
    X(RM_DISPLAY,        "rm.display",         L"PowerNapp will turn off the display when the countdown ends.") \
    X(RM_TITLE,          "rm.title",           L"%ls in %ls") \
    X(RM_NOW,            "rm.now",             L"%ls now") \
    X(RM_SNOOZE,         "rm.snooze",          L"Snooze 10 min") \
    X(RM_STOP,           "rm.stop",            L"Stop") \
    X(RM_WINDOW,         "rm.window",          L"PowerNapp reminder") \
    X(RM_ACC_NAME,       "rm.acc_name",        L"%ls in %d seconds") \
    X(RM_ACC_DESC,       "rm.acc_desc",        L"%ls Buttons: %ls now, Snooze 10 minutes, Stop. Escape stops.") \
    X(ABOUT_TITLE,       "about.title",        L"About PowerNapp") \
    X(ABOUT_VERSION,     "about.version",      L"Version %ls") \
    X(ABOUT_DESC,        "about.desc",         L"A tiny app that shuts down, restarts or sleeps your PC on a schedule. Windows Task Scheduler does the work, so it uses no memory once closed.") \
    X(ABOUT_ASK,         "about.ask",          L"Questions, concerns or a gig to offer?") \
    X(ABOUT_COPYRIGHT,   "about.copyright",    L"\x00A9 2026 PowerNapps (by Denis Platonov), Los Angeles") \
    X(ABOUT_CONTACT,     "about.contact",      L"Contact us") \
    X(ABOUT_LINK,        "about.link",         L"More cool apps at powernapps.net") \
    X(ABOUT_UPDATES,     "about.updates",      L"Check for updates") \
    X(ABOUT_OK,          "about.ok",           L"OK") \
    X(ABOUT_CONTACT_ACC, "about.contact_acc",  L"Contact us button") \
    X(ABOUT_LINK_ACC,    "about.link_acc",     L"More cool apps at powernapps.net link") \
    X(ABOUT_UPDATES_ACC, "about.updates_acc",  L"Check for updates button") \
    X(ABOUT_OK_ACC,      "about.ok_acc",       L"OK button") \
    X(ABOUT_ACC,         "about.acc",          L"%ls. %ls %ls %ls. Controls: %ls, %ls, %ls, %ls.") \
    X(TS_DESC,           "ts.desc",            L"Created by PowerNapp. Change or cancel it in PowerNapp.")

typedef enum {
#define X(id, k, e) L_##id,
    STRINGS(X)
#undef X
    L_COUNT
} StrId;
