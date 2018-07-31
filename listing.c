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
 * listing.c
 *
 *  Created on: 7 Oct 2016
 *      Author: billy
 */


#include "listing.h"

#include "repo.h"
#include "meta.h"
#include "theme.h"
#include "rest.h"

#include "apr_strings.h"
#include "apr_time.h"
#include "http_protocol.h"
#include "util_script.h"

#include "jansson.h"


#define S_LISTING_DEBUG (0)

static void PrintCollEntry (const collEnt_t *coll_entry_p, apr_pool_t *pool_p);

static json_t *GetIRodsObjectAsJSON (const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_pool_t *pool_p);

static bool SetStringValue (const char *src_s, char **dest_ss, apr_pool_t *pool_p);


static int CompareIRodsObjectsDoublePointers (const void *a_p, const void *b_p);



void InitIRodsObject (IRodsObject *obj_p)
{
	memset (obj_p, 0, sizeof (IRodsObject));
}



apr_status_t SetIRodsConfig (IRodsConfig *config_p, const char *exposed_root_s, const char *root_path_s, const char *metadata_root_link_s)
{
	apr_status_t status = APR_SUCCESS;

	config_p -> ic_exposed_root_s = exposed_root_s;
	config_p -> ic_root_path_s = root_path_s;
	config_p -> ic_metadata_root_link_s = metadata_root_link_s;

	return status;
}


apr_status_t SetIRodsObjectFromIdString (IRodsObject *obj_p, const char *id_s, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	apr_status_t status = APR_EGENERAL;
	int select_columns_p [7] = { COL_DATA_NAME, COL_D_OWNER_NAME, COL_COLL_NAME, COL_D_MODIFY_TIME, COL_DATA_SIZE, COL_D_RESC_NAME, -1 };
	int where_columns_p [1] = { COL_D_DATA_ID };
	const char *where_values_ss [1];
	genQueryOut_t *results_p = NULL;
	const char *minor_s = GetMinorId (id_s);

	if (!minor_s)
		{
			minor_s = id_s;
		}

	*where_values_ss = minor_s;

	results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, 1, 0, pool_p);

	if (results_p)
		{
			if (results_p -> rowCnt == 1)
				{
					char *name_s = apr_pstrdup (pool_p, results_p -> sqlResult [0].value);

					if (name_s)
						{
							char *owner_s = apr_pstrdup (pool_p, results_p -> sqlResult [1].value);

							if (owner_s)
								{
									char *coll_s = apr_pstrdup (pool_p, results_p -> sqlResult [2].value);

									if (coll_s)
										{
											char *modify_s = apr_pstrdup (pool_p, results_p -> sqlResult [3].value);

											if (modify_s)
												{
													char *size_s = apr_pstrdup (pool_p, results_p -> sqlResult [4].value);

													if (size_s)
														{
															char *resource_s = apr_pstrdup (pool_p, results_p -> sqlResult [5].value);

															if (resource_s)
																{
																	rodsLong_t size = atoi (size_s);

																	status = SetIRodsObject (obj_p, DATA_OBJ_T, id_s, name_s, coll_s, owner_s, resource_s, modify_s, size, pool_p);
																}		/* if (resource_s) */
															else
																{
																	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy resource \"%s\"", results_p -> sqlResult [5].value);
																}


														}		/* if (size_s) */
													else
														{
															ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy size \"%s\"", results_p -> sqlResult [4].value);
														}


												}		/* if (modify_s) */
											else
												{
													ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy date \"%s\"", results_p -> sqlResult [3].value);
												}


										}		/* if (coll_s) */
									else
										{
											ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy collection \"%s\"", results_p -> sqlResult [2].value);
										}


								}		/* if (owner_s) */
							else
								{
									ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy owner \"%s\"", results_p -> sqlResult [1].value);
								}

						}		/* if (name_s) */
					else
						{
							ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy name \"%s\"", results_p -> sqlResult [0].value);
						}

				}		/* if (results_p -> rowCnt == 1) */
			else
				{
					/* it may be a collection */
					select_columns_p [0] = COL_COLL_NAME;
					select_columns_p [1] = COL_COLL_OWNER_NAME;
					select_columns_p [2] = COL_COLL_PARENT_NAME;
					select_columns_p [3] = COL_COLL_CREATE_TIME;
					select_columns_p [4] = -1;

					where_columns_p [0] = COL_COLL_ID;

					freeGenQueryOut (&results_p);

					results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, 1, 0, pool_p);

					if (results_p)
						{
							char *name_s = apr_pstrdup (pool_p, results_p -> sqlResult [0].value);

							if (name_s)
								{
									char *parent_s = apr_pstrdup (pool_p, results_p -> sqlResult [2].value);

									if (parent_s)
										{
											char *owner_s = NULL;
											size_t parent_length = strlen (parent_s);

											/* Extract the local collection name relative to its parent */
											if (strncmp (parent_s, name_s, parent_length) == 0)
												{
													name_s += parent_length;

													if (*name_s == '/')
														{
															++ name_s;
														}
												}

											owner_s = apr_pstrdup (pool_p, results_p -> sqlResult [1].value);

											if (owner_s)
												{
													char *modify_s = apr_pstrdup (pool_p, results_p -> sqlResult [3].value);

													if (modify_s)
														{
															status = SetIRodsObject (obj_p, COLL_OBJ_T, id_s, name_s, parent_s, owner_s, NULL, modify_s, 0, pool_p);
														}		/* if (modify_s) */
													else
														{
															ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy date \"%s\"", results_p -> sqlResult [3].value);
														}

												}		/* if (owner_s) */
											else
												{
													ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy owner \"%s\"", results_p -> sqlResult [1].value);
												}

										}		/* if (parent_s) */
									else
										{
											ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy collection \"%s\"", results_p -> sqlResult [2].value);
										}

								}		/* if (name_s) */
							else
								{
									ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to copy name \"%s\"", results_p -> sqlResult [0].value);
								}

						}		/* if (results_p) */
				}

			freeGenQueryOut (&results_p);
		}		/* if (results_p) */

	return status;
}


