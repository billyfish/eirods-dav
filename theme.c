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

static int AreIconsDisplayed (const struct HtmlTheme *theme_p);

static apr_status_t PrintParentLink (const char *icon_s, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p);


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
			ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, req_p, "rcOpenCollection failed: %d = %s", status, get_rods_error_msg(status));

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
			char *metadata_link_s = apr_pstrcat (pool_p, davrods_resource_p -> root_dir, conf_p -> davrods_api_path_s, REST_METADATA_PATH_S, NULL);
			IRodsConfig irods_config;

			if (SetIRodsConfig (&irods_config, exposed_root_s, davrods_root_path_s, metadata_link_s) == APR_SUCCESS)
				{
					// Actually print the directory listing, one table row at a time.
					do
						{
							status = rclReadCollection (davrods_resource_p -> rods_conn, &coll_handle, &coll_entry);

							if (status >= 0)
								{
									IRodsObject irods_obj;

									apr_status = SetIRodsObjectFromCollEntry (&irods_obj, &coll_entry, davrods_resource_p -> rods_conn, pool_p);

									if (apr_status == APR_SUCCESS)
										{
											apr_status = PrintItem (theme_p, &irods_obj, &irods_config, bucket_brigade_p, pool_p, resource_p -> info -> rods_conn, req_p);

											if (apr_status != APR_SUCCESS)
												{
													ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "Failed to PrintItem for \"%s\":\"%s\"",
																				 coll_entry.collName ? coll_entry.collName : "",
																				 coll_entry.dataName ? coll_entry.dataName : "");
												}
										}
									else
										{
											ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "Failed to SetIRodsObjectFromCollEntry for \"%s\":\"%s\"",
																		 coll_entry.collName ? coll_entry.collName : "",
																		 coll_entry.dataName ? coll_entry.dataName : "");
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

				}		/* if (SetIRodsConfig (&irods_config, exposed_root_s, davrods_root_path_s, REST_METADATA_PATH_S)) */
			else
				{
					ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "SetIRodsConfig failed for exposed_root_s:\"%s\" davrods_root_path_s:\"%s\"",
												 exposed_root_s ? exposed_root_s : "<NULL>",
												 davrods_root_path_s ? davrods_root_path_s: "<NULL>");
				}

		}		/* if (apr_status == APR_SUCCESS) */
	else
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "PrintAllHTMLBeforeListing failed");
		}

	apr_status = PrintAllHTMLAfterListing (theme_p, req_p, bucket_brigade_p, pool_p);
	if (apr_status != APR_SUCCESS)
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "PrintAllHTMLAfterListing failed");
		}


	CloseBucketsStream (bucket_brigade_p);

	if ((status = ap_pass_brigade (output_p, bucket_brigade_p)) != APR_SUCCESS)
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
	const char * const table_end_s = "</tbody>\n</table>\n</main>\n";

	apr_status_t apr_status = PrintBasicStringToBucketBrigade (table_end_s, bucket_brigade_p, req_p, __FILE__, __LINE__);

	if (apr_status == APR_SUCCESS)
		{
			if (theme_p -> ht_bottom_s)
				{
					apr_status = PrintBasicStringToBucketBrigade (theme_p -> ht_bottom_s, bucket_brigade_p, req_p, __FILE__, __LINE__);

					if (apr_status != APR_SUCCESS)
						{
							return apr_status;
						} /* if (apr_ret != APR_SUCCESS) */

				}		/* if (theme_p -> ht_bottom_s) */

			apr_status =  PrintBasicStringToBucketBrigade ("\n</body>\n</html>\n", bucket_brigade_p, req_p, __FILE__, __LINE__);
		}
	else
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "PrintBasicStringToBucketBrigade failed for \"%s\"", table_end_s);
		}

	return apr_status;
}


