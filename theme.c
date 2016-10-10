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
#include "auth.h"
#include "rest.h"

#include "listing.h"

/************************************/

static int DisplayIcons (const struct HtmlTheme *theme_p);

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
  theme_p -> ht_show_metadata_flag = 0;
  theme_p -> ht_rest_api_s = NULL;

  theme_p -> ht_show_ids_flag = 0;
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
			const char *davrods_root_path_s = davrods_resource_p -> root_dir;

			const char *exposed_root_s = GetRodsExposedPath (req_p);

			// Actually print the directory listing, one table row at a time.
			do
				{
					status = rclReadCollection (davrods_resource_p -> rods_conn, &coll_handle, &coll_entry);

					if (status >= 0)
						{
							char *metadata_link_s = apr_pstrcat (pool_p, davrods_resource_p -> root_dir, conf_p -> davrods_api_path_s, REST_METADATA_PATH_S, NULL);
							IRodsObject irods_obj;


							if (SetIRodsObjectFromCollEntry (&irods_obj, &coll_entry, davrods_resource_p -> rods_conn, pool_p))
								{
									int success_code = PrintItem (theme_p, &irods_obj, davrods_root_path_s, exposed_root_s, metadata_link_s, bucket_brigade_p, pool_p, resource_p -> info -> rods_conn);
								}

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



	apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<th class=\"name\">Name</th>");

//	if (theme_p -> ht_show_ids_flag)
//		{
//			apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<th class=\"id\">Id</th>");
//		}

	apr_brigade_puts (bucket_brigade_p, NULL, NULL, "<th class=\"size\">Size</th><th class=\"owner\">Owner</th><th class=\"datestamp\">Last modified</th>");

	if (theme_p -> ht_show_metadata_flag)
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


int PrintItem (struct HtmlTheme *theme_p, const IRodsObject *irods_obj_p, const char *root_path_s, const char *exposed_root_s, const char * const metadata_root_link_s, apr_bucket_brigade *bb_p, apr_pool_t *pool_p, rcComm_t *connection_p)
{
	int success_code = 0;
	const char *link_suffix_s = irods_obj_p -> io_obj_type == COLL_OBJ_T ? "/" : NULL;
	const char *name_s = GetIRodsObjectDisplayName (irods_obj_p);
	char *timestamp_s = GetIRodsObjectLastModifiedTime (irods_obj_p, pool_p);
	char *size_s = GetIRodsObjectSizeAsString (irods_obj_p, pool_p);

        if (theme_p -> ht_show_ids_flag) {
          apr_brigade_printf (bb_p, NULL, NULL, "<tr class=\"id\" id=\"%s\">", irods_obj_p -> io_id_s);
        }
        else {
          apr_brigade_puts (bb_p, NULL, NULL, " <tr>");
        }

	if (name_s)
		{
			const char *icon_s = GetIRodsObjectIcon (irods_obj_p, theme_p);
			const char *alt_s = GetIRodsObjectAltText (irods_obj_p);
			char *relative_link_s = GetIRodsObjectRelativeLink (irods_obj_p, root_path_s, exposed_root_s, pool_p);

			// Collection links need a trailing slash for the '..' links to work correctly.
			if (icon_s)
				{
					apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"icon\"><img src=\"%s\" alt=\"%s\"></td>", ap_escape_html (pool_p, icon_s), alt_s);
				}

			apr_brigade_printf(bb_p, NULL, NULL,
					"<td class=\"name\"><a href=\"%s\">%s%s</a></td>",
					relative_link_s,
					ap_escape_html (pool_p, name_s),
					link_suffix_s ? link_suffix_s : "");

		}		/* if (name_s && alt_s) */



//	if (theme_p -> ht_show_ids_flag)
//		{
//			apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"id\">%s</td>", irods_obj_p -> io_id_s);
//		}


	// Print data object size.
	apr_brigade_puts (bb_p, NULL, NULL, "<td class=\"size\">");
	if (size_s)
		{
			apr_brigade_printf (bb_p, NULL, NULL, "%sB", size_s);
		}
	else if (irods_obj_p -> io_obj_type == DATA_OBJ_T)
		{
			apr_brigade_printf(bb_p, NULL, NULL, "%luB", irods_obj_p -> io_size);
		}
	apr_brigade_puts (bb_p, NULL, NULL, "</td>");

	// Print owner
	apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"owner\">%s</td>", ap_escape_html (pool_p, irods_obj_p -> io_owner_name_s));

	if (timestamp_s)
		{
			apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"time\">%s</td>", timestamp_s);
		}
	else
		{
			apr_brigade_puts (bb_p, NULL, NULL, "<td class=\"time\"></td>");
		}


	if (theme_p -> ht_show_metadata_flag)
		{
			if (GetAndAddMetadataForIRodsObject (irods_obj_p, metadata_root_link_s, bb_p, connection_p, pool_p) != 0)
				{

				}
		}

	apr_brigade_puts (bb_p, NULL, NULL, "</tr>\n");

	return success_code;
}



static int DisplayIcons (const struct HtmlTheme *theme_p)
{
	return ((theme_p -> ht_collection_icon_s) || (theme_p -> ht_object_icon_s) || (theme_p -> ht_icons_map_p)) ? 1 : 0;
}
