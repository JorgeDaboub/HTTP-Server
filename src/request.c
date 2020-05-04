/* request.c: HTTP Request Functions */

#include "spidey.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

int parse_request_method(Request *r);
int parse_request_headers(Request *r);

/**
 * Accept request from server socket.
 *
 * @param   sfd         Server socket file descriptor.
 * @return  Newly allocated Request structure.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
Request *accept_request(int sfd)
{
    Request *r;
    struct sockaddr raddr;
    socklen_t rlen;
    rlen = sizeof(struct sockaddr);

    /* Allocate request struct (zeroed) */
    r = calloc(1, sizeof(Request));
    if (!(r))
    {
        debug("Allocating request failed: %s", strerror(errno));
    }

    r->headers = calloc(1, sizeof(Header));
    if(!r->headers){
      debug("Can't allocate headers: %s", strerror(errno));
    }

    /* Accept a client */
    r->fd = accept(sfd, &raddr, &rlen);
    if (r->fd < 0)
    {
        debug("Unable to accept client: %s", strerror(errno));
        goto fail;
    }

    /* Lookup client information */
    int status = getnameinfo(&raddr, rlen, r->host, sizeof(r->host), r->port, sizeof(r->port), NI_NUMERICHOST | NI_NUMERICSERV);
    if (status < 0)
    {
        debug("Failed to getnameinfo: %s", gai_strerror(errno));
        goto fail;
    }
    /* Open socket stream */
    r->stream = fdopen(r->fd, "w+");
    if (!r->stream)
    {
        debug("Unable to fdopen: %s", strerror(errno));
        goto fail;
    }

    log("Accepted request from %s:%s", r->host, r->port);
    return r;

fail:
    /* Deallocate request struct */
    free_request(r);
    return NULL;
}

/**
 * Deallocate request struct.
 *
 * @param   r           Request structure.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/
void free_request(Request *r)
{
    if (!r)
    {
        return;
    }

    /* Close socket or fd */
    if (r->stream)
        fclose(r->stream);
    else if (r->fd)
        close(r->fd);

    /* Free allocated strings */
    free(r->method);
    free(r->uri);
    free(r->path);
    free(r->query);

    /* Free headers */
    struct header *next;
    while (r->headers)
    {
        next = r->headers->next;
        free(r->headers->name);
        free(r->headers->data);
        free(r->headers);
        r->headers = next;
    }

    /* Free request */
    free(r);
    log("Free r Complete");
}

/**
 * Parse HTTP Request.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
int parse_request(Request *r)
{
    /* Parse HTTP Request Method */
    if (parse_request_method(r))
    {
        debug("Unable to parse request method.");
        return -1;
    }
    /* Parse HTTP Requet Headers*/
    if (parse_request_headers(r))
    {
        debug("Unable to parse request headers.");
        return -1;
    }

    return 0;
}

/**
 * Parse HTTP Request Method and URI.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int parse_request_method(Request *r)
{
    char buffer[BUFSIZ];
    char *method;
    char *uri;
    char *query;

    /* Read line from socket */
    if (!fgets(buffer, BUFSIZ, r->stream))
    {
        debug("Unable to read from socket: %s", strerror(errno));
        return -1;
    }
    /* Parse method and uri */
    method = strtok(buffer, WHITESPACE);
    if (!method)
    {
        debug("Error with method");
        return -1;
    }

    uri = strtok(NULL, WHITESPACE);
    if (!uri)
    {
        debug("Error with uri");
        return -1;
    }

    /* Parse query from uri */
    query = strchr(uri, '?');
    if(query){
      *query++ = '\0';
    }else{
      query = "";
    }

  /* Record method, uri, and query in request struct */
    r->method = strdup(method);
    r->uri = strdup(uri);
    r->query = strdup(query);


    debug("HTTP METHOD: %s", r->method);
    debug("HTTP URI:    %s", r->uri);
    debug("HTTP QUERY:  %s", r->query);

    return 0;
}

/**
 * Parse HTTP Request Headers.
 *
 * @param   r           Request structure.
 * @return  -1 on error and 0 on success.
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <DATA>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, data  = buffer.split(':')
 *      header      = new Header(name, data)
 *      headers.append(header)
 **/
int parse_request_headers(Request *r)
{
    Header *curr = NULL;
    char buffer[BUFSIZ];
    char *name;
    char *data;

    /* Parse headers from socket */

    while (fgets(buffer, BUFSIZ, r->stream) && strlen(buffer) > 2)
    {
        chomp(buffer);
        data = strchr(buffer, ':');
        debug("data: %s\n", data);
        if (!data)
        {
            return -1;
        }
        data = data + 1;
        data = skip_whitespace(data);
        name = strtok(buffer, ":");
        debug("Name: %s\n", name);
        if (!name)
        {
            return -1;
        }

        curr = calloc(1, sizeof(Header));
        curr->data = strdup(data);
        curr->name = strdup(name);
        curr->next = r->headers;
        r->headers = curr;
    }
    if (!r->headers)
        return -1;

#ifndef NDEBUG
    for (Header *header = r->headers; header; header = header->next)
    {
        debug("HTTP HEADER %s = %s", header->name, header->data);
    }
#endif
    return 0;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