apr_status_t PrintAllHTMLBeforeListing (struct HtmlTheme *theme_p, const char * const relative_uri_s, const char * const user_s, const char * const zone_s, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p)
{
	// Send start of HTML document.
	const char *escaped_relative_uri_s = ap_escape_html (pool_p, relative_uri_s);
	const char *escaped_zone_s = ap_escape_html (pool_p, zone_s);

	/*
	 * Print the start of the doc
	 */
	apr_status_t apr_status =	apr_brigade_printf (bucket_brigade_p, NULL, NULL, "<!DOCTYPE html>\n<html lang=\"en\">\n<head><title>Index of %s on %s</title>\n", escaped_relative_uri_s, escaped_zone_s);
	if (apr_status != APR_SUCCESS)
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "Failed to add start of html doc with relative uri \"%s\" and zone \"%s\"", escaped_relative_uri_s, escaped_zone_s);

			return apr_status;
		}


	/*
	 * If we have additional data for the <head> section, add it here.
	 */
	if (theme_p -> ht_head_s)
		{
			apr_status = PrintBasicStringToBucketBrigade (theme_p -> ht_head_s, bucket_brigade_p, req_p, __FILE__, __LINE__);

			if (apr_status != APR_SUCCESS)
				{
					return apr_status;
				} /* if (apr_ret != APR_SUCCESS) */

		} /* if (theme_p -> ht_head_s) */


	/*
	 * Write the start of the body section
	 */
	apr_status = PrintBasicStringToBucketBrigade ("<body>\n\n"
			"<!-- Warning: Do not parse this directory listing programmatically,\n"
			"              the format may change without notice!\n"
			"              If you want to script access to these WebDAV collections,\n"
			"              please use the PROPFIND method instead. -->\n\n",
			bucket_brigade_p, req_p, __FILE__, __LINE__);

	if (apr_status != APR_SUCCESS)
		{
			return apr_status;
		}


	/*
	 * If we have additional data to go above the directory listing, add it here.
	 */
	if (theme_p -> ht_top_s)
		{
			apr_status = PrintBasicStringToBucketBrigade (theme_p -> ht_top_s, bucket_brigade_p, req_p, __FILE__, __LINE__);

			if (apr_status != APR_SUCCESS)
				{
					return apr_status;
				} /* if (apr_ret != APR_SUCCESS) */

		}		/* if (theme_p -> ht_top_s) */


	/*
	 * Print the user status
	 */
	apr_status = apr_brigade_printf (bucket_brigade_p, NULL, NULL, "<main>\n<h1>You are logged in as %s and browsing the index of %s on %s</h1>\n", user_s, escaped_relative_uri_s, escaped_zone_s);
	if (apr_status != APR_SUCCESS)
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "Failed to add the user status with user \"%s\", uri \"%s\", zone \"%s\"", user_s, escaped_relative_uri_s, escaped_zone_s);
			return apr_status;
		} /* if (apr_ret != APR_SUCCESS) */


	if (strcmp (relative_uri_s, "/"))
		{
			apr_status = PrintParentLink (theme_p -> ht_parent_icon_s, req_p, bucket_brigade_p, pool_p);

			if (apr_status != APR_SUCCESS)
				{
					ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "Failed to print parent link");
					return apr_status;
				}

		}		/* if (strcmp (davrods_resource_p->relative_uri, "/")) */


	/*
	 * Add the listing class
	 */
	apr_status = apr_brigade_printf (bucket_brigade_p, NULL, NULL, "<table class=\"%s\">\n<thead>\n<tr>", theme_p -> ht_listing_class_s ? theme_p -> ht_listing_class_s : "listing");
	if (apr_status != APR_SUCCESS)
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, apr_status, req_p, "Failed to add start of table listing with class \"%s\"", theme_p -> ht_listing_class_s ? theme_p -> ht_listing_class_s : "listing");
			return apr_status;
		} /* if (apr_ret != APR_SUCCESS) */


	/*
	 * If we are going to display icons, add the column
	 */
	if (AreIconsDisplayed (theme_p))
		{
			apr_status = PrintBasicStringToBucketBrigade ("<th class=\"icon\"></th>", bucket_brigade_p, req_p, __FILE__, __LINE__);

			if (apr_status != APR_SUCCESS)
				{
					return apr_status;
				} /* if (apr_ret != APR_SUCCESS) */

		}		/* if (AreIconsDisplayed (theme_p)) */


	apr_status = PrintBasicStringToBucketBrigade ("<th class=\"name\">Name</th><th class=\"size\">Size</th><th class=\"owner\">Owner</th><th class=\"datestamp\">Last modified</th>", bucket_brigade_p, req_p, __FILE__, __LINE__);
	if (apr_status != APR_SUCCESS)
		{
			return apr_status;
		} /* if (apr_ret != APR_SUCCESS) */


	if (theme_p -> ht_show_metadata_flag)
		{
			apr_status = PrintBasicStringToBucketBrigade ("<th class=\"properties\">Properties</th>", bucket_brigade_p, req_p, __FILE__, __LINE__);

			if (apr_status != APR_SUCCESS)
				{
					return apr_status;
				} /* if (apr_ret != APR_SUCCESS) */

		}		/* if (theme_p -> ht_show_metadata_flag) */


	apr_status = PrintBasicStringToBucketBrigade ("</tr>\n</thead>\n<tbody>\n", bucket_brigade_p, req_p, __FILE__, __LINE__);

	return apr_status;
}



