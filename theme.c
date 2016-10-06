/*
 * theme.c
 *
 *  Created on: 28 Sep 2016
 *      Author: billy
 */

#include "theme.h"
#include "meta.h"
#include "repo.h"
#include "common.h"
#include "config.h"

/************************************/


static int GetAndAddMetadata (const objType_t object_type, const char *id_s, const char *coll_name_s, const char * const link_s, apr_bucket_brigade *bb_p, rcComm_t *connection_p, apr_pool_t *pool_p);

static int GetAndAddMetadataForCollEntry (const collEnt_t *entry_p, apr_bucket_brigade *bb_p, const dav_resource *resource_p, apr_pool_t *pool_p);


static int PrintMetadata (apr_bucket_brigade *bb_p, IrodsMetadata **metadata_pp, int size, const char *link_s);

static int CompareIrodsMetadata (const void *v0_p, const void *v1_p);

static int DisplayIcons (const struct HtmlTheme *theme_p);

static const char *GetIconForCollEntry (const struct HtmlTheme * const theme_p, collEnt_t *coll_entry_p);

static const char *GetIcon (const struct HtmlTheme * const theme_p, const objType_t object_type, const char * const name_s);



/*************************************/


void InitHtmlTheme (struct HtmlTheme *theme_p)
{
  theme_p -> ht_head_s = NULL;
  theme_p -> ht_top_s = NULL;
  theme_p -> ht_bottom_s = NULL;
  theme_p -> ht_collection_icon_s = NULL;
  theme_p -> ht_object_icon_s = NULL;
  theme_p -> ht_parent_icon_s = NULL;
  theme_p -> ht_listing_class_s = NULL;
  theme_p -> ht_show_metadata = 0;
  theme_p -> ht_rest_api_s = NULL;

  theme_p -> ht_icons_map_p = NULL;
}


dav_error *DeliverThemedDirectory (const dav_resource *resource_p, ap_filter_t *output_p)
{
	struct dav_resource_private *davrods_resource_p = (struct dav_resource_private *) resource_p -> info;
	request_rec *req_p = davrods_resource_p -> r;
	apr_pool_t *pool_p = resource_p -> pool;

	collInp_t coll_inp = { { 0 } };
	collHandle_t coll_handle = { 0 };
	collEnt_t coll_entry;
	int status;

	strcpy(coll_inp.collName, davrods_resource_p->rods_path);

	// Open the collection
	status = rclOpenCollection (davrods_resource_p->rods_conn, davrods_resource_p->rods_path, LONG_METADATA_FG, &coll_handle);

	if (status < 0)
		{
			ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p,
					"rcOpenCollection failed: %d = %s", status,
					get_rods_error_msg(status));

			return dav_new_error (pool_p, HTTP_INTERNAL_SERVER_ERROR, 0, status, "Could not open a collection");
		}

	davrods_dir_conf_t *conf_p = davrods_resource_p->conf;
	struct HtmlTheme *theme_p = &(conf_p->theme);

	const char * const user_s = davrods_resource_p -> rods_conn -> clientUser.userName;

	// Make brigade.
	apr_bucket_brigade *bucket_brigade_p = apr_brigade_create (pool_p, output_p -> c -> bucket_alloc);
	apr_status_t apr_status = PrintAllHTMLBeforeListing (theme_p, davrods_resource_p -> relative_uri, user_s, conf_p -> rods_zone, req_p, bucket_brigade_p, pool_p);


	if (apr_status == APR_SUCCESS)
		{
			// Actually print the directory listing, one table row at a time.
			do
				{
					status = rclReadCollection (davrods_resource_p -> rods_conn, &coll_handle, &coll_entry);

					if (status >= 0)
						{
							const char *davrods_root_path_s = "";
							int success_code = PrintItem (theme_p, coll_entry.objType, coll_entry.dataId, coll_entry.dataName, coll_entry.collName, coll_entry.ownerName, coll_entry.modifyTime, coll_entry.dataSize, davrods_root_path_s, conf_p -> davrods_api_path_s, bucket_brigade_p, pool_p, resource_p -> info -> rods_conn);
						}
					else
						{
							if (status == CAT_NO_ROWS_FOUND)
								{
									// End of collection.
								}
							else
								{
									ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS,
											req_p,
											"rcReadCollection failed for collection <%s> with error <%s>",
											davrods_resource_p->rods_path, get_rods_error_msg(status));

									apr_brigade_destroy(bucket_brigade_p);

									return dav_new_error(pool_p, HTTP_INTERNAL_SERVER_ERROR,
											0, 0, "Could not read a collection entry from a collection.");
								}
						}
				}
			while (status >= 0);

		}		/* if (apr_status == APR_SUCCESS) */


	PrintAllHTMLAfterListing (theme_p, req_p, bucket_brigade_p, pool_p);

	CloseBucketsStream (bucket_brigade_p);

	if ((status = ap_pass_brigade(output_p, bucket_brigade_p)) != APR_SUCCESS)
		{
			apr_brigade_destroy (bucket_brigade_p);
			return dav_new_error(pool_p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
					"Could not write content to filter.");
		}
	apr_brigade_destroy(bucket_brigade_p);

	return NULL;
}


