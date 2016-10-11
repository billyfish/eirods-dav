/*
 * rest.h
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#ifndef REST_H_
#define REST_H_


#include "httpd.h"


#define REST_METADATA_PATH_S ("metadata")


int DavrodsRestHandler (request_rec *req_p);


#endif /* REST_H_ */
