/*
 * curl_tool.h
 *
 *  Created on: 13 Apr 2018
 *      Author: billy
 */

#ifndef CURL_UTIL_H_
#define CURL_UTIL_H_


/*
** Copyright 2014-2016 The Earlham Institute
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/**
 * @file
 * @brief
 */

#ifndef GRASSROOTS_CURL_TOOLS_H
#define GRASSROOTS_CURL_TOOLS_H

#include <stdio.h>

#include <stdbool.h>

#include "httpd.h"

#include <curl/curl.h>

#include "apr_pools.h"
#include "apr_buckets.h"


/**
 * @brief A tool for making http(s) requests and responses.
 *
 * A datatype that uses CURL to perform http(s) requests
 * and responses.
 * @ingroup network_group
 */
typedef struct CurlUtil
{
	/** @private */
	CURL *ct_curl_p;

	/** @private */
	apr_pool_t *ct_pool_p;

	apr_bucket_brigade *ct_buffer_p;


	request_rec *ct_req_p;

	/** @private */
	char *ct_buffer_s;

	/** @private */
	struct curl_httppost *ct_form_p;

	/** @private */
	struct curl_httppost *ct_last_field_p;

	/** @private */
	struct curl_slist *ct_headers_list_p;
} CurlUtil;




#ifdef __cplusplus
	extern "C" {
#endif


/**
 * Allocate a CurlUtil.
 *
 * @param mode The CurlMode that this CurlUtil will use.
 * @return A newly-allocated CurlUtil or <code>NULL</code> upon error.
 * @memberof CurlUtil
 * @see FreeCurlUtil
 */
CurlUtil *AllocateCurlUtil (request_rec *req_p, apr_pool_t *pool_p);



/**
 * Free a CurlUtil
 *
 * @param curl_p The CurlUtil to free.
 * @memberof CurlUtil
 */
void FreeCurlUtil (CurlUtil *curl_p);



bool ResetCurlUtil (CurlUtil *tool_p);


char *SimpleCallGetRequest (request_rec *req_p, apr_pool_t *pool_p, const char *uri_s);


char *CallGetRequest (CurlUtil *tool_p, const char *uri_s);



/**
 * Get the URL encoded version of a string.
 *
 * @param tool_p The CurlUtil to use to generate the URL encoded string.
 * @param src_s The string to encode.
 * @return The URL encoded version of the input string. This must be freed with
 * FreeURLEscapedString.
 * @see GetURLEscapedString
 * @memberof CurlUtil
 */
char *GetURLEscapedString (CurlUtil *tool_p, const char *src_s);


/**
 * Free a previously allocated string created by GetURLEscapedString.
 *
 * @param value_s The string to free. This must have been created by GetURLEscapedString.
 * @see GetURLEscapedString
 * @memberof CurlUtil
 */
void FreeURLEscapedString (char *value_s);


#ifdef __cplusplus
}
#endif


#endif		/* #ifndef GRASSROOTS_CURL_TOOLS_H */


#endif /* CURL_UTIL_H_ */
