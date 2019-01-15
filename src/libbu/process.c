/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include <stdlib.h> /* exit */
#include <sys/types.h>
#include "bio.h"
#include "bu/file.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/vls.h"

#if !defined(HAVE_DECL_WAIT) && !defined(wait) && !defined(_WINSOCKAPI_)
extern pid_t wait(int *);
#endif

int
bu_process_id()
{
#ifdef HAVE_UNISTD_H
    return getpid();
#else
    return (int)GetCurrentProcessId();
#endif
}


struct bu_process {
    struct bu_list l;
    FILE *fp_in;
    FILE *fp_out;
    FILE *fp_err;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE fd_in;
    HANDLE fd_out;
    HANDLE fd_err;
    HANDLE hProcess;
    DWORD pid;
#else
    int fd_in;
    int fd_out;
    int fd_err;
    int pid;
#endif
    int aborted;
};


void
bu_process_close(struct bu_process *pinfo, int fd)
{
    if (!pinfo) return;
    if (fd == 0) {
	if (!pinfo->fp_in) return;
	(void)fclose(pinfo->fp_in);
	pinfo->fp_in = NULL;
	return;
    }
    if (fd == 1) {
	if (!pinfo->fp_out) return;
	(void)fclose(pinfo->fp_out);
	pinfo->fp_out = NULL;
	return;
    }
    if (fd == 2) {
	if (!pinfo->fp_err) return;
	(void)fclose(pinfo->fp_err);
	pinfo->fp_err = NULL;
	return;
    }
}

FILE *
bu_process_open(struct bu_process *pinfo, int fd)
{
    if (!pinfo) return NULL;

    bu_process_close(pinfo, fd);

    if (fd == 0) {
#ifndef _WIN32
	pinfo->fp_in = fdopen(pinfo->fd_in, "wb");
#else
	pinfo->fp_in = _fdopen(_open_osfhandle((intptr_t)pinfo->fd_in, _O_TEXT), "wb");
#endif
	return pinfo->fp_in;
    }
    if (fd == 1) {
#ifndef _WIN32
	pinfo->fp_out = fdopen(pinfo->fd_out, "rb");
#else
	pinfo->fp_out = _fdopen(_open_osfhandle((intptr_t)pinfo->fd_out, _O_TEXT), "rb");
#endif
	return pinfo->fp_out;
    }
    if (fd == 2) {
#ifndef _WIN32
	pinfo->fp_err = fdopen(pinfo->fd_err, "rb");
#else
	pinfo->fp_err = _fdopen(_open_osfhandle((intptr_t)pinfo->fd_err, _O_TEXT), "rb");
#endif
	return pinfo->fp_err;
    }

    return NULL;
}


void *
bu_process_fd(struct bu_process *pinfo, int fd)
{
    if (!pinfo || (fd < 0 || fd > 2)) return NULL;
    if (fd == 0) {
	return (void *)(&(pinfo->fd_in));
    }
    if (fd == 1) {
	return (void *)(&(pinfo->fd_out));
    }
    if (fd == 2) {
	return (void *)(&(pinfo->fd_err));
    }
    return NULL;
}

int
bu_process_pid(struct bu_process *pinfo)
{
    if (!pinfo) return -1;
    return (int)pinfo->pid;
}

int
bu_process_read(char *buff, int *count, struct bu_process *pinfo, int fd, int n)
{
    int ret = 1;
    if (!pinfo || !buff || !n || !count || (fd < 1 || fd > 2)) return -1;
    if (fd == 1) {
#ifndef _WIN32
	(*count) = read((int)pinfo->fd_out, buff, n);
	if ((*count) <= 0) {
	    ret = -1;
	}
#else
	DWORD dcount;
	BOOL rf = ReadFile(pinfo->fd_out, buff, n, &dcount, 0);
	if (!rf || (rf && dcount == 0)) {
	    ret = -1;
	}
	(*count) = (int)dcount;
#endif
    }
    if (fd == 2) {
#ifndef _WIN32
	(*count) = read((int)pinfo->fd_err, buff, n);
	if ((*count) <= 0) {
	    ret = -1;
	}
#else
	DWORD dcount;
	BOOL rf = ReadFile(pinfo->fd_err, buff, n, &dcount, 0);
	if (!rf || (rf && dcount == 0)) {
	    ret = -1;
	}
	(*count) = (int)dcount;
#endif
    }
    /* sanity clamping */
    if ((*count) < 0) {
	perror("READ ERROR");
	(*count) = 0;
    } else if ((*count) > n) {
	(*count) = n;
    }

    return ret;
}