IRodsObjectNode *AllocateIRodsObjectNode (const objType_t obj_type, const char *id_s, const char *data_s, const char *collection_s, const char *owner_name_s, const char *resource_s, const char *last_modified_time_s, const rodsLong_t size, apr_pool_t *pool_p)
{
	IRodsObject *obj_p = (IRodsObject *) malloc (sizeof (IRodsObject));

	if (obj_p)
		{
			IRodsObjectNode *node_p = (IRodsObjectNode *) malloc (sizeof (IRodsObjectNode));

			if (node_p)
				{
					if (SetIRodsObject (obj_p, obj_type, id_s, data_s, collection_s, owner_name_s, resource_s, last_modified_time_s, size, pool_p) == APR_SUCCESS)
						{
							node_p -> ion_object_p = obj_p;
							node_p -> ion_next_p = NULL;

							return node_p;
						}

					free (node_p);
				}

			free (obj_p);
		}

	return NULL;
}


void FreeIRodsObjectNode (IRodsObjectNode *node_p)
{
	free (node_p -> ion_object_p);
	free (node_p);
}


void FreeIRodsObjectNodeList (IRodsObjectNode *root_node_p)
{
	while (root_node_p)
		{
			IRodsObjectNode *next_node_p = root_node_p -> ion_next_p;

			FreeIRodsObjectNode (root_node_p);

			root_node_p = next_node_p;
		}
}



apr_status_t PrintIRodsObjectNodesToJSON (IRodsObjectNode *node_p, const IRodsConfig *config_p, request_rec *req_p)
{
	apr_status_t status = APR_EGENERAL;
	apr_pool_t *pool_p = req_p -> pool;

	json_t *values_p = json_array ();

	if (values_p)
		{
			bool success_flag = true;

			while (node_p && success_flag)
				{
					json_t *obj_json_p = GetIRodsObjectAsJSON (node_p -> ion_object_p, config_p, pool_p);

					if (obj_json_p)
						{
							if (json_array_append_new (values_p, obj_json_p) == 0)
								{
									node_p = node_p -> ion_next_p;
								}
							else
								{
									success_flag = false;
								}
						}
					else
						{
							success_flag = false;
						}
				}

			if (success_flag)
				{
					char *data_s = json_dumps (values_p, JSON_INDENT (2));

					if (data_s)
						{
							ap_rputs (data_s, req_p);
							free (data_s);
						}
				}

			json_decref (values_p);
		}
	else
		{

		}

	return status;
}


