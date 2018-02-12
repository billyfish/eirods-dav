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


enum MetadataDisplay
{
	MD_UNSET,
	MD_NONE,
	MD_FULL,
	MD_ON_DEMAND,
	MD_NUM_ENTRIES
};


struct HtmlTheme
{
	const char *ht_head_s;

	const char *ht_top_s;

	const char *ht_bottom_s;

	const char *ht_collection_icon_s;

	const char *ht_object_icon_s;

	const char *ht_listing_class_s;

	enum MetadataDisplay ht_show_metadata_flag;

	int ht_metadata_editable_flag;

	const char *ht_add_metadata_icon_s;

	const char *ht_edit_metadata_icon_s;

	const char *ht_delete_metadata_icon_s;

	const char *ht_download_metadata_icon_s;

	const char *ht_ok_icon_s;

	const char *ht_cancel_icon_s;

	const char *ht_rest_api_s;

	char **ht_resources_ss;

	int ht_show_resource_flag;

	int ht_show_ids_flag;

	int ht_add_search_form_flag;

	int ht_active_flag;

	const char *ht_login_url_s;

	const char *ht_logout_url_s;

	const char *ht_user_icon_s;

	apr_table_t *ht_icons_map_p;


	const char *ht_name_heading_s;

	const char *ht_size_heading_s;

	const char *ht_owner_heading_s;

	const char *ht_date_heading_s;

	const char *ht_properties_heading_s;
};



#ifdef ALLOCATE_THEME_CONSTANTS
#define THEME_PREFIX
#define THEME_VAL(x) = x
#else
#define THEME_PREFIX extern
#define THEME_VAL(x)
#endif


THEME_PREFIX const char THEME_HIDE_COLUMN_S [] THEME_VAL ("!");


/* forward declaration */
struct davrods_dir_conf;


#ifdef __cplusplus
extern "C"
{
#endif



struct HtmlTheme * AllocateHtmlTheme (apr_pool_t *pool_p);

dav_error *DeliverThemedDirectory (const dav_resource *resource_p, ap_filter_t *output_p);

apr_status_t PrintItem (struct HtmlTheme *theme_p, const IRodsObject *irods_obj_p, const IRodsConfig *config_p, unsigned int row_index, apr_bucket_brigade *bb_p, apr_pool_t *pool_p, rcComm_t *connection_p, request_rec *req_p);

apr_status_t PrintAllHTMLBeforeListing (struct dav_resource_private *davrods_resource_p, const char * const page_title_s, const char * const user_s, davrods_dir_conf_t *conf_p, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);

apr_status_t PrintAllHTMLAfterListing (struct HtmlTheme *theme_p, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);

void MergeThemeConfigs (davrods_dir_conf_t *conf_p, davrods_dir_conf_t *parent_p, davrods_dir_conf_t *child_p, apr_pool_t *pool_p);


char *GetLocationPath (request_rec *req_p, davrods_dir_conf_t *conf_p, apr_pool_t *pool_p, const char *needle_s);

char *GetDavrodsAPIPath (struct dav_resource_private *davrods_resource_p, davrods_dir_conf_t *conf_p, request_rec *req_p);


#ifdef __cplusplus
}
#endif

#endif /* THEME_H_ */
