/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1991 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1991 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1991 DIGITAL EQUIPMENT CORPORATION
 * Portions Copyright (c) 2010 Apple Inc.
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company, Apple Inc. or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company, Apple Inc. nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * access_server      : demo DCE RPC application
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 1
#endif

#define getopt getopt_system

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <compat/dcerpc.h>
#include <lw/base.h>
#include "access.h"
#include "misc.h"

#undef getopt

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifndef _WIN32
static void wait_for_signals();
#endif

/*
 *
 * A template DCE RPC server
 *
 * main() contains the basic calls needed to register an interface,
 * get communications endpoints, and register the endpoints
 * with the endpoint mapper.
 *
 * ReverseIt() implements the interface specified in access.idl
 *
 */

static void
bind_server(
    rpc_binding_vector_p_t * server_binding,
    rpc_if_handle_t interface_spec,
    char * protocol,
    char * endpoint
    )
{
    char * function = "n/a";
    unsigned32 status;

    /*
     * Prepare the server binding handle
     * use all avail protocols (UDP and TCP). This basically allocates
     * new sockets for us and associates the interface UUID and
     * object UUID of with those communications endpoints.
     */

#if 0
    rpc_server_use_all_protseqs_if(0, interface_spec, &status);
#else
    if (!endpoint)
    {
        if (!protocol)
        {
            function = "rpc_server_use_all_protseqs()";
            rpc_server_use_all_protseqs(rpc_c_protseq_max_calls_default, &status);
        }
        else
        {
            function = "rpc_server_use_protseq()";
            rpc_server_use_protseq(protocol, rpc_c_protseq_max_calls_default, &status);
        }
    }
    else
    {
        function = "rpc_server_use_protseq_ep()";
        rpc_server_use_protseq_ep(protocol, rpc_c_protseq_max_calls_default, endpoint, &status);
    }
#endif

    chk_dce_err(status, function, "", 1);
    rpc_server_inq_bindings(server_binding, &status);
    chk_dce_err(status, "rpc_server_inq_bindings()", "", 1);
}