apr_status_t PrintAllHTMLAfterListing (struct HtmlTheme *theme_p, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p)
{
	apr_status_t apr_status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, "</tbody>\n</table>\n</main>\n");

	if (apr_status == APR_SUCCESS)
		{
			if (theme_p -> ht_bottom_s)
				{
					apr_status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, theme_p->ht_bottom_s);

					if (apr_status != APR_SUCCESS)
						{
							ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p, "Failed to add end table to bottom section \"%s\", %d", theme_p -> ht_bottom_s, apr_status);
						} /* if (apr_ret != APR_SUCCESS) */

				}		/* if (theme_p -> ht_bottom_s) */
		}

	apr_status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, "\n</body>\n</html>\n");

	return apr_status;
}


apr_status_t PrintAllHTMLBeforeListing (struct HtmlTheme *theme_p, const char * const relative_uri_s, const char * const user_s, const char * const zone_s, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p)
{
	apr_status_t apr_status;

	// Send start of HTML document.
	apr_brigade_printf(bucket_brigade_p, NULL, NULL,
			"<!DOCTYPE html>\n<html lang=\"en\">\n<head><title>Index of %s on %s</title>\n",
			ap_escape_html(pool_p, relative_uri_s),
			ap_escape_html(pool_p, zone_s));

	//    WHISPER("head \"%s\"", theme_p -> ht_head_s);
	//    WHISPER("top \"%s\"", theme_p -> ht_top_s);
	//    WHISPER("bottom \"%s\"", theme_p -> ht_bottom_s);
	//    WHISPER("coll \"%s\"", theme_p -> ht_collection_icon_s);
	//    WHISPER("obj \"%s\"", theme_p -> ht_object_icon_s);
	//    WHISPER("metadata \"%d\"", theme_p -> ht_show_metadata);

	/*
	 * If we have additional data for the <head> section, add it here.
	 */
	if (theme_p->ht_head_s)
		{
			apr_status = apr_brigade_puts(bucket_brigade_p, NULL, NULL, theme_p->ht_head_s);

			if (apr_status != APR_SUCCESS)
				{
					ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_SUCCESS,
							req_p,
							"Failed to add html to <head> section \"%s\"",
							theme_p->ht_head_s);

				} /* if (apr_ret != APR_SUCCESS) */

		} /* if (theme_p -> ht_head_s) */

	apr_brigade_puts(bucket_brigade_p, NULL, NULL,
			"<body>\n\n"
					"<!-- Warning: Do not parse this directory listing programmatically,\n"
					"              the format may change without notice!\n"
					"              If you want to script access to these WebDAV collections,\n"
					"              please use the PROPFIND method instead. -->\n\n");

	/*
	 * If we have additional data to go above the directory listing, add it here.
	 */
	if (theme_p -> ht_top_s)
		{
			apr_status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, theme_p -> ht_top_s);

			if (apr_status != APR_SUCCESS)
				{
					ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p, "Failed to add html to top section \"%s\"", theme_p -> ht_top_s);
				} /* if (apr_ret != APR_SUCCESS) */

		}		/* if (theme_p -> ht_top_s) */

	apr_brigade_printf(bucket_brigade_p, NULL, NULL,
			"<main>\n<h1>You are logged in as %s and browsing the index of %s on %s</h1>\n",
			user_s,
			ap_escape_html (pool_p, relative_uri_s),
			ap_escape_html (pool_p, zone_s));


	if (strcmp (relative_uri_s, "/"))
		{
			apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<p><a href=\"..\">");

			if (theme_p -> ht_parent_icon_s)
				{
					apr_brigade_printf (bucket_brigade_p, NULL, NULL, "<img src=\"%s\" alt=\"Browse to parent Collection\"/>", ap_escape_html (pool_p, theme_p -> ht_parent_icon_s));
				}
			else
				{
					apr_brigade_puts(bucket_brigade_p, NULL, NULL, "↖");
				}

			apr_brigade_puts(bucket_brigade_p, NULL, NULL, " Parent collection</a></p>\n");

		}		/* if (strcmp (davrods_resource_p->relative_uri, "/")) */

	/*
	 * Add the listing class
	 */
	apr_status = apr_brigade_printf (bucket_brigade_p, NULL, NULL, "<table class=\"%s\">\n<thead>\n<tr>", theme_p -> ht_listing_class_s ? theme_p -> ht_listing_class_s : "listing");
	if (apr_status != APR_SUCCESS)
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p, "Failed to add start of table listing, %d", apr_status);
		} /* if (apr_ret != APR_SUCCESS) */


	/*
	 * If we are going to display icons, add the column
	 */
	if (DisplayIcons (theme_p))
		{
			apr_status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<th class=\"icon\"></th>");

			if (apr_status != APR_SUCCESS)
				{
					ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p, "Failed to add table header for icons, %d", apr_status);
				} /* if (apr_ret != APR_SUCCESS) */

		}		/* if ((theme_p -> ht_collection_icon_s) || (theme_p -> ht_object_icon_s)) */

	apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<th class=\"name\">Name</th><th class=\"size\">Size</th><th class=\"owner\">Owner</th><th class=\"datestamp\">Last modified</th>");

	if (theme_p -> ht_show_metadata)
		{
			apr_status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<th class=\"metadata\">Properties</th>");

			if (apr_status != APR_SUCCESS)
				{
					ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p, "Failed to add table header for metadata, %d", apr_status);
				} /* if (apr_ret != APR_SUCCESS) */

		}		/* if (theme_p->ht_show_metadata) */


	apr_brigade_puts (bucket_brigade_p, NULL, NULL, "</tr>\n</thead>\n<tbody>\n");

	return apr_status;
}




