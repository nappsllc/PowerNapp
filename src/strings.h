/* String table: X(ID, "file.key", L"English text").  Placeholders (%ls %d %lld) in a translation must
   match the English text's, or the translation is ignored.  Escapes in lang files: \n and \\. */
#define STRINGS(X) \
    X(ACTION_TITLE, "action.title", L"Action") \
    X(TEST_LS,      "test.ls",      L"Hello %ls") \
    X(TEST_D,       "test.d",       L"Count %d")

typedef enum {
#define X(id, k, e) L_##id,
    STRINGS(X)
#undef X
    L_COUNT
} StrId;
