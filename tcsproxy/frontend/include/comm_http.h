/*
 *  comm_http.h: headers for HTTP-specific communication functions.
 */

#ifndef _COMM_HTTP_H
#define _COMM_HTTP_H

/*
 *  Functions in comm_http.c for reading from client
 */

#define READ_ALL_HEADERS  ((int)(-1))

#define PERF_HTTP_HEADER_BLOCKSIZE  1024 /* block size for reading headers */

int readline_or_timeout(Request *hp, int nbytes, char *optional_buffer);
HTTP_Status parse_status_and_headers(Request *hp);

/*
 *  Functions in httpsupport.c for sending to client connection
 */

void send_content(const char *content, int len, const char *mimetype, int sock);
void http_error_return(Request *h, HTTP_Status status);
void tunnel(Request *r);
void complete_bypass(Request *r);

#endif /* ifndef _COMM_HTTP_H */
