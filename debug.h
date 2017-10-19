/*
** Copyright 2014-2017 The Earlham Institute
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
/*
 * debug.h
 *
 *  Created on: 17 Oct 2017
 *      Author: billy
 */

#ifndef DEBUG_H_
#define DEBUG_H_


#include "httpd.h"

#include "apr_hash.h"
#include "apr_tables.h"


void DebugRequest (request_rec *req_p);

void DebugAPRTable (const apr_table_t *table_p, const char * const message_s, request_rec *req_p);


void DebugAPRHash (const apr_hash_t  *hash_p, const char * const message_s, request_rec *req_p);


#endif /* DEBUG_H_ */
