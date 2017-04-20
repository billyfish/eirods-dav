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
#include "config.h"

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

	int ht_active_flag;

	apr_table_t *ht_icons_map_p;
};


/* forward declaration */
struct davrods_dir_conf;


#ifdef __cplusplus
extern "C"
{
#endif



struct HtmlTheme * AllocateHtmlTheme (apr_pool_t *pool_p);

dav_error *DeliverThemedDirectory (const dav_resource *resource_p, ap_filter_t *output_p);

apr_status_t PrintItem (struct HtmlTheme *theme_p, const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_bucket_brigade *bb_p, apr_pool_t *pool_p, rcComm_t *connection_p, request_rec *req_p);

apr_status_t PrintAllHTMLBeforeListing (struct dav_resource_private *davrods_resource_p, const char * const page_title_s, const char * const user_s, davrods_dir_conf_t *conf_p, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);

apr_status_t PrintAllHTMLAfterListing (struct HtmlTheme *theme_p, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);

void MergeThemeConfigs (davrods_dir_conf_t *conf_p, davrods_dir_conf_t *parent_p, davrods_dir_conf_t *child_p, apr_pool_t *pool_p);


#ifdef __cplusplus
}
#endif

#endif /* THEME_H_ */
