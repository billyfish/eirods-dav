/*
 * listing.c
 *
 *  Created on: 7 Oct 2016
 *      Author: billy
 */


#include "listing.h"

#include "repo.h"
#include "meta.h"

#include "apr_strings.h"
#include "apr_time.h"


int SetIRodsObject (IRodsObject *obj_p, const objType_t obj_type, const char *id_s, const char *data_s, const char *collection_s, const char *owner_name_s, const char *last_modified_time_s, const rodsLong_t size)
{
	int res = 1;

	obj_p -> io_obj_type = obj_type;
	obj_p -> io_id_s = id_s;
	obj_p -> io_data_s = data_s;
	obj_p -> io_collection_s = collection_s;
	obj_p -> io_owner_name_s = owner_name_s;
	obj_p -> io_last_modified_time_s = last_modified_time_s;
	obj_p -> io_size = size;

	return res;
}


int SetIRodsObjectFromCollEntry (IRodsObject *obj_p, const collEnt_t *coll_entry_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	int res;

	if (coll_entry_p -> objType == COLL_OBJ_T)
		{
			const char *id_s = NULL;

			/*
			 * For collections, the data id is NULL, so let's get the collection id for it.
			 */

			int select_columns_p [2] = { COL_COLL_ID, -1 };
			int where_columns_p [1] = { COL_COLL_NAME };
			const char *where_values_ss [1] = { coll_entry_p -> collName };

			genQueryOut_t *results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, 1, pool_p);

			if (results_p)
				{
					if (results_p -> rowCnt == 1)
						{
							id_s = apr_pstrdup (pool_p, results_p -> sqlResult [0].value);
						}

					freeGenQueryOut (&results_p);
				}

			res = SetIRodsObject (obj_p, coll_entry_p -> objType, id_s, coll_entry_p -> dataName, coll_entry_p -> collName, coll_entry_p -> ownerName, coll_entry_p -> modifyTime, coll_entry_p -> dataSize);

		}
	else
		{
			res = SetIRodsObject (obj_p, coll_entry_p -> objType, coll_entry_p -> dataId, coll_entry_p -> dataName, coll_entry_p -> collName, coll_entry_p -> ownerName, coll_entry_p -> modifyTime, coll_entry_p -> dataSize);
		}

	return res;
}


const char *GetIRodsObjectDisplayName (const IRodsObject *obj_p)
{
	const char *name_s = NULL;

	switch (obj_p -> io_obj_type)
		{
			case DATA_OBJ_T:
				name_s = obj_p -> io_data_s;
				break;

			case COLL_OBJ_T:
				name_s = get_basename (obj_p -> io_collection_s);
				break;

			default:
				break;
		}

	return name_s;
}


