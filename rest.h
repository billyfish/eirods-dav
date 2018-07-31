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
/*
 * rest.h
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#ifndef REST_H_
#define REST_H_

#include "config.h"

#include "irods/rcConnect.h"

#include "httpd.h"
#include "listing.h"
#include "meta.h"
#include "output_format.h"


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
REST_PREFIX const char REST_METADATA_MATCHING_KEYS_S [] REST_VAL ("metadata/keys");
REST_PREFIX const char REST_METADATA_MATCHING_VALUES_S [] REST_VAL ("metadata/values");

REST_PREFIX const char REST_GET_INFO_S [] REST_VAL ("general/info");
REST_PREFIX const char REST_LIST_S [] REST_VAL ("general/list");


REST_PREFIX const char VIEW_SEARCH_S [] REST_VAL ("search");
REST_PREFIX const char VIEW_LIST_S [] REST_VAL ("list");


int EIRodsDavAPIHandler (request_rec *req_p);

const char *GetMinorId (const char *id_s);


rcComm_t *GetIRODSConnectionForAPI (request_rec *req_p, davrods_dir_conf_t *config_p);



#endif /* REST_H_ */
