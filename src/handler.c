/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Internal Declarations */
Status handle_browse_request(Request *request);
Status handle_file_request(Request *request);
Status handle_cgi_request(Request *request);
Status handle_error(Request *request, Status status);

/**
 * Handle HTTP Request.
 *
 * @param   r           HTTP Request structure
 * @return  Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
Status  handle_request(Request *r) {
    Status result;

    log("Handling request");

    /* Parse request */
    if(parse_request(r) == -1){ // -1 from function means failure
      log("parse_request failed");
      return handle_error(r, HTTP_STATUS_BAD_REQUEST);
    }

    /* Determine request path */
    r->path = determine_request_path(r->uri);
    if(!r->path){
      log("URI path missing");
      return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }
    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type based on file type */
    struct stat s;

    if (stat(r->path, &s) < 0){ // file don't exist
        log("stat call failed. File nonexistent?");
        result = handle_error(r, HTTP_STATUS_NOT_FOUND);
    }else if (S_ISDIR(s.st_mode)){  // directory
        log("Handling browse request (out)");
        result = handle_browse_request(r);
    }else if(S_ISREG(s.st_mode)){ // regular file
        if(access(r->path, X_OK) == 0){ // if can execute regular file
            log("Handling CGI request (out)");
            result = handle_cgi_request(r);
        }else if(access(r->path, R_OK) == 0){ // if can read (and can't execute) regular file
            log("Handling file request (out)");
            result = handle_file_request(r);
        }
    }else{
        log("file has insufficient permissions for any handling");
        result = handle_error(r, HTTP_STATUS_BAD_REQUEST);
    }

    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    return result;
}