static apr_status_t PrintParentLink (const char *icon_s, request_rec *req_p, apr_bucket_brigade *bucket_brigade_p, apr_pool_t *pool_p)
{
	apr_status_t status = PrintBasicStringToBucketBrigade ("<p><a href=\"..\">", bucket_brigade_p, req_p, __FILE__, __LINE__);

	if (status != APR_SUCCESS)
		{
			return status;
		}

	if (icon_s)
		{
			const char *escaped_icon_s = ap_escape_html (pool_p, icon_s);

			status = apr_brigade_printf (bucket_brigade_p, NULL, NULL, "<img src=\"%s\" alt=\"Browse to parent Collection\"/>", escaped_icon_s);

			if (status != APR_SUCCESS)
				{
					ap_log_rerror (APLOG_MARK, APLOG_ERR, status, req_p, "Failed to print icon \"%s\"", escaped_icon_s);
					return status;
				}
		}
	else
		{
			/* Print a north-west arrow */
			status = PrintBasicStringToBucketBrigade ("&#8598;", bucket_brigade_p, req_p, __FILE__, __LINE__);

			if (status != APR_SUCCESS)
				{
					return status;
				}
		}

	status = PrintBasicStringToBucketBrigade (" Parent collection</a></p>\n", bucket_brigade_p, req_p, __FILE__, __LINE__);

	return status;
}


apr_status_t PrintItem (struct HtmlTheme *theme_p, const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_bucket_brigade *bb_p, apr_pool_t *pool_p, rcComm_t *connection_p, request_rec *req_p)
{
	apr_status_t status = APR_SUCCESS;
	const char *link_suffix_s = irods_obj_p -> io_obj_type == COLL_OBJ_T ? "/" : NULL;
	const char *name_s = GetIRodsObjectDisplayName (irods_obj_p);
	char *timestamp_s = GetIRodsObjectLastModifiedTime (irods_obj_p, pool_p);
	char *size_s = GetIRodsObjectSizeAsString (irods_obj_p, pool_p);

	if (theme_p -> ht_show_ids_flag)
		{
			apr_brigade_printf (bb_p, NULL, NULL, "<tr class=\"id\" id=\"%s\">", irods_obj_p -> io_id_s);
		}
	else
		{
			status = PrintBasicStringToBucketBrigade ("<tr>", bb_p, req_p, __FILE__, __LINE__);

			if (status != APR_SUCCESS)
				{
					return status;
				}
		}

	if (name_s)
		{
			const char *icon_s = GetIRodsObjectIcon (irods_obj_p, theme_p);
			const char *alt_s = GetIRodsObjectAltText (irods_obj_p);
			char *relative_link_s = GetIRodsObjectRelativeLink (irods_obj_p, config_p, pool_p);

			// Collection links need a trailing slash for the '..' links to work correctly.
			if (icon_s)
				{
					apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"icon\"><img src=\"%s\"", ap_escape_html (pool_p, icon_s));

					if (alt_s)
						{
							apr_brigade_printf (bb_p, NULL, NULL, " alt=\"%s\"", alt_s);
						}

					status = PrintBasicStringToBucketBrigade (" /></td>", bb_p, req_p, __FILE__, __LINE__);
					if (status != APR_SUCCESS)
						{
							return status;
						}
				}

			apr_brigade_printf(bb_p, NULL, NULL,
					"<td class=\"name\"><a href=\"%s\">%s%s</a></td>",
					relative_link_s,
					ap_escape_html (pool_p, name_s),
					link_suffix_s ? link_suffix_s : "");

		}		/* if (name_s) */

	// Print data object size.
	status = PrintBasicStringToBucketBrigade ("<td class=\"size\">", bb_p, req_p, __FILE__, __LINE__);
	if (status != APR_SUCCESS)
		{
			return status;
		}


	if (size_s)
		{
			apr_brigade_printf (bb_p, NULL, NULL, "%sB", size_s);
		}
	else if (irods_obj_p -> io_obj_type == DATA_OBJ_T)
		{
			apr_brigade_printf(bb_p, NULL, NULL, "%luB", irods_obj_p -> io_size);
		}

	status = PrintBasicStringToBucketBrigade ("</td>", bb_p, req_p, __FILE__, __LINE__);
	if (status != APR_SUCCESS)
		{
			return status;
		}


	// Print owner
	apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"owner\">%s</td>", ap_escape_html (pool_p, irods_obj_p -> io_owner_name_s));

	if (timestamp_s)
		{
			apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"time\">%s</td>", timestamp_s);
		}
	else
		{
			status = PrintBasicStringToBucketBrigade ("<td class=\"time\"></td>", bb_p, req_p, __FILE__, __LINE__);
			if (status != APR_SUCCESS)
				{
					return status;
				}
		}


	if (theme_p -> ht_show_metadata_flag)
		{
			const char *zone_s = NULL;

			if (GetAndPrintMetadataForIRodsObject (irods_obj_p, config_p -> ic_metadata_root_link_s, zone_s, bb_p, connection_p, pool_p) != 0)
				{

				}
		}

	status = PrintBasicStringToBucketBrigade ("</tr>\n", bb_p, req_p, __FILE__, __LINE__);
	if (status != APR_SUCCESS)
		{
			return status;
		}

	return status;
}


static int AreIconsDisplayed (const struct HtmlTheme *theme_p)
{
	return ((theme_p -> ht_collection_icon_s) || (theme_p -> ht_object_icon_s) || (theme_p -> ht_icons_map_p)) ? 1 : 0;
}
