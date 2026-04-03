/*
 * OyNIx Browser - Native Launcher
 * Finds Python 3 and launches the OyNIx browser module.
 * Falls back through multiple Python paths for compatibility.
 *
 * Compile: gcc -O2 -o oynix src/launcher.c
 * Coded by Claude (Anthropic)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

#define MAX_PATH 4096
#define PURPLE "\033[95m"
#define BOLD   "\033[1m"
#define DIM    "\033[2m"
#define RESET  "\033[0m"

/* Python interpreters to try, in order of preference */
static const char *python_candidates[] = {
    "python3.12",
    "python3.11",
    "python3.10",
    "python3",
    "python",
    NULL
};

/* Check if a command exists in PATH */
static int command_exists(const char *cmd) {
    char check[MAX_PATH];
    snprintf(check, sizeof(check), "command -v %s >/dev/null 2>&1", cmd);
    return system(check) == 0;
}

/* Check if a file exists */
static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Get the directory containing this executable */
static int get_exe_dir(char *buf, size_t size) {
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len == -1) {
        /* Fallback for non-Linux */
        char *cwd = getcwd(buf, size);
        return cwd != NULL ? 0 : -1;
    }
    buf[len] = '\0';
    /* Get dirname */
    char *dir = dirname(buf);
    memmove(buf, dir, strlen(dir) + 1);
    return 0;
}

/* Find a working Python interpreter */
static const char *find_python(void) {
    for (int i = 0; python_candidates[i]; i++) {
        if (command_exists(python_candidates[i])) {
            /* Verify it's Python 3.8+ */
            char cmd[MAX_PATH];
            snprintf(cmd, sizeof(cmd),
                "%s -c \"import sys; exit(0 if sys.version_info >= (3,8) else 1)\" "
                "2>/dev/null",
                python_candidates[i]);
            if (system(cmd) == 0) {
                return python_candidates[i];
            }
        }
    }
    return NULL;
}

/* Check and create data directories */
static void ensure_data_dirs(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    char path[MAX_PATH];
    const char *subdirs[] = {
        "models", "search_index", "database", "cache", "sync", NULL
    };

    for (int i = 0; subdirs[i]; i++) {
        snprintf(path, sizeof(path), "%s/.config/oynix/%s", home, subdirs[i]);
        mkdir(path, 0755);
    }
}

/* Check if Python deps are installed */
static int check_deps(const char *python) {
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd),
        "%s -c \"import PyQt6.QtWidgets\" 2>/dev/null", python);
    return system(cmd) == 0;
}