static json_t *GetIRodsObjectAsJSON (const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_pool_t *pool_p)
{
	json_t *irods_json_p = json_object ();

	if (irods_json_p)
		{
			char *relative_link_s = GetIRodsObjectRelativeLink (irods_obj_p, config_p, pool_p);

			if (relative_link_s)
				{
					if (json_object_set_new (irods_json_p, "path", json_string (relative_link_s)) == 0)
						{
							char *major_s = apr_itoa (pool_p, irods_obj_p -> io_obj_type);

							if (major_s)
								{
									char *id_s = apr_pstrcat (pool_p, major_s, ".", irods_obj_p -> io_id_s, NULL);

									if (id_s)
										{
											if (json_object_set_new (irods_json_p, "id", json_string (id_s)) == 0)
												{
													return irods_json_p;
												}
											else
												{
													ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to set \"id\": \"%s\"", id_s);
												}
										}
									else
										{
											ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to concatenate \"s\", \".\" & \"%s\"", major_s, irods_obj_p -> io_id_s);
										}
								}
							else
								{

								}
						}
					else
						{
							ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to set \"path\": \"%s\"", relative_link_s);
						}
				}
			else
				{
					ap_log_perror (APLOG_MARK, APLOG_ERR, APR_ENOMEM, pool_p, "Failed to create relative link for \"%s\"", irods_obj_p -> io_data_s);
				}

			json_decref (irods_json_p);
		}
	else
		{
			ap_log_perror (APLOG_MARK, APLOG_ERR, APR_ENOMEM, pool_p, "Failed to create JSON object");
		}

	return NULL;
}


apr_status_t SetIRodsObject (IRodsObject *obj_p, const objType_t obj_type, const char *id_s, const char *data_s, const char *collection_s, const char *owner_name_s, const char *resource_s, const char *last_modified_time_s, const rodsLong_t size, apr_pool_t *pool_p)
{
	apr_status_t status = APR_ENOMEM;
	bool done_id_flag = false;

	/*
	 * If the id string contains the major id too,
	 * scroll past it as we'll just store the minor id
	 * since we store the object type separately
	 */
	char *prefix_s = apr_itoa (pool_p, obj_type);

	if (prefix_s)
		{
			char *major_id_s = apr_pstrcat (pool_p, prefix_s, ".", NULL);

			if (major_id_s)
				{
					const size_t l = strlen (major_id_s);

					if (strncmp (major_id_s, id_s, l) == 0)
						{
							id_s += l;
						}

					done_id_flag = true;
				}
		}

	if (done_id_flag)
		{
			if (SetStringValue (id_s, & (obj_p -> io_id_s), pool_p))
				{
					if (SetStringValue (data_s, & (obj_p -> io_data_s), pool_p))
						{
							if (SetStringValue (collection_s, & (obj_p -> io_collection_s), pool_p))
								{
									if (SetStringValue (owner_name_s, & (obj_p -> io_owner_name_s), pool_p))
										{
											if (SetStringValue (resource_s, & (obj_p -> io_resource_s), pool_p))
												{
													if (SetStringValue (last_modified_time_s, & (obj_p -> io_last_modified_time_s), pool_p))
														{
															obj_p -> io_obj_type = obj_type;
															obj_p -> io_size = size;

															status = APR_SUCCESS;
														}		/* if (SetStringValue (last_modified_time_s, & (obj_p -> io_last_modified_time_s), pool_p)) */

												}		/* if (SetStringValue (resource_s, & (obj_p -> io_resource_s), pool_p)) */

										}		/* if (SetStringValue (owner_name_s, & (obj_p -> io_owner_name_s), pool_p)) */

								}		/* if (SetStringValue (collection_s, & (obj_p -> io_collection_s), pool_p)) */

						}		/* if (SetStringValue (data_s, & (obj_p -> io_data_s), pool_p)) */

				}		/* if (SetStringValue (id_s, & (obj_p -> io_id_s), pool_p)) */

		}		/* if (done_id_flag) */


	return status;
}


