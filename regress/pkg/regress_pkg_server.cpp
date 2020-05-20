/*            R E G R E S S _ P K G _ S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file regress_pkg_server.c
 *
 * Server program for the libpkg regression test.  This is similar to the
 * libpkg example - the main difference being that it exits immediately with an
 * error when anything goes wrong.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/vls.h"
#include "bu/snooze.h"
#include "bu/time.h"
#include "pkg.h"
#include "regress_pkg_protocol.h"

/*
 * callback when a HELO message packet is received.
 *
 * We should not encounter this packet specifically since we listened
 * for it before beginning processing of packets as part of a simple
 * handshake setup.
 */
void
server_helo(struct pkg_conn *UNUSED(connection), char *UNUSED(buf))
{
    bu_exit(-1, "Unexpected HELO encountered\n");
}

/* callback when a DATA message packet is received */
void
server_data(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("Received message from client: %s\n", buf);
    free(buf);
}


/* callback when a CIAO message packet is received */
void
server_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CIAO encountered: %s\n", buf);
    free(buf);
}

int
main(int UNUSED(argc), const char *argv[]) {
    int port = 2000;
    struct pkg_conn *client;
    int netfd;
    char portname[MAX_PORT_DIGITS + 1] = {0};
    /* int pkg_result  = 0; */
    struct bu_vls buffer = BU_VLS_INIT_ZERO;
    char *msgbuffer;
    long bytes = 0;
    /** our server callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, server_helo, "HELO", NULL},
	{MSG_DATA, server_data, "DATA", NULL},
	{MSG_CIAO, server_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    bu_setprogname(argv[0]);

    /* ignore broken pipes, on platforms where we have SIGPIPE */
#ifdef SIGPIPE
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    /* start up the server on the given port */
    snprintf(portname, MAX_PORT_DIGITS, "%d", port);
    netfd = pkg_permserver(portname, "tcp", 0, 0);
    if (netfd < 0) {
	bu_exit(-1, "Unable to start the server");
    }

    /* listen for a good client indefinitely.  this is a simple
     * handshake that waits for a HELO message from the client.  if it
     * doesn't get one, the server continues to wait.
     */
    int64_t timer = bu_gettime();
    bu_log("Listening on port %d\n", port);
    do {
	client = pkg_getclient(netfd, callbacks, NULL, 1);
	if (client == PKC_NULL) {
	    // If we've been trying for more than 10 seconds, something
	    // went wrong with the test.
	    if ((bu_gettime() - timer) > BU_SEC2USEC(10.0)) {
		bu_log("Connection inactive for >10 seconds, quitting.\n");
		bu_exit(1, "Timeout - inactive");
	    }
	    bu_log("Connection seems to be busy, waiting...\n");
	    bu_snooze(BU_SEC2USEC(0.1));
	    continue;
	} else if (client == PKC_ERROR) {
	    pkg_close(client);
	    client = PKC_NULL;
	    bu_exit(-1, "Fatal error accepting client connection.\n");
	    continue;
	}

	// Something happened - reset idle timer
	timer = bu_gettime();

	/* got a connection, process it */
	msgbuffer = pkg_bwaitfor (MSG_HELO, client);
	if (msgbuffer == NULL) {
	    bu_log("Failed to process the client connection, still waiting\n");
	    pkg_close(client);
	    client = PKC_NULL;
	} else {
	    bu_log("msgbuffer: %s\n", msgbuffer);
	    /* validate magic header that client should have sent */
	    if (!BU_STR_EQUAL(msgbuffer, MAGIC_ID)) {
		bu_log("Bizarre corruption, received a HELO without at matching MAGIC ID!\n");
		pkg_close(client);
		client = PKC_NULL;
	    }
	}
    } while (client == PKC_NULL);

    /* send the first message to the server */
    bu_vls_sprintf(&buffer, "This is a message from the server.");
    bytes = pkg_send(MSG_DATA, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
    if (bytes < 0) goto failure;

    /* send another message to the server */
    bu_vls_sprintf(&buffer, "Yet another message from the server.");
    bytes = pkg_send(MSG_DATA, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
    if (bytes < 0) goto failure;

    /* Tell the client we're done */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, client);
    if (bytes < 0) {
	bu_exit(-1, "Connection to client seems faulty.\n");
    }

    /* Wait to hear from the client */
    do {
	(void)pkg_process(client);
	(void)pkg_suckin(client);
	(void)pkg_process(client);
    } while (client->pkc_type != MSG_CIAO);


    /* Confirm the client is done */
    (void)pkg_bwaitfor(MSG_CIAO , client);

    /* shut down the server, one-time use */
    pkg_close(client);
    return 0;

failure:
    pkg_close(client);
    bu_vls_free(&buffer);
    bu_exit(-1, "Unable to successfully send message.\n");
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