void
#ifdef _WIN32
bu_process_exec(struct bu_process **p, const char *cmd, int argc, const char **argv, int out_eql_err, int hide_window)
#else
bu_process_exec(struct bu_process **p, const char *cmd, int argc, const char **argv, int out_eql_err, int UNUSED(hide_window))
#endif
{
    int pret = 0;
#ifdef HAVE_UNISTD_H
    int pid;
    int pipe_in[2];
    int pipe_out[2];
    int pipe_err[2];
    const char **av = NULL;
    if (!p || !cmd) {
	return;
    }

    BU_GET(*p, struct bu_process);
    (*p)->fp_in = NULL;
    (*p)->fp_out = NULL;
    (*p)->fp_err = NULL;

    av = (const char **)bu_calloc(argc+2, sizeof(char *), "argv array");
    if (!argc || !BU_STR_EQUAL(cmd, argv[0])) {
	/* By convention the first argument to execvp should match the
	 * cmd string - if it doesn't we can handle it in av, but it
	 * means the actual exec av array will be longer by one. */
	av[0] = cmd;
	for (int i = 1; i <= argc; i++) {
	    av[i] = argv[i-1];
	}
	av[argc+1] = (char *)NULL;
    } else {
	for (int i = 0; i < argc; i++) {
	    av[i] = argv[i];
	}
	av[argc] = (char *)NULL;
    }

    pret = pipe(pipe_in);
    if (pret < 0) {
	perror("pipe");
    }

    pret = pipe(pipe_out);
    if (pret < 0) {
	perror("pipe");
    }

    pret = pipe(pipe_err);
    if (pret < 0) {
	perror("pipe");
    }

    /* fork + exec */
    if ((pid = fork()) == 0) {
	/* make this a process group leader */
	setpgid(0, 0);

	/* Redirect stdin and stderr */
	(void)close(0);
	pret = dup(pipe_in[0]);
	if (pret < 0) {
	    perror("dup");
	}
	(void)close(1);
	pret = dup(pipe_out[1]);
	if (pret < 0) {
	    perror("dup");
	}
	(void)close(2);
	pret = dup(pipe_err[1]);
	if (pret < 0) {
	    perror("dup");
	}

	/* close pipes */
	(void)close(pipe_in[0]);
	(void)close(pipe_in[1]);
	(void)close(pipe_out[0]);
	(void)close(pipe_out[1]);
	(void)close(pipe_err[0]);
	(void)close(pipe_err[1]);

	for (int i = 3; i < 20; i++) {
	    (void)close(i);
	}

	(void)execvp(cmd, (char * const*)av);
	perror(cmd);
	exit(16);
    }

    (void)close(pipe_in[0]);
    (void)close(pipe_out[1]);
    (void)close(pipe_err[1]);

    /* Save necessary information for parental process manipulation */
    (*p)->fd_in = pipe_in[1];
    if (out_eql_err) {
	(*p)->fd_out = pipe_err[0];
    } else {
	(*p)->fd_out = pipe_out[0];
    }
    (*p)->fd_err = pipe_err[0];
    (*p)->pid = pid;

#else
    struct bu_vls cp_cmd = BU_VLS_INIT_ZERO;
    HANDLE pipe_in[2], pipe_inDup;
    HANDLE pipe_out[2], pipe_outDup;
    HANDLE pipe_err[2], pipe_errDup;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};
    if (!cmd || !argc || !argv) {
	return;
    }

    BU_GET(*p, struct bu_process);
    (*p)->fp_in = NULL;
    (*p)->fp_out = NULL;
    (*p)->fp_err = NULL;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    /* Create a pipe for the child process's STDOUT. */
    CreatePipe(&pipe_out[0], &pipe_out[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_out[0],
	    GetCurrentProcess(),  &pipe_outDup ,
	    0,  FALSE,
	    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_out[0]);

    /* Create a pipe for the child process's STDERR. */
    CreatePipe(&pipe_err[0], &pipe_err[1], &sa, 0);

    /* Create noninheritable read handle and close the inheritable read handle. */
    DuplicateHandle(GetCurrentProcess(), pipe_err[0],
	    GetCurrentProcess(),  &pipe_errDup ,
	    0,  FALSE,
	    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_err[0]);

    /* Create a pipe for the child process's STDIN. */
    CreatePipe(&pipe_in[0], &pipe_in[1], &sa, 0);

    /* Duplicate the write handle to the pipe so it is not inherited. */
    DuplicateHandle(GetCurrentProcess(), pipe_in[1],
	    GetCurrentProcess(), &pipe_inDup,
	    0, FALSE,                  /* not inherited */
	    DUPLICATE_SAME_ACCESS);
    CloseHandle(pipe_in[1]);

    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop = NULL;
    if (hide_window) {
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
    } else {
	si.dwFlags = STARTF_USESTDHANDLES;
    }
    si.hStdInput   = pipe_in[0];
    if (out_eql_err) {
	si.hStdOutput  = pipe_err[1];
    } else {
	si.hStdOutput  = pipe_out[1];
    }
    si.hStdError   = pipe_err[1];

    /* Create_Process uses a string, not a char array */
    for (int i = 0; i < argc; i++) {
	/* Quote all path names for CreateProcess */
	if (bu_file_exists(argv[i], NULL)) {
	    bu_vls_printf(&cp_cmd, "\"%s\" ", argv[i]);
	} else {
	    bu_vls_printf(&cp_cmd, "%s ", argv[i]);
	}
    }

    CreateProcess(NULL, bu_vls_addr(&cp_cmd), NULL, NULL, TRUE,
	    DETACHED_PROCESS, NULL, NULL,
	    &si, &pi);
    bu_vls_free(&cp_cmd);

    CloseHandle(pipe_in[0]);
    CloseHandle(pipe_out[1]);
    CloseHandle(pipe_err[1]);

    /* Save necessary information for parental process manipulation */
    (*p)->fd_in = pipe_inDup;
    if (out_eql_err) {
	(*p)->fd_out = pipe_errDup;
    } else {
	(*p)->fd_out = pipe_outDup;
    }
    (*p)->fd_err = pipe_errDup;
    (*p)->hProcess = pi.hProcess;
    (*p)->pid = pi.dwProcessId;
    (*p)->aborted = 0;

#endif
}