apr_status_t SetIRodsObjectFromCollEntry (IRodsObject *obj_p, const collEnt_t *coll_entry_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	apr_status_t status;

	if (S_LISTING_DEBUG)
		{
			PrintCollEntry (coll_entry_p, pool_p);
		}

	if (coll_entry_p -> objType == COLL_OBJ_T)
		{
			const char *id_s = GetCollectionId (coll_entry_p -> collName, connection_p, pool_p);

			status = SetIRodsObject (obj_p, coll_entry_p -> objType, id_s, coll_entry_p -> dataName, coll_entry_p -> collName, coll_entry_p -> ownerName, coll_entry_p -> resource, coll_entry_p -> modifyTime, coll_entry_p -> dataSize, pool_p);
		}
	else
		{
			status = SetIRodsObject (obj_p, coll_entry_p -> objType, coll_entry_p -> dataId, coll_entry_p -> dataName, coll_entry_p -> collName, coll_entry_p -> ownerName, coll_entry_p -> resource, coll_entry_p -> modifyTime, coll_entry_p -> dataSize, pool_p);
		}

	return status;
}



char *GetIRodsObjectFullPath (const IRodsObject *obj_p, apr_pool_t *pool_p)
{
	char *name_s = NULL;

	if (obj_p -> io_collection_s)
		{
			if (obj_p -> io_data_s)
				{
					const size_t l =  strlen (obj_p -> io_collection_s);

					if (* ((obj_p -> io_collection_s) + l - 1) == '/')
						{
							name_s = apr_pstrcat (pool_p, obj_p -> io_collection_s, obj_p -> io_data_s, NULL);
						}
					else
						{
							name_s = apr_pstrcat (pool_p, obj_p -> io_collection_s, "/", obj_p -> io_data_s, NULL);
						}
				}
			else
				{
					name_s = apr_pstrdup (pool_p, obj_p -> io_collection_s);
				}
		}
	else if (obj_p -> io_data_s)
		{
			name_s = apr_pstrdup (pool_p, obj_p -> io_data_s);
		}

	return name_s;
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


char *GetIRodsObjectRelativeLink (const IRodsObject *irods_obj_p, const IRodsConfig *config_p, apr_pool_t *pool_p)
{
	char *res_s = NULL;

	if ((config_p -> ic_exposed_root_s) && (irods_obj_p -> io_collection_s))
		{
			size_t l = strlen (config_p -> ic_exposed_root_s);

			if (strncmp (config_p -> ic_exposed_root_s, irods_obj_p -> io_collection_s, l) == 0)
				{
					char *escaped_uri_root_s = ap_escape_html (pool_p, ap_escape_uri (pool_p, config_p -> ic_root_path_s));
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

							//res_s = apr_pstrcat (pool_p, "./", escaped_relative_collection_s, "/", escaped_data_s, NULL);
							if (escaped_relative_collection_s && (strlen (escaped_relative_collection_s) > 0))
								{
									res_s = apr_pstrcat (pool_p, escaped_uri_root_s, separator_s, escaped_relative_collection_s, "/", escaped_data_s, NULL);
								}
							else
								{
									res_s = apr_pstrcat (pool_p, escaped_uri_root_s, separator_s, escaped_data_s, NULL);
								}
						}
					else
						{
							res_s = apr_pstrcat (pool_p, escaped_uri_root_s, separator_s, escaped_relative_collection_s, "/", NULL);
							//res_s = apr_pstrcat (pool_p, "./", escaped_relative_collection_s, "/", NULL);
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



apr_status_t GetAndPrintMetadataForIRodsObject (const IRodsObject *irods_obj_p, const char * const api_root_url_s, const char *zone_s, const struct HtmlTheme * const theme_p, apr_bucket_brigade *bb_p, rcComm_t *connection_p, request_rec *req_p, apr_pool_t *pool_p)
{
	apr_status_t status = APR_SUCCESS;
	apr_array_header_t *metadata_array_p = GetMetadata (connection_p, irods_obj_p -> io_obj_type, irods_obj_p -> io_id_s, irods_obj_p -> io_collection_s, zone_s, pool_p);

	apr_brigade_puts (bb_p, NULL, NULL, "<td class=\"metatable\"><div class=\"metadata_toolbar\"\n");

	if (metadata_array_p)
		{
			if (!apr_is_empty_array (metadata_array_p))
				{

					if (theme_p -> ht_show_download_metadata_links_flag)
						{
							status = PrintDownloadMetadataObjectAsLinks (theme_p, bb_p, api_root_url_s, irods_obj_p);
						}

					if (status == APR_SUCCESS)
						{
							apr_table_t *params_p = NULL;
							ap_args_to_table (req_p, &params_p);
							int editable_flag = GetEditableFlag (theme_p, params_p, pool_p);

							status = PrintMetadata (irods_obj_p -> io_id_s, metadata_array_p, theme_p, editable_flag, bb_p, api_root_url_s, pool_p);
						}
				}		/* if (!apr_is_empty_array (metadata_array_p)) */

		}		/* if (metadata_array_p) */

	apr_brigade_puts(bb_p, NULL, NULL, "</div></td>");

	return status;
}


apr_status_t GetAndPrintMetadataRestLinkForIRodsObject (const IRodsObject *irods_obj_p, const char * const apt_root_link_s, const char *zone_s, const struct HtmlTheme * const theme_p, apr_bucket_brigade *bb_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	apr_status_t status = APR_SUCCESS;
	objType_t obj_type = UNKNOWN_OBJ_T;
	const char *name_s = NULL;

	switch (irods_obj_p -> io_obj_type)
		{
			case DATA_OBJ_T:
				name_s = irods_obj_p -> io_data_s;
				obj_type = irods_obj_p -> io_obj_type;
				break;

			case COLL_OBJ_T:
				name_s = irods_obj_p -> io_collection_s;
				obj_type = irods_obj_p -> io_obj_type;
				break;

			default:
				break;

		}

	if (obj_type != UNKNOWN_OBJ_T)
		{
/*
			char *parent_id_s = GetParentCollectionId (irods_obj_p -> io_id_s, obj_type, zone_s, connection_p, pool_p);

			if (apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"metatable\"><a class=\"get_metadata\" id=\"2.%s_%d.%s\"></a></td>", parent_id_s, obj_type, irods_obj_p -> io_id_s) == APR_SUCCESS)
				{
					status = APR_SUCCESS;
				}
*/

			if ((status = apr_brigade_printf (bb_p, NULL, NULL, "<td class=\"metatable\"><div class=\"metadata_toolbar\"><a class=\"get_metadata\"></a>", obj_type)) == APR_SUCCESS)
				{
					if (theme_p -> ht_show_download_metadata_links_flag)
						{
							status = PrintDownloadMetadataObjectAsLinks (theme_p, bb_p, apt_root_link_s, irods_obj_p);
						}

					if (status == APR_SUCCESS)
						{
							status = apr_brigade_puts (bb_p, NULL, NULL, "</div></td>");
						}
				}
		}

	return status;
}




char *GetId (char *value_s, objType_t *type_p, apr_pool_t *pool_p)
{
	char *id_s = NULL;

	if (value_s)
		{
			char *sep_s = strchr (value_s, '.');

			if (sep_s)
				{
					objType_t obj_type = UNKNOWN_OBJ_T;
					long l = -1;

					*sep_s = '\0';
					l = strtol (value_s, NULL, 10);
					*sep_s = '.';

					switch (l)
						{
							case DATA_OBJ_T:
							case COLL_OBJ_T:
								obj_type = l;
								break;

							default:
								break;

						}

					if (obj_type != UNKNOWN_OBJ_T)
						{
							if (type_p)
								{
									*type_p = obj_type;
								}

							sep_s = strpbrk (value_s, " \t\n\r\v\f");

							if (sep_s)
								{
									char c = *sep_s;

									*sep_s = '\0';
									id_s = apr_pstrdup (pool_p, value_s);
									*sep_s = c;
								}
							else
								{
									id_s = apr_pstrdup (pool_p, value_s);
								}
						}

				}		/* if (sep_s) */

		}		/* if (value_s) */

	return id_s;
}



rodsObjStat_t *GetObjectStat (const char * const path_s, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	dataObjInp_t inp;
	rodsObjStat_t *stat_p = NULL;
	int status;

	memset (&inp, 0, sizeof (dataObjInp_t));
	rstrcpy (inp.objPath, path_s, MAX_NAME_LEN);

	status = rcObjStat (connection_p, &inp, &stat_p);

	if (status < 0)
		{
			const char *error_s = rodsErrorName (status, NULL);

			if (error_s)
				{
					ap_log_perror (APLOG_MARK, APLOG_ERR, APR_ENOSTAT, pool_p, "Failed to get object stat for %s, error: %s", path_s, error_s);
				}
			else
				{
					ap_log_perror (APLOG_MARK, APLOG_ERR, APR_ENOSTAT, pool_p, "Failed to get object stat for %s, error: %d", path_s, status);
				}
		}

	return stat_p;
}


void SortIRodsObjectNodeListIntoDirectoryOrder (IRodsObjectNode *root_node_p)
{
	IRodsObjectNode *node_p = root_node_p;
	size_t num_nodes = 0;

	while (node_p)
		{
			node_p = node_p -> ion_next_p;
			++ num_nodes;
		}

	if (num_nodes)
		{
			IRodsObject **objs_pp = malloc (num_nodes * sizeof (IRodsObject *));

			if (objs_pp)
				{
					IRodsObject **obj_pp = objs_pp;
					node_p = root_node_p;

					while (node_p)
						{
							*obj_pp = node_p -> ion_object_p;

							node_p = node_p -> ion_next_p;
							++ obj_pp;
						}

					qsort (objs_pp, num_nodes, sizeof (IRodsObjectNode *), CompareIRodsObjectsDoublePointers);

					node_p = root_node_p;
					obj_pp = objs_pp;

					while (node_p)
						{
							node_p -> ion_object_p = *obj_pp;

							node_p = node_p -> ion_next_p;
							++ obj_pp;
						}


					free (objs_pp);
				}
		}
}


static int CompareIRodsObjectsDoublePointers (const void *a_p, const void *b_p)
{
	int res = 0;
	const IRodsObject *obj_a_p = * ((const IRodsObject **) a_p);
	const IRodsObject *obj_b_p = * ((const IRodsObject **) b_p);

	if (obj_a_p -> io_obj_type != obj_b_p -> io_obj_type)
		{
			if (obj_a_p -> io_obj_type == COLL_OBJ_T)
				{
					res = -1;
				}
			else
				{
					res = 1;
				}

		}
	else
		{
			res = strcmp (obj_a_p -> io_collection_s, obj_b_p -> io_collection_s);

			if (obj_a_p -> io_obj_type != COLL_OBJ_T)
				{
					if (res == 0)
						{
							res = strcmp (obj_a_p -> io_data_s, obj_b_p -> io_data_s);
						}
				}

		}

	return res;
}


static void PrintCollEntry (const collEnt_t *coll_entry_p, apr_pool_t *pool_p)
{
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "\n=====================\n");
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "objType %d\n", coll_entry_p -> objType);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "replNum %d\n", coll_entry_p -> replNum);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "replStatus %d\n", coll_entry_p -> replStatus);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "dataMode %d\n", coll_entry_p -> dataMode);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "dataSize %d\n", coll_entry_p -> dataSize);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "collName %s\n", coll_entry_p -> collName);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "dataName %s\n", coll_entry_p -> dataName);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "dataId %s\n", coll_entry_p -> dataId);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "createTime %s\n", coll_entry_p -> createTime);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "modifyTime %s\n", coll_entry_p -> modifyTime);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "chksum", coll_entry_p -> chksum);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "resource %s\n", coll_entry_p -> resource);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "resc_hier %s\n", coll_entry_p -> resc_hier);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "phyPath %s\n", coll_entry_p -> phyPath);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "ownerName %s\n", coll_entry_p -> ownerName);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "dataType %s\n", coll_entry_p -> dataType);
	ap_log_perror (APLOG_MARK, APLOG_ERR, APR_SUCCESS, pool_p, "=====================\n");
}


static bool SetStringValue (const char *src_s, char **dest_ss, apr_pool_t *pool_p)
{
	bool success_flag = false;

	if (src_s)
		{
			char *dest_s = apr_pstrdup (pool_p, src_s);

			if (dest_s)
				{
					*dest_ss = dest_s;
					success_flag = true;
				}
		}
	else
		{
			*dest_ss = NULL;
			success_flag = true;
		}

	return success_flag;
}

