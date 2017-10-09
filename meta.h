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
 * meta.h
 *
 *  Created on: 27 Sep 2016
 *      Author: billy
 */

#ifndef META_H_
#define META_H_


#include "mod_dav.h"
#include "apr_pools.h"
#include "apr_tables.h"
#include "apr_buckets.h"

#include "irods/rodsConnect.h"
#include "irods/rodsGenQuery.h"
#include "irods/miscUtil.h"

#include "config.h"
#include "rest.h"

typedef struct IrodsMetadata
{
	char *im_key_s;
	char *im_value_s;
	char *im_units_s;
} IrodsMetadata;


typedef enum SearchOperator
{
	SO_EQUALS,
	SO_LIKE,
	SO_NUM_OPERATORS
} SearchOperator;


#ifdef __cplusplus
extern "C"
{
#endif




apr_array_header_t *GetMetadataForCollEntry (const dav_resource *resource_p, const collEnt_t *entry_p, const char *zone_s);

apr_array_header_t *GetMetadata (rcComm_t *irods_connection_p, const objType_t object_type, const char *id_s, const char *coll_name_s, const char *zone_s, apr_pool_t *pool_p);


int printGenQI( genQueryInp_t *genQueryInp );


IrodsMetadata *AllocateIrodsMetadata (const char * const key_s, const char * const value_s, const char * const units_s, apr_pool_t *pool_p);


void SortIRodsMetadataArray (apr_array_header_t *metadata_array_p, int (*compare_fn) (const void *v0_p, const void *v1_p));


apr_status_t PrintMetadata (const char *id_s, const apr_array_header_t *metadata_list_p, struct HtmlTheme *theme_p, apr_bucket_brigade *bb_p, const char *metadata_search_link_s, apr_pool_t *pool_p);


char *DoMetadataSearch (const char * const key_s, const char *value_s, const SearchOperator op, const char * const username_s, apr_pool_t *pool_p, rcComm_t *connection_p, struct apr_bucket_alloc_t *bucket_allocator_p, davrods_dir_conf_t *conf_p, request_rec *req_p, const char *davrods_path_s);


genQueryOut_t *RunQuery (rcComm_t *connection_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, const SearchOperator *where_ops_p, size_t num_where_columns, const int options, apr_pool_t *pool_p);

apr_array_header_t *GetAllDataObjectMetadataKeys (apr_pool_t *pool_p, rcComm_t *connection_p);

apr_status_t GetSearchOperatorFromString (const char *op_s, SearchOperator *op_p);


apr_table_t *GetAllDataObjectMetadataValuesForKey (apr_pool_t *pool_p, rcComm_t *connection_p, const char *key_s);

char *GetParentCollectionId (const char *child_id_s, const objType_t object_type, const char *zone_s, rcComm_t *irods_connection_p, apr_pool_t *pool_p);

apr_status_t GetMetadataTableForId (char *combined_id_s, davrods_dir_conf_t *config_p, rcComm_t *connection_p, request_rec *req_p, apr_pool_t *pool_p, apr_bucket_brigade *bucket_brigade_p, OutputFormat format);


#ifdef __cplusplus
}
#endif


#endif /* META_H_ */
