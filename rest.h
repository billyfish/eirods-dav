/*
 * rest.h
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#ifndef REST_H_
#define REST_H_


#include "httpd.h"


#ifdef ALLOCATE_REST_CONSTANTS
#define REST_PREFIX
#define REST_VAL(x) = x
#else
#define REST_PREFIX extern
#define REST_VAL(x)
#endif


REST_PREFIX const char REST_METADATA_SEARCH_S [] REST_VAL ("metadata/search");
REST_PREFIX const char REST_METADATA_EDIT_S [] REST_VAL ("metadata/edit");
REST_PREFIX const char REST_METADATA_GET_S [] REST_VAL ("metadata/get");
REST_PREFIX const char REST_METADATA_ADD_S [] REST_VAL ("metadata/add");
REST_PREFIX const char REST_METADATA_DELETE_S [] REST_VAL ("metadata/delete");




int DavrodsRestHandler (request_rec *req_p);

const char *GetMinorId (const char *id_s);

#endif /* REST_H_ */