int
#ifndef _WIN32
bu_process_wait(int *aborted, struct bu_process *pinfo, int UNUSED(wtime))
#else
bu_process_wait(int *aborted, struct bu_process *pinfo, int wtime)
#endif
{
    int rc = 0;
#ifndef _WIN32
    int rpid;
    int retcode = 0;
    if (!pinfo) return -1;
    close(pinfo->fd_out);
    close(pinfo->fd_err);
    while ((rpid = wait(&retcode)) != pinfo->pid && rpid != -1);
    rc = retcode;
#else
    DWORD retcode = 0;
    if (!pinfo) return -1;

    /* wait for the forked process */
    if (wtime > 0) {
	WaitForSingleObject(pinfo->hProcess, wtime);
    } else {
	WaitForSingleObject(pinfo->hProcess, INFINITE);
    }

    if (GetLastError() == ERROR_PROCESS_ABORTED) {
	pinfo->aborted = 1;
    }

    GetExitCodeProcess(pinfo->hProcess, &retcode);
    /* may be useful to try pr_wait_status() here */
    rc = (int)retcode;
#endif
    if (aborted) {
	(*aborted) = pinfo->aborted;
    }

    /* Clean up */
    bu_process_close(pinfo, 1);
    bu_process_close(pinfo, 2);
    BU_PUT(pinfo, struct bu_process);

    return rc;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
