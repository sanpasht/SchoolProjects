/**
 * Tony Givargis
 * Copyright (C), 2023-2025
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * jitc.c
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>
#include "system.h"
#include "jitc.h"

struct jitc {
    void *handle;
};

int
jitc_compile(const char *input, const char *output)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        TRACE("fork()");
        return -1;
    }

    if (pid == 0) {
        char *argv[7];
        argv[0] = "gcc";
        argv[1] = "-fPIC";
        argv[2] = "-shared";
        argv[3] = "-o";
        argv[4] = (char *)output;
        argv[5] = (char *)input;
        argv[6] = NULL;

        execv("/usr/bin/gcc", argv);

        TRACE("execv()");
        _exit(1);
    }

    if (waitpid(pid, &status, 0) < 0) {
        TRACE("waitpid()");
        return -1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0; 
    }

    return -1; 
}

struct jitc *
jitc_open(const char *pathname)
{
    struct jitc *j;

    j = malloc(sizeof(struct jitc));
    if (!j) {
        TRACE("malloc()");
        return NULL;
    }

    j->handle = dlopen(pathname, RTLD_NOW);
    if (!j->handle) {
        const char *err = dlerror();
        TRACE(err ? err : "dlopen()");
        FREE(j);
        return NULL;
    }
    return j;
}

void
jitc_close(struct jitc *j)
{
    if (j) {
        if (j->handle) {
            dlclose(j->handle);
            j->handle = NULL;
        }
        FREE(j);
    }
}

long
jitc_lookup(struct jitc *j, const char *symbol)
{
    void *sym;

    if (!j || !j->handle) {
        return 0;
    }

    sym = dlsym(j->handle, symbol);
    if (!sym) {
        const char *err = dlerror();
        TRACE(err ? err : "dlsym()");
        return 0;
    }

    return (long)(intptr_t)sym;
}



/**
 * Needs:
 *   fork()
 *   execv()
 *   waitpid()
 *   WIFEXITED()
 *   WEXITSTATUS()
 *   dlopen()
 *   dlclose()
 *   dlsym()
 */

/* research the above Needed API and design accordingly */
