/*
 * httpresp_const.h - constants of httpresp.c: the states of a chunked body.
 *
 * Author: Thomas Dzubin
 */
#ifndef HTTPRESP_CONST_H
#define HTTPRESP_CONST_H

#define CHUNK_SIZE_LINE   0     /* reading the hex size line */
#define CHUNK_DATA        1     /* inside a chunk */
#define CHUNK_DATA_END    2     /* the CR LF after a chunk */
#define CHUNK_TRAILERS    3     /* after the last chunk: trailer lines until an empty one */

#endif /* HTTPRESP_CONST_H */