static void usage()
{
    printf("usage: access_server [-e endpoint] [-n] [-u] [-t]\n");
    printf("         -e:  specify endpoint\n");
    printf("         -n:  use named pipe protocol\n");
    printf("         -u:  use UDP protocol\n");
    printf("         -t:  use TCP protocol (default)\n");
    printf("\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    unsigned32 status;
    rpc_binding_vector_p_t server_binding;
    char * string_binding;
    unsigned32 i;
    char * protocol = NULL;
    char * endpoint = NULL;
    int c;
    char * function = NULL;

    /*
     * Process the cmd line args
     */

    while ((c = getopt(argc, argv, "e:nutl")) != EOF)
    {
        switch (c)
        {
        case 'e':
            endpoint = optarg;
            break;
        case 'n':
            protocol = PROTOCOL_NP;
            break;
        case 'u':
            protocol = PROTOCOL_UDP;
            break;
        case 't':
            protocol = PROTOCOL_TCP;
            break;
        case 'l':
            protocol = PROTOCOL_LRPC;
            break;
        default:
            usage();
        }
    }

    if (endpoint && !protocol)
    {
        printf("ERROR: protocol is required when endpoint is specified\n");
        exit(1);
    }

    /*
     * Register the Interface with the local endpoint mapper (rpcd)
     */

    printf ("Registering server.... \n");
    rpc_server_register_if(access_v1_0_s_ifspec,
                           NULL,
                           NULL,
                           &status);
    chk_dce_err(status, "rpc_server_register_if()", "", 1);

    printf("registered.\nPreparing binding handle...\n");

    bind_server(&server_binding, access_v1_0_s_ifspec, protocol, endpoint);

    /*
     * Register bindings with the endpoint mapper
     */

    printf("registering bindings with endpoint mapper\n");

    rpc_ep_register(access_v1_0_s_ifspec,
                    server_binding,
                    NULL,
                    (unsigned char *)"QDA application server",
                    &status);
    chk_dce_err(status, "rpc_ep_register()", "", 1);

    printf("registered.\n");

    /*
     * Print out the servers endpoints (TCP and UDP port numbers)
     */

    printf ("Server's communications endpoints are:\n");

    for (i=0; i<RPC_FIELD_COUNT(server_binding); i++)
    {
        rpc_binding_to_string_binding(RPC_FIELD_BINDING_H(server_binding)[i],
                                      (unsigned char **)&string_binding,
                                      &status);
        if (string_binding)
            printf("\t%s\n", string_binding);
    }

#ifndef _WIN32
    /*
     * Start the signal waiting thread in background. This thread will
     * Catch SIGINT and gracefully shutdown the server.
     */

    wait_for_signals();
#endif

    /*
     * Begin listening for calls
     */

    printf ("listening for calls....\n");

    DCETHREAD_TRY
    {
        rpc_server_listen(rpc_c_listen_max_calls_default, &status);
    }
    DCETHREAD_CATCH_ALL(THIS_CATCH)
    {
        printf ("Server stoppped listening\n");
    }
    DCETHREAD_ENDTRY;

    /*
     * If we reached this point, then the server was stopped, most likely
     * by the signal handler thread called rpc_mgmt_stop_server().
     * gracefully cleanup and unregister the bindings from the
     * endpoint mapper.
     */

#ifndef _WIN32
    /*
     * Kill the signal handling thread
     */

#endif

    printf ("Unregistering server from the endpoint mapper....\n");
    rpc_ep_unregister(access_v1_0_s_ifspec,
                      server_binding,
                      NULL,
                      &status);
    chk_dce_err(status, "rpc_ep_unregister()", "", 0);

    /*
     * retire the binding information
     */

    printf("Cleaning up communications endpoints...\n");
    rpc_server_unregister_if(access_v1_0_s_ifspec,
                             NULL,
                             &status);
    chk_dce_err(status, "rpc_server_unregister_if()", "", 0);

    exit(0);
}

/*=========================================================================
 *
 * Server implementation of ReverseIt()
 *
 *=========================================================================*/

void
WhoAmI(
    rpc_binding_handle_t h,
    string_t* sid
    )
{
    PACCESS_TOKEN token = NULL;
    unsigned32 st = rpc_s_ok;
    string_t str = NULL;
    union
    {
        SID_AND_ATTRIBUTES user;
        BYTE buffer[sizeof(SID_AND_ATTRIBUTES) + SID_MAX_SIZE];
    } u;
    NTSTATUS status = 0;
    ULONG len = 0;
    PSTR sidstr = NULL;

    rpc_binding_inq_access_token_caller(
        h,
        &token,
        &st);

    if (st)
    {
        goto error;
    }

    status = RtlQueryAccessTokenInformation(
        token,
        TokenUser,
        &u.user,
        sizeof(u),
        &len);

    if (status)
    {
        goto error;
    }

    status = RtlAllocateCStringFromSid(
        &sidstr,
        u.user.Sid);

    if (status)
    {
        goto error;
    }

    str = rpc_ss_allocate(strlen(sidstr) + 1);
    strcpy(str, sidstr);

    *sid = str;

cleanup:

    RTL_FREE(&sidstr);

    return;

error:

    *sid = NULL;

    goto cleanup;
}

#ifndef _WIN32
/*=========================================================================
 *
 * wait_for_signals()
 *
 *
 * Set up the process environment to properly deal with signals.
 * By default, we isolate all threads from receiving asynchronous
 * signals. We create a thread that handles all async signals.
 * The signal handling actions are handled in the handler thread.
 *
 * For AIX, we cant use a thread that sigwaits() on a specific signal,
 * we use a plain old, lame old Unix signal handler.
 *
 *=========================================================================*/

void
wait_for_signals()
{
    sigset_t signals;

    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);

    dcethread_signal_to_interrupt(&signals, dcethread_self());
}

#endif
