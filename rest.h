/*
 * rest.h
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#ifndef REST_H_
#define REST_H_


#include "httpd.h"

#include "rodsConnect.h"



int DavrodsRestHandler (request_rec *req_p);


#endif /* REST_H_ */