int PrintItem (struct HtmlTheme *theme_p, const objType_t obj_type, const char *id_s, const char * const data_s, const char *collection_s, const char * const owner_name_s, const char *last_modified_time_s, const rodsLong_t size, const char *root_path_s, const char * const link_s, apr_bucket_brigade *bb_p, apr_pool_t *pool_p, rcComm_t *connection_p)
{
	int success_code = 0;
	const char *alt_s = NULL;
	const char *link_suffix_s = NULL;
	const char *name_s = NULL;

	apr_brigade_puts (bb_p, NULL, NULL, " <tr>");

	switch (obj_type)
		{
			case DATA_OBJ_T:
				name_s = data_s;
				alt_s = "iRods Data Object";
				break;

			case COLL_OBJ_T:
				name_s = get_basename (collection_s);
				alt_s = "iRods Collection";
				link_suffix_s = "/";
				break;

			default:
				break;
		}

	if (name_s && alt_s)
		{
			const char *icon_s = GetIcon (theme_p, obj_type, name_s);

			// Collection links need a trailing slash for the '..' links to work correctly.
			if (icon_s)
				{
					apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"icon\"><img src=\"%s\" alt=\"%s\"></td>", ap_escape_html (pool_p, icon_s), alt_s);
				}

			apr_brigade_printf(bb_p, NULL, NULL,
					"<td class=\"name\"><a href=\"%s%s%s\">%s%s</a></td>",
					root_path_s ? root_path_s : "",
					ap_escape_html(pool_p, ap_escape_uri (pool_p, name_s)),
					link_suffix_s ? link_suffix_s : "",
					ap_escape_html (pool_p, name_s),
					link_suffix_s ? link_suffix_s : "");

		}		/* if (name_s && alt_s) */


	// Print data object size.
	if (obj_type == DATA_OBJ_T)
		{
			char size_buf [5] = { 0 };

			// Fancy file size formatting.
			apr_strfsize (size, size_buf);

			if (size_buf [0])
				{
					apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"size\">%sB</td>", size_buf);
				}
			else
				{
					apr_brigade_printf(bb_p, NULL, NULL, "<td class=\"size\">%luB</td>", size);
				}
		}
	else
		{
			apr_brigade_puts(bb_p, NULL, NULL, "<td class=\"size\"></td>");
		}

	// Print owner.
	apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"owner\">%s</td>", ap_escape_html (pool_p, owner_name_s));

	// Print modified-date string.
	uint64_t timestamp = atoll (last_modified_time_s);
	apr_time_t apr_time = 0;
	apr_time_exp_t exploded = { 0 };
	char date_str [64] = { 0 };

	apr_time_ansi_put (&apr_time, timestamp);
	apr_time_exp_lt (&exploded, apr_time);

	size_t ret_size;

	if (!apr_strftime (date_str, &ret_size, sizeof (date_str), "%Y-%m-%d %H:%M", &exploded))
		{
			apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"datestamp\">%s</td>", ap_escape_html (pool_p, date_str));
		}
	else
		{
			// Fallback, just in case.
			static_assert(sizeof(date_str) >= APR_RFC822_DATE_LEN, "Size of date_str buffer too low for RFC822 date");

			int status = apr_rfc822_date (date_str, timestamp * 1000 * 1000);

			apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"datestamp\">%s</td>", ap_escape_html (pool_p, status >= 0 ? date_str : "Thu, 01 Jan 1970 00:00:00 GMT"));
		}

	if (theme_p -> ht_show_metadata)
		{
			if (GetAndAddMetadata (obj_type, id_s, collection_s, link_s, bb_p, connection_p, pool_p) != 0)
				{

				}
		}

	apr_brigade_puts (bb_p, NULL, NULL, "</tr>\n");

	return success_code;
}


