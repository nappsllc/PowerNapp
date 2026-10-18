/* PowerNapp: where the running exe lives. Part of the unity build; needs only <windows.h>/<wchar.h>, so the unit tests include it too. */

/* True when exe is %LOCALAPPDATA%\Programs\PowerNapp\<file>: the folder the installer uses (case-insensitive,
 * trailing backslashes on LOCALAPPDATA ignored). A portable or test copy anywhere else is false. */
static bool is_installed_location(const wchar_t *exe, const wchar_t *localappdata) {
    static const wchar_t SUB[] = L"\\Programs\\PowerNapp";
    if (!exe || !localappdata) return false;
    size_t ll = wcslen(localappdata);
    while (ll && localappdata[ll - 1] == L'\\') ll--;
    if (!ll) return false;
    size_t sl = wcslen(SUB), dl = ll + sl;
    const wchar_t *bs = wcsrchr(exe, L'\\');
    if (!bs || (size_t)(bs - exe) != dl) return false; /* the exe sits directly in that folder */
    return _wcsnicmp(exe, localappdata, ll) == 0 && _wcsnicmp(exe + ll, SUB, sl) == 0;
}
