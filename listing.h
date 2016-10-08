/*
 * listing.h
 *
 *  Created on: 7 Oct 2016
 *      Author: billy
 */

#ifndef LISTING_H_
#define LISTING_H_

#include "rodsType.h"
#include "miscUtil.h"

#include "apr_pools.h"
#include "apr_buckets.h"

/* Forward declaration */
struct HtmlTheme;


typedef struct IRodsObject
{
	objType_t io_obj_type;
	const char *io_id_s;
	const char *io_data_s;
	const char *io_collection_s;
	const char *io_owner_name_s;
	const char *io_last_modified_time_s;
	rodsLong_t io_size;
} IRodsObject;


typedef struct IRodsConfig
{
	const char *ic_exposed_root_s;

	const char *ic_root_path_s;

	const char *ic_metadata_root_link_s;
} IRodsConfig;



int SetIRodsObject (IRodsObject *obj_p, const objType_t io_obj_type, const char *io_id_s, const char *io_data_s, const char *io_collection_s, const char *io_owner_name_s, const char *io_last_modified_time_s, const rodsLong_t size);

int SetIRodsObjectFromCollEntry (IRodsObject *obj_p, const collEnt_t *coll_entry_p);

const char *GetIRodsObjectDisplayName (const IRodsObject *obj_p);

const char *GetIRodsObjectIcon (const IRodsObject *irods_obj_p, const struct HtmlTheme * const theme_p);

const char *GetIRodsObjectAltText (const IRodsObject *irods_obj_p);

char *GetIRodsObjectRelativeLink (const IRodsObject *irods_obj_p, const char *uri_root_s, const char *exposed_root_s, apr_pool_t *pool_p);

char *GetIRodsObjectSizeAsString (const IRodsObject *irods_obj_p, apr_pool_t *pool_p);

char *GetIRodsObjectLastModifiedTime (const  IRodsObject *irods_obj_p, apr_pool_t *pool_p);


int GetAndAddMetadataForIRodsObject (const IRodsObject *irods_obj_p, const char * const link_s, apr_bucket_brigade *bb_p, rcComm_t *connection_p, apr_pool_t *pool_p);

#endif /* LISTING_H_ */