static int GetAndAddMetadataForCollEntry (const collEnt_t *entry_p, apr_bucket_brigade *bb_p, const dav_resource *resource_p, apr_pool_t *pool_p)
{
	dav_resource_private *res_p = (dav_resource_private *) (resource_p -> info);
	return GetAndAddMetadata (entry_p -> objType, entry_p -> dataId, entry_p -> collName, res_p -> conf -> davrods_api_path_s, bb_p, res_p -> rods_conn, pool_p);
}


static int GetAndAddMetadata (const objType_t object_type, const char *id_s, const char *coll_name_s, const char * const link_s, apr_bucket_brigade *bb_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	int status = -1;
	apr_array_header_t *metadata_array_p = GetMetadata (connection_p, object_type, id_s, coll_name_s, pool_p);

	apr_brigade_puts (bb_p, NULL, NULL, "<td class=\"metadata\">");

	if (metadata_array_p)
		{
			/*
			 * Sort the metadata keys into alphabetical order
			 */
			if (!apr_is_empty_array (metadata_array_p))
				{
					IrodsMetadata **metadata_pp = (IrodsMetadata **) apr_palloc (pool_p, (metadata_array_p -> nelts) * sizeof (IrodsMetadata **));

					if (metadata_pp)
						{
							int i;
							IrodsMetadata **md_pp = metadata_pp;

							for (i = 0; i < metadata_array_p -> nelts; ++ i, ++ md_pp)
								{
									*md_pp = ((IrodsMetadata **) metadata_array_p -> elts) [i];
								}

							qsort (metadata_pp, metadata_array_p -> nelts, sizeof (IrodsMetadata **), CompareIrodsMetadata);

							md_pp = metadata_pp;

							status = PrintMetadata (bb_p, metadata_pp, metadata_array_p -> nelts, link_s);

						}		/* if (metadata_pp) */

				}		/* if (!apr_is_empty_array (metadata_array_p)) */

		}		/* if (metadata_array_p) */

	apr_brigade_puts(bb_p, NULL, NULL, "</td>");

	return status;
}