const char *GetIRodsObjectIcon (const IRodsObject *irods_obj_p, const struct HtmlTheme * const theme_p)
{
	const char *icon_s = NULL;

	if (theme_p -> ht_icons_map_p)
		{
			const char *key_s = NULL;

			switch (irods_obj_p -> io_obj_type)
				{
					case DATA_OBJ_T:
						key_s = strrchr (irods_obj_p -> io_data_s, '.');
						break;

					case COLL_OBJ_T:
						key_s = get_basename (irods_obj_p -> io_collection_s);
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
			switch (irods_obj_p -> io_obj_type)
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


const char *GetIRodsObjectAltText (const IRodsObject *irods_obj_p)
{
	const char *alt_s = NULL;

	switch (irods_obj_p -> io_obj_type)
		{
			case DATA_OBJ_T:
				alt_s = "iRods Data Object";
				break;

			case COLL_OBJ_T:
				alt_s = "iRods Collection";
				break;

			default:
				break;
		}

	return alt_s;
}


char *GetIRodsObjectRelativeLink (const IRodsObject *irods_obj_p, const char *uri_root_s, const char *exposed_root_s, apr_pool_t *pool_p)
{
	char *res_s = NULL;

	if (exposed_root_s && (irods_obj_p -> io_collection_s))
		{
			size_t l = strlen (exposed_root_s);

			if (strncmp (exposed_root_s, irods_obj_p -> io_collection_s, l) == 0)
				{
					char *escaped_uri_root_s = ap_escape_html (pool_p, ap_escape_uri (pool_p, uri_root_s));
					char *escaped_relative_collection_s = ap_escape_html (pool_p, ap_escape_uri (pool_p, (irods_obj_p -> io_collection_s) + l));
					const char *separator_s = "";

					l = strlen (escaped_uri_root_s);

					if (l == 0)
						{
							escaped_uri_root_s = "/";
						}
					else
						{
							if ((* (escaped_uri_root_s  +  (l - 1)) != '/') && (*escaped_relative_collection_s != '/'))
								{
									separator_s = "/";
								}
						}

					if (irods_obj_p -> io_data_s)
						{
							char *escaped_data_s = ap_escape_html (pool_p, irods_obj_p -> io_data_s);

							res_s = apr_pstrcat (pool_p, escaped_uri_root_s, separator_s, escaped_relative_collection_s, "/", escaped_data_s, NULL);
						}
					else
						{
							res_s = apr_pstrcat (pool_p, escaped_uri_root_s, separator_s, escaped_relative_collection_s, "/", NULL);
						}

				}

		}

	return res_s;
}


char *GetIRodsObjectSizeAsString (const IRodsObject *irods_obj_p, apr_pool_t *pool_p)
{
	char *size_s = NULL;

	if (irods_obj_p -> io_obj_type == DATA_OBJ_T)
		{
			char buffer_s [5] = { 0 };

			// Fancy file size formatting.
			apr_strfsize (irods_obj_p -> io_size, buffer_s);

			if (*buffer_s)
				{
					size_s = apr_pstrdup (pool_p, buffer_s);
				}
			else
				{
					size_s = apr_ltoa (pool_p, irods_obj_p -> io_size);
				}

		}		/* if (irods_obj_p -> io_obj_type == DATA_OBJ_T) */

	return size_s;
}


char *GetIRodsObjectLastModifiedTime (const  IRodsObject *irods_obj_p, apr_pool_t *pool_p)
{
	char *datestamp_s = NULL;

	// Print modified-date string.
	uint64_t timestamp = atoll (irods_obj_p -> io_last_modified_time_s);
	apr_time_t apr_time = 0;
	apr_status_t status = apr_time_ansi_put (&apr_time, timestamp);

	if (status == APR_SUCCESS)
		{
			apr_time_exp_t exploded = { 0 };

			status = apr_time_exp_lt (&exploded, apr_time);

			if (status == APR_SUCCESS)
				{
					char date_s [1024] = { 0 };
					size_t ret_size;

					status = apr_strftime (date_s, &ret_size, sizeof (date_s), "%Y-%m-%d %H:%M", &exploded);

					if (status == APR_SUCCESS)
						{
							datestamp_s = apr_pstrdup (pool_p, date_s);
						}
				}
		}

	if (!datestamp_s)
		{
			datestamp_s = apr_palloc (pool_p, APR_RFC822_DATE_LEN + 1);

			if (datestamp_s)
				{
					status = apr_rfc822_date (datestamp_s, timestamp * 1000 * 1000);
				}
		}

	return datestamp_s;
}


int GetAndAddMetadataForIRodsObject (const IRodsObject *irods_obj_p, const char * const link_s, apr_bucket_brigade *bb_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	int status = -1;
	apr_array_header_t *metadata_array_p = GetMetadata (connection_p, irods_obj_p -> io_obj_type, irods_obj_p -> io_id_s, irods_obj_p -> io_collection_s, pool_p);

	apr_brigade_puts (bb_p, NULL, NULL, "<td class=\"metadata\">");

	if (metadata_array_p)
		{
			if (!apr_is_empty_array (metadata_array_p))
				{
					status = PrintMetadata (metadata_array_p, bb_p, link_s);
				}		/* if (!apr_is_empty_array (metadata_array_p)) */

		}		/* if (metadata_array_p) */

	apr_brigade_puts(bb_p, NULL, NULL, "</td>");

	return status;
}