/* Install Python deps */
static int install_deps(const char *python, const char *exe_dir) {
    char req_path[MAX_PATH];
    snprintf(req_path, sizeof(req_path), "%s/requirements.txt", exe_dir);

    if (!file_exists(req_path)) {
        /* Try parent dir */
        snprintf(req_path, sizeof(req_path), "%s/../requirements.txt", exe_dir);
        if (!file_exists(req_path)) {
            /* Try /usr/lib/oynix */
            snprintf(req_path, sizeof(req_path),
                "/usr/lib/oynix/requirements.txt");
        }
    }

    if (!file_exists(req_path)) {
        fprintf(stderr, "Cannot find requirements.txt\n");
        return -1;
    }

    printf(DIM "  Installing dependencies..." RESET "\n");

    char cmd[MAX_PATH];
    /* Try --break-system-packages first, fall back to plain pip */
    snprintf(cmd, sizeof(cmd),
        "%s -m pip install --break-system-packages -r \"%s\" 2>/dev/null || "
        "%s -m pip install -r \"%s\"",
        python, req_path, python, req_path);

    return system(cmd);
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf(PURPLE BOLD "  ╔═══════════════════════════════════════════════╗\n");
    printf("  ║         OyNIx Browser v2.1.2                 ║\n");
    printf("  ║    The Nyx-Powered Local AI Browser          ║\n");
    printf("  ╚═══════════════════════════════════════════════╝" RESET "\n\n");

    /* Find Python */
    const char *python = find_python();
    if (!python) {
        fprintf(stderr,
            "ERROR: Python 3.8+ is required but not found.\n"
            "Install Python from https://python.org\n");
        return 1;
    }
    printf(DIM "  Using: %s" RESET "\n", python);

    /* Get our directory */
    char exe_dir[MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) != 0) {
        fprintf(stderr, "ERROR: Cannot determine executable directory\n");
        return 1;
    }

    /* Ensure data directories exist */
    ensure_data_dirs();

    /* Check if deps are installed */
    if (!check_deps(python)) {
        printf(DIM "  PyQt6 not found, installing dependencies..." RESET "\n");
        if (install_deps(python, exe_dir) != 0) {
            fprintf(stderr,
                "ERROR: Failed to install dependencies.\n"
                "Run manually: %s -m pip install -r requirements.txt\n",
                python);
            return 1;
        }
    }

    /* Set LD_LIBRARY_PATH for pip-installed Qt6 libraries */
    /* pip's PyQt6-WebEngine-Qt6 bundles .so files the linker can't find */
    char ld_cmd[MAX_PATH];
    snprintf(ld_cmd, sizeof(ld_cmd),
        "%s -c \"import PyQt6,os; print(os.path.join("
        "os.path.dirname(PyQt6.__file__),'Qt6','lib'))\" 2>/dev/null",
        python);

    FILE *fp = popen(ld_cmd, "r");
    if (fp) {
        char qt6_lib[MAX_PATH];
        if (fgets(qt6_lib, sizeof(qt6_lib), fp)) {
            /* Strip newline */
            qt6_lib[strcspn(qt6_lib, "\n")] = 0;
            if (file_exists(qt6_lib)) {
                const char *existing_ld = getenv("LD_LIBRARY_PATH");
                char ld_path[MAX_PATH * 2];
                if (existing_ld) {
                    snprintf(ld_path, sizeof(ld_path), "%s:%s",
                        qt6_lib, existing_ld);
                } else {
                    snprintf(ld_path, sizeof(ld_path), "%s", qt6_lib);
                }
                setenv("LD_LIBRARY_PATH", ld_path, 1);
            }
        }
        pclose(fp);
    }

    /* Build command: python -m oynix [args...] */
    /* Set PYTHONPATH to include our directory */
    char pythonpath[MAX_PATH * 2];
    const char *existing = getenv("PYTHONPATH");
    if (existing) {
        snprintf(pythonpath, sizeof(pythonpath), "%s:%s", exe_dir, existing);
    } else {
        snprintf(pythonpath, sizeof(pythonpath), "%s", exe_dir);
    }

    /* Also check /usr/lib/oynix */
    if (file_exists("/usr/lib/oynix/oynix/__init__.py")) {
        char tmp[MAX_PATH * 2];
        snprintf(tmp, sizeof(tmp), "/usr/lib/oynix:%s", pythonpath);
        strncpy(pythonpath, tmp, sizeof(pythonpath) - 1);
    }

    setenv("PYTHONPATH", pythonpath, 1);

    /* Build argv for exec */
    int new_argc = argc + 2;  /* python -m oynix [original args] */
    char **new_argv = malloc(sizeof(char *) * (new_argc + 1));
    if (!new_argv) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        return 1;
    }

    new_argv[0] = (char *)python;
    new_argv[1] = "-m";
    new_argv[2] = "oynix";
    for (int i = 1; i < argc; i++) {
        new_argv[i + 2] = argv[i];
    }
    new_argv[new_argc] = NULL;

    /* Replace this process with Python */
    execvp(python, new_argv);

    /* If exec failed */
    fprintf(stderr, "ERROR: Failed to launch Python: %s\n", strerror(errno));
    free(new_argv);
    return 1;
}