static int PrintMetadata (apr_bucket_brigade *bb_p, IrodsMetadata **metadata_pp, int size, const char *link_s)
{
	int status = 0;
	apr_status_t apr_stat = apr_brigade_puts (bb_p, NULL, NULL, "<ul class=\"metadata\">");

	if (apr_stat == 0)
		{
			for ( ; size > 0; -- size, ++ metadata_pp)
				{
					const IrodsMetadata *metadata_p = *metadata_pp;

					apr_brigade_puts (bb_p, NULL, NULL, "<li>");

					if (link_s)
						{
							apr_brigade_printf (bb_p, NULL, NULL, "<a href=\"%s?key=%s&value=%s\">", link_s, metadata_p -> im_key_s, metadata_p -> im_value_s);
						}

					apr_brigade_printf (bb_p, NULL, NULL, "<span class=\"key\">%s</span>: <span class=\"value\">%s</span>", metadata_p -> im_key_s, metadata_p -> im_value_s);

					if (metadata_p -> im_units_s)
						{
							apr_brigade_printf (bb_p, NULL, NULL, "<span class=\"units\">%s</span>", metadata_p -> im_units_s);
						}


					if (link_s)
						{
							apr_brigade_puts (bb_p, NULL, NULL, "</a>");
						}


					apr_brigade_puts (bb_p, NULL, NULL, "</li>");
				}

			apr_brigade_puts (bb_p, NULL, NULL, "</ul>");

		}

	return status;
}


static int CompareIrodsMetadata (const void *v0_p, const void *v1_p)
{
	int res = 0;
	IrodsMetadata *md0_p = * ((IrodsMetadata **) v0_p);
	IrodsMetadata *md1_p = * ((IrodsMetadata **) v1_p);

	res = strcasecmp (md0_p -> im_key_s, md1_p -> im_key_s);

	if (res == 0)
		{
			res = strcasecmp (md0_p -> im_value_s, md1_p -> im_value_s);
		}

	return res;
}


static int DisplayIcons (const struct HtmlTheme *theme_p)
{
	return ((theme_p -> ht_collection_icon_s) || (theme_p -> ht_object_icon_s) || (theme_p -> ht_icons_map_p)) ? 1 : 0;
}


static const char *GetIconForCollEntry (const struct HtmlTheme * const theme_p, collEnt_t *coll_entry_p)
{
	const char *name_s = NULL;

	switch (coll_entry_p -> objType)
		{
			case DATA_OBJ_T:
				name_s = coll_entry_p -> dataName;
				break;

			case COLL_OBJ_T:
				name_s = coll_entry_p -> collName;
				break;

			default:
				break;
		}

	if (name_s)
		{
			name_s = GetIcon (theme_p, coll_entry_p -> objType, name_s);
		}

	return name_s;
}


static const char *GetIcon (const struct HtmlTheme * const theme_p, const objType_t object_type, const char * const name_s)
{
	const char *icon_s = NULL;

	if (theme_p -> ht_icons_map_p)
		{
			const char *key_s = NULL;

			switch (object_type)
				{
					case DATA_OBJ_T:
						key_s = strrchr (name_s, '.');
						break;

					case COLL_OBJ_T:
						key_s = get_basename (name_s);
						break;

					default:
						break;
				}

			if (key_s)
				{
					icon_s = apr_table_get (theme_p -> ht_icons_map_p, key_s);
				}
		}

	if (!icon_s)
		{
			switch (object_type)
				{
					case DATA_OBJ_T:
						icon_s = theme_p -> ht_object_icon_s;
						break;

					case COLL_OBJ_T:
						icon_s = theme_p -> ht_collection_icon_s;
						break;

					default:
						break;
				}
		}

	return icon_s;
}
