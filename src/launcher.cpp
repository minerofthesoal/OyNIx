/*
 * OyNIx Browser v2.3 - Cross-Platform Native Launcher
 * Works on Linux, Windows, and macOS.
 * Finds Python 3 and launches the OyNIx browser module.
 *
 * Compile Linux:   g++ -O2 -o oynix src/launcher.cpp
 * Compile Windows: cl /O2 /EHsc src/launcher.cpp /Fe:oynix.exe
 * Compile macOS:   clang++ -O2 -o oynix src/launcher.cpp
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #include <direct.h>
    #define PATH_SEP "\\"
    #define ENV_SEP ";"
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <libgen.h>
    #include <errno.h>
    #define PATH_SEP "/"
    #define ENV_SEP ":"
#endif

static const int MAX_BUF = 4096;

// ── Color output (disabled on Windows when no ANSI support) ────────
#ifdef _WIN32
static bool has_ansi = false;
static void init_colors() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            has_ansi = SetConsoleMode(h, mode);
        }
    }
}
#define PURPLE (has_ansi ? "\033[95m" : "")
#define BOLD   (has_ansi ? "\033[1m"  : "")
#define DIM    (has_ansi ? "\033[2m"  : "")
#define RESET  (has_ansi ? "\033[0m"  : "")
#else
static void init_colors() {}
#define PURPLE "\033[95m"
#define BOLD   "\033[1m"
#define DIM    "\033[2m"
#define RESET  "\033[0m"
#endif

// ── Utility ────────────────────────────────────────────────────────
static bool file_exists(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

static bool command_exists(const char *cmd) {
#ifdef _WIN32
    std::string check = std::string("where ") + cmd + " >nul 2>&1";
#else
    std::string check = std::string("command -v ") + cmd + " >/dev/null 2>&1";
#endif
    return system(check.c_str()) == 0;
}

static std::string get_exe_dir() {
    char buf[MAX_BUF];
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_BUF);
    if (len > 0) {
        // Strip filename to get directory
        char *last = strrchr(buf, '\\');
        if (last) *last = '\0';
        return std::string(buf);
    }
    _getcwd(buf, MAX_BUF);
    return std::string(buf);
#elif defined(__APPLE__)
    // macOS: use _NSGetExecutablePath or fall back to cwd
    uint32_t size = MAX_BUF;
    extern int _NSGetExecutablePath(char *, uint32_t *);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        return std::string(dirname(buf));
    }
    getcwd(buf, MAX_BUF);
    return std::string(buf);
#else
    ssize_t len = readlink("/proc/self/exe", buf, MAX_BUF - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(dirname(buf));
    }
    getcwd(buf, MAX_BUF);
    return std::string(buf);
#endif
}

// ── Find Python ────────────────────────────────────────────────────
static std::string find_python() {
    // On Windows, also check common install locations
#ifdef _WIN32
    const char *candidates[] = {
        "python", "python3", "py -3",
        NULL
    };
    // Check common Windows Python locations
    const char *win_paths[] = {
        "C:\\Python312\\python.exe",
        "C:\\Python311\\python.exe",
        "C:\\Python310\\python.exe",
        NULL
    };
    // Check LOCALAPPDATA
    char local_app[MAX_BUF];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app) == S_OK) {
        std::string versions[] = {"312", "311", "310"};
        for (auto &v : versions) {
            std::string path = std::string(local_app) +
                "\\Programs\\Python\\Python" + v + "\\python.exe";
            if (file_exists(path.c_str())) {
                return path;
            }
        }
    }
#else
    const char *candidates[] = {
        "python3.12", "python3.11", "python3.10", "python3", "python",
        NULL
    };
#endif

    for (int i = 0; candidates[i]; i++) {
        if (command_exists(candidates[i])) {
            // Verify Python 3.8+
            std::string cmd = std::string(candidates[i]) +
                " -c \"import sys; exit(0 if sys.version_info >= (3,8) else 1)\""
#ifdef _WIN32
                + " 2>nul";
#else
                + " 2>/dev/null";
#endif
            if (system(cmd.c_str()) == 0) {
                return std::string(candidates[i]);
            }
        }
    }

#ifdef _WIN32
    for (int i = 0; win_paths[i]; i++) {
        if (file_exists(win_paths[i])) return std::string(win_paths[i]);
    }
#endif

    return "";
}

// ── Data directories ───────────────────────────────────────────────
static void ensure_data_dirs() {
    std::string base;
#ifdef _WIN32
    char appdata[MAX_BUF];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        base = std::string(appdata) + "\\oynix";
    } else {
        base = std::string(getenv("USERPROFILE") ? getenv("USERPROFILE") : "C:\\Users\\Default")
            + "\\.config\\oynix";
    }
#else
    const char *home = getenv("HOME");
    if (!home) return;
    base = std::string(home) + "/.config/oynix";
#endif

    const char *subdirs[] = {
        "models", "search_index", "database", "cache", "sync", NULL
    };
    for (int i = 0; subdirs[i]; i++) {
        std::string path = base + PATH_SEP + subdirs[i];
#ifdef _WIN32
        _mkdir(path.c_str());
#else
        mkdir(path.c_str(), 0755);
#endif
    }
}

// ── Redirect to log when no terminal ───────────────────────────────
static void setup_logging() {
#ifdef _WIN32
    // On Windows, if launched from a shortcut (no console), attach or alloc
    if (GetConsoleWindow() == NULL) {
        // Don't allocate a console - just log to file
        std::string base;
        char appdata[MAX_BUF];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
            base = std::string(appdata) + "\\oynix\\launch.log";
        }
        if (!base.empty()) {
            freopen(base.c_str(), "a", stdout);
            freopen(base.c_str(), "a", stderr);
        }
    }
#else
    if (!isatty(fileno(stdout))) {
        const char *home = getenv("HOME");
        if (home) {
            std::string logpath = std::string(home) + "/.config/oynix/launch.log";
            std::string dirpath = std::string(home) + "/.config/oynix";
            mkdir(dirpath.c_str(), 0755);
            FILE *log = fopen(logpath.c_str(), "a");
            if (log) {
                dup2(fileno(log), fileno(stdout));
                dup2(fileno(log), fileno(stderr));
                fclose(log);
            }
        }
    }
#endif
}

// ── Main ───────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    init_colors();
    setup_logging();

    printf("\n");
    printf("%s%s  +===============================================+\n", PURPLE, BOLD);
    printf("  |         OyNIx Browser v2.3                   |\n");
    printf("  |    The Nyx-Powered Local AI Browser          |\n");
    printf("  +===============================================+%s\n\n", RESET);

    // Find Python
    std::string python = find_python();
    if (python.empty()) {
        fprintf(stderr,
            "ERROR: Python 3.8+ is required but not found.\n"
            "Install Python from https://python.org\n"
#ifdef _WIN32
            "Make sure to check 'Add Python to PATH' during installation.\n"
#endif
        );
#ifdef _WIN32
        MessageBoxA(NULL,
            "Python 3.8+ is required but not found.\n\n"
            "Install Python from https://python.org\n"
            "Check 'Add Python to PATH' during installation.",
            "OyNIx Browser", MB_ICONERROR);
#endif
        return 1;
    }
    printf("%s  Using: %s%s\n", DIM, python.c_str(), RESET);

    // Get our directory
    std::string exe_dir = get_exe_dir();

    // Ensure data directories
    ensure_data_dirs();

    // Set PYTHONPATH
    std::string pythonpath = exe_dir;
    const char *existing = getenv("PYTHONPATH");
    if (existing) {
        pythonpath = exe_dir + ENV_SEP + existing;
    }
#ifdef _WIN32
    _putenv_s("PYTHONPATH", pythonpath.c_str());
#else
    setenv("PYTHONPATH", pythonpath.c_str(), 1);

    // Set LD_LIBRARY_PATH for Qt6 on Linux
    std::string ld_cmd = python +
        " -c \"import PyQt6,os; print(os.path.join("
        "os.path.dirname(PyQt6.__file__),'Qt6','lib'))\" 2>/dev/null";
    FILE *fp = popen(ld_cmd.c_str(), "r");
    if (fp) {
        char qt6_lib[MAX_BUF];
        if (fgets(qt6_lib, sizeof(qt6_lib), fp)) {
            qt6_lib[strcspn(qt6_lib, "\n")] = 0;
            if (file_exists(qt6_lib)) {
                const char *eld = getenv("LD_LIBRARY_PATH");
                std::string ld_path = std::string(qt6_lib);
                if (eld) ld_path += ":" + std::string(eld);
                setenv("LD_LIBRARY_PATH", ld_path.c_str(), 1);
            }
        }
        pclose(fp);
    }
#endif

    // Launch: python -m oynix [args...]
#ifdef _WIN32
    // On Windows, use CreateProcess to avoid console flash
    // Try pythonw first for no-console launch
    std::string pythonw = python;
    size_t pos = pythonw.rfind("python.exe");
    if (pos != std::string::npos) {
        pythonw.replace(pos, 10, "pythonw.exe");
    }
    if (!file_exists(pythonw.c_str())) {
        pythonw = python;  // fall back to python.exe
    }

    std::string cmdline = "\"" + pythonw + "\" -m oynix";
    for (int i = 1; i < argc; i++) {
        cmdline += " \"";
        cmdline += argv[i];
        cmdline += "\"";
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // hide console window

    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(NULL, (LPSTR)cmdline.c_str(),
            NULL, NULL, FALSE,
            CREATE_NO_WINDOW,  // no console window
            NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }

    // Fallback: try system()
    system(cmdline.c_str());
#else
    // On Unix, exec replaces this process
    std::vector<const char*> new_argv;
    new_argv.push_back(python.c_str());
    new_argv.push_back("-m");
    new_argv.push_back("oynix");
    for (int i = 1; i < argc; i++) {
        new_argv.push_back(argv[i]);
    }
    new_argv.push_back(NULL);

    execvp(python.c_str(), (char**)new_argv.data());
    fprintf(stderr, "ERROR: Failed to launch Python: %s\n", strerror(errno));
    return 1;
#endif

    return 0;
}
