/* single.c: Single User HTTP Server */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

/**
 * Handle one HTTP request at a time.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Exit status of server (EXIT_SUCCESS).
 **/
int single_server(int sfd)
{
    Request *request;
    /* Accept and handle HTTP request */
    while (true)
    {
        /* Accept request */
        request = accept_request(sfd);
        if (!request)
        {
            log("Unable to accept request: %s", strerror(errno));
            continue;
        }

        /* Handle request */
        handle_request(request);

        /* Free request */
        free_request(request);
    }

    /* Close server socket */
    close(sfd);
    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
