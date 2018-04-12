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
/* listing.h
 *
 *  Created on: 7 Oct 2016
 *      Author: billy
 */

#ifndef LISTING_H_
#define LISTING_H_

typedef unsigned int uint;
#include <sys/types.h>

#include "irods/rodsType.h"
#include "irods/miscUtil.h"

#include "apr_pools.h"
#include "apr_buckets.h"

#include "config.h"

/* Forward declaration */
struct HtmlTheme;


typedef struct IRodsObject
{
	objType_t io_obj_type;
	const char *io_id_s;
	const char *io_data_s;
	const char *io_collection_s;
	const char *io_owner_name_s;
	const char *io_resource_s;
	const char *io_last_modified_time_s;
	rodsLong_t io_size;
} IRodsObject;


typedef struct IRodsConfig
{
	const char *ic_exposed_root_s;

	const char *ic_root_path_s;

	const char *ic_metadata_root_link_s;
} IRodsConfig;


#ifdef __cplusplus
extern "C"
{
#endif


void InitIRodsObject (IRodsObject *obj_p);


apr_status_t SetIRodsConfig (IRodsConfig *config_p, const char *exposed_root_s, const char *root_path_s, const char *metadata_root_link_s);


apr_status_t SetIRodsObject (IRodsObject *obj_p, const objType_t obj_type, const char *id_s, const char *data_s, const char *collection_s, const char *owner_name_s, const char *resource_s, const char *last_modified_time_s, const rodsLong_t size);


apr_status_t SetIRodsObjectFromCollEntry (IRodsObject *obj_p, const collEnt_t *coll_entry_p, rcComm_t *connection_p, apr_pool_t *pool_p);


apr_status_t SetIRodsObjectFromIdString (IRodsObject *obj_p, const char *id_s, rcComm_t *connection_p, apr_pool_t *pool_p);



/**
 * Get the user-friendly name for an IRodsObject.
 *
 * @param obj_p The IRodsObject to get the name for.
 * @return The name or <code>NULL</code> upon error.
 */
const char *GetIRodsObjectDisplayName (const IRodsObject *obj_p);


/**
 * Get the path to use for the icon to use for a given IRodsObject.
 *
 * @param irods_obj_p The IRodsObject to get the icon for
 * @param theme_p The theme configuration to use for determining the icon.
 * @return The path to the icon to use or <code>NULL</code> if one could not be found.
 */
const char *GetIRodsObjectIcon (const IRodsObject *irods_obj_p, const struct HtmlTheme * const theme_p);

/**
 * Get the text to use for the alt tag of the image used for a given IRodsObject.
 *
 * @param irods_obj_p The IRodsObject to get the alt tag for.
 * @return The value to use for the alt tag or <code>NULL</code> if one could not be found.
 */
const char *GetIRodsObjectAltText (const IRodsObject *irods_obj_p);


/**
 * Get the relative link to access an IRodsObject.
 *
 * @param irods_obj_p The IRodsObject to get the link for.
 * @param config_p The configuration for the DavRODS server.
 * @param pool_p A memory pool to use for allocations.
 * @return The relative link or <code>NULL</code> upon error.
 */
char *GetIRodsObjectRelativeLink (const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_pool_t *pool_p);


/**
 * Get the size of an IRodsObject as a user-friendly string.
 *
 * @param irods_obj_p The IRodsObject to get the size of.
 * @param pool_p The APR pool to allocate the memory for the string.
 * @return The string for the size or <code>NULL</code> upon error.
 */
char *GetIRodsObjectSizeAsString (const IRodsObject *irods_obj_p, apr_pool_t *pool_p);


/**
 * Get the time that an IRodsObject was last modified as a user-friendly string.
 *
 * @param irods_obj_p The IRodsObject to get the time for.
 * @param pool_p The APR pool to allocate the memory for the string.
 * @return The string for the time or <code>NULL</code> upon error.
 */
char *GetIRodsObjectLastModifiedTime (const  IRodsObject *irods_obj_p, apr_pool_t *pool_p);


apr_status_t GetAndPrintMetadataForIRodsObject (const IRodsObject *irods_obj_p, const char * const link_s, const char *zone_s, const struct HtmlTheme * const theme_p, apr_bucket_brigade *bb_p, rcComm_t *connection_p, request_rec *req_p, apr_pool_t *pool_p);


apr_status_t GetAndPrintMetadataRestLinkForIRodsObject (const IRodsObject *irods_obj_p, const char * const apt_root_link_s, const char *zone_s, const struct HtmlTheme * const theme_p, apr_bucket_brigade *bb_p, rcComm_t *connection_p, apr_pool_t *pool_p);


char *GetId (char *value_s, objType_t *type_p, apr_pool_t *pool_p);


rodsObjStat_t *GetObjectStat (const char * const path_s, rcComm_t *connection_p, apr_pool_t *pool_p);


char *GetIRodsObjectFullPath (const IRodsObject *obj_p, apr_pool_t *pool_p);

#ifdef __cplusplus
}
#endif


#endif /* LISTING_H_ */