/**
 * Handle browse request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_browse_request(Request *r) {
    struct dirent **entries;
    int n;

    log("Handling browsing request (in)");

    /* Open a directory for reading or scanning */
    n = scandir(r->path, &entries, 0, alphasort);
    if(n < 0){
      debug("scandir failed: %s", strerror(errno));
      return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");

    fprintf(r->stream, "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\"><link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css\" integrity=\"sha384-Vkoo8x4CGsO3+Hhxv8T/Q5PaXtkKtu6ug5TOeNV6gBiFeWPGFN9MuhOf23Q9Ifjh\" crossorigin=\"anonymous\"><style>body { background-color: rgb(128, 96, 0); } a:link { color: rgb(1, 0, 91); } ul { list-style: none; } ul li::before { content: \"•\"; color: rgb(1, 0, 91); font-weight: bold; display: inline-block; width: 1em; margin-left: -1em; } a:visited { color: rgb(1, 0, 91); } </style></head><ul>\n");

    /* For each entry in directory, emit HTML list item */
    fprintf(r->stream, "<ul>\n"); // start unordered list

    for(int i = 0; i < n; i++){ // each entry put into list
      if(!streq(entries[i]->d_name, ".")){
        fprintf(r->stream, "<li>");
        // HTML: <a href = "address that clicking takes you"> clickable text </a>
        fprintf(r->stream, "<a href=\"%s/%s\">%s</a>", streq(r->uri, "/") ? "" : r->uri,
                                                     entries[i]->d_name,
                                                     entries[i]->d_name);
        fprintf(r->stream, "</li>\n");
      }
      free(entries[i]);
    }
    free(entries);
    fprintf(r->stream, "</ul>\n"); // end unordered list
    
    for(int i = 0 ; i< 50; i++)
    fprintf(r->stream, "<br>");
    /* Return OK */
    return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_file_request(Request *r) {
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    log("Handling file request (in)");

    /* Open file for reading */
    fs = fopen(r->path, "r");
    if(!fs){
      debug("fopen failed: %s", strerror(errno));
      return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }


    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");
    fprintf(r->stream, "Content-Type: %s\r\n", mimetype);
    fprintf(r->stream, "\r\n");

    /* Read from file and write to socket in chunks */
    while((nread = fread(buffer, 1, BUFSIZ, fs)) > 0){
      if (fwrite(buffer, 1, nread, r->stream) < 0){
        goto fail;
      }
    }

    /* Close file, deallocate mimetype, return OK */
    fclose(fs);
    free(mimetype);
    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    fclose(fs);
    free(mimetype);
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
}

/**
 * Handle CGI request
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 -
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
Status  handle_cgi_request(Request *r) {
    FILE *pfs;
    char buffer[BUFSIZ];

    log("Handling CGI request (in)");

    /* Export CGI environment variables from request:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    if(setenv("QUERY_STRING", r->query, 1) == -1) debug("Can't set QUERY_STRING: %s", strerror(errno));
    if(setenv("REMOTE_ADDR", r->host, 1) == -1) debug("Can't set REMOTE_ADDR: %s", strerror(errno));
    if(setenv("REMOTE_PORT", r->port, 1) == -1) debug("Can't set REMOTE_PORT: %s", strerror(errno));
    if(setenv("REQUEST_METHOD", r->method, 1) == -1) debug("Can't set REQUEST_METHOD: %s", strerror(errno));
    if(setenv("REQUEST_URI", r->uri, 1) == -1) debug("Can't set REQUEST_URI: %s", strerror(errno));
    if(setenv("SCRIPT_FILENAME", r->path, 1) == -1) debug("Can't set SCRIPT_FILENAME: %s", strerror(errno));

    if(setenv("DOCUMENT_ROOT", RootPath, 1) == -1) debug("Can't set DOCUMENT_ROOT: %s", strerror(errno));
    if(setenv("SERVER_PORT", Port, 1) == -1) debug("Can't set SERVER_PORT: %s", strerror(errno));

    /* Export CGI environment variables from request headers */
    Header * h = r->headers;

    while(h->name){
      if(streq(h->name, "Host")) setenv("HTTP_HOST", h->data, 1);
      if(streq(h->name, "Accept")) setenv("HTTP_ACCEPT", h->data, 1);
      if(streq(h->name, "Accept-Language")) setenv("HTTP_ACCEPT_LANGUAGE", h->data, 1);
      if(streq(h->name, "Accept-Encoding")) setenv("HTTP_ACCEPT_ENCODING", h->data, 1);
      if(streq(h->name, "Connection")) setenv("HTTP_CONNECTION", h->data, 1);
      if(streq(h->name, "User-Agent")) setenv("HTTP_USER_AGENT", h->data, 1);
      h = h->next; // advance to the next header
    }

    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    if(!pfs){
      pclose(pfs);
      return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
    }

    /* Copy data from popen to socket */
    while(fgets(buffer, BUFSIZ, pfs)){
      fputs(buffer, r->stream);
    }

    /* Close popen, return OK */
    pclose(pfs);
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
Status  handle_error(Request *r, Status status) {
    const char *status_string = http_status_string(status);

    log("Handling error");

    /* Write HTTP Header */
    fprintf(r->stream, "HTTP/1.0 %s\r\n", status_string);
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");

    /* Write HTML Description of Error*/
    fprintf(r->stream, "<html><body>\n");
    fprintf(r->stream, "<link href=\"//maxcdn.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css\" rel=\"stylesheet\" id=\"bootstrap-css\"> <script src=\"//maxcdn.bootstrapcdn.com/bootstrap/4.1.1/js/bootstrap.min.js\"></script> <script src=\"//cdnjs.cloudflare.com/ajax/libs/jquery/3.2.1/jquery.min.js\"></script> <div class=\"d-flex justify-content-center align-items-center\" id=\"main\"> <h1 class=\"mr-3 pr-3 align-top border-right inline-block align-content-center\">404</h1> <div class=\"inline-block align-middle\"> <h2 class=\"font-weight-normal lead\" id=\"desc\">The page you requested was not found.</h2> </div> </div>");
    fprintf(r->stream, "</body></html>\n");

    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
