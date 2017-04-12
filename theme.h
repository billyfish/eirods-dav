/*
 * theme.h
 *
 *  Created on: 28 Sep 2016
 *      Author: billy
 */

#ifndef THEME_H_
#define THEME_H_

#include "mod_dav.h"
#include "apr_buckets.h"
#include "apr_tables.h"

#include "irods/rodsType.h"
#include "irods/rodsConnect.h"


#include "listing.h"


struct HtmlTheme
{
	const char *ht_head_s;

	const char *ht_top_s;

	const char *ht_bottom_s;

	const char *ht_collection_icon_s;

	const char *ht_object_icon_s;

	const char *ht_parent_icon_s;

	const char *ht_listing_class_s;

	int ht_show_metadata_flag;

	const char *ht_rest_api_s;

	int ht_show_ids_flag;

	int ht_add_search_form_flag;

	apr_table_t *ht_icons_map_p;
};



#ifdef __cplusplus
extern "C"
{
#endif



void InitHtmlTheme (struct HtmlTheme *theme_p);

dav_error *DeliverThemedDirectory (const dav_resource *resource_p, ap_filter_t *output_p);

apr_status_t PrintItem (struct HtmlTheme *theme_p, const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_bucket_brigade *bb_p, apr_pool_t *pool_p, rcComm_t *connection_p, request_rec *req_p);

apr_status_t PrintAllHTMLBeforeListing (struct HtmlTheme *theme_p, const char * const relative_uri_s, const char * const user_s, const char * const zone_s, const char * const api_path_s, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);

apr_status_t PrintAllHTMLAfterListing (struct HtmlTheme *theme_p, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);


#ifdef __cplusplus
}
#endif

#endif /* THEME_H_ */
