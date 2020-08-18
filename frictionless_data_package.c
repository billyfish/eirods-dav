/*
** Copyright 2014-2018 The Earlham Institute
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
 * frictionless_data_package.c
 *
 *  Created on: 12 Aug 2020
 *      Author: billy
 */

#include "frictionless_data_package.h"
#include "meta.h"
#include "repo.h"
#include "theme.h"

#include "httpd.h"

#include "irods/rcConnect.h"

#include "jansson.h"


/*
 * Static declarations
 */

static const char * const S_DATA_PACKAGE_S = "datapackage.json";


static const char *GetJSONString (const json_t *json_p, const char * const key_s);

static bool SetJSONString (json_t *json_p, const char * const key_s, const char * const value_s, apr_pool_t *pool_p);

static json_t *GetResources (const dav_resource *resource_p);

static bool AddResources (json_t *fd_p, const dav_resource *resource_p);

static bool AddResource (collEnt_t * const entry_p, json_t *resources_p, rcComm_t *connection_p, apr_pool_t *pool_p);

static json_t *PopulateResourceFromDataObject (collEnt_t * const entry_p,  rcComm_t *connection_p, apr_pool_t *pool_p);

static json_t *PopulateResourceFromCollection (collEnt_t * const entry_p,  rcComm_t *connection_p, apr_pool_t *pool_p);

static bool AddLicense (json_t *resource_p, const char *name_s, const char *url_s, apr_pool_t *pool_p);

static char *ConvertFDCompliantName (char *name_s);

static apr_status_t BuildDataPackage (json_t *data_package_p, const apr_array_header_t *metadata_list_p, apr_pool_t *pool_p);



/*
 * API definitions
 */

apr_status_t AddFrictionlessDataPackage (rcComm_t *connection_p, const char *collection_id_s, const char *collection_name_s, const char *zone_s, apr_pool_t *pool_p)
{
	apr_status_t status = APR_SUCCESS;
	apr_array_header_t *metadata_array_p = GetMetadata (connection_p, COLL_OBJ_T, collection_id_s, collection_name_s, zone_s, pool_p);

	if (metadata_array_p)
		{
			if (!apr_is_empty_array (metadata_array_p))
				{

				}		/* if (!apr_is_empty_array (metadata_array_p)) */

		}		/* if (metadata_array_p) */

	return status;
}



dav_error *DeliverFDDataPackage (const dav_resource *resource_p, ap_filter_t *output_p)
{
	dav_error *res_p = NULL;
	struct dav_resource_private *davrods_resource_p = (struct dav_resource_private *) resource_p -> info;
	apr_pool_t *pool_p = resource_p -> pool;
	request_rec *req_p = resource_p -> info -> r;
	apr_bucket_brigade *bb_p = apr_brigade_create (pool_p, output_p -> c -> bucket_alloc);

	if (bb_p)
		{
			json_t *dp_p = json_object ();

			if (dp_p)
				{
					const char *collection_s = strchr (davrods_resource_p -> rods_path, '/');

					if (collection_s)
						{
							char *collection_id_s;

							++ collection_s;

							collection_id_s = GetCollectionId (collection_s, davrods_resource_p -> rods_conn, pool_p);

							if (collection_id_s)
								{
									apr_array_header_t *metadata_p = GetMetadata (davrods_resource_p -> rods_conn, COLL_OBJ_T, collection_id_s, NULL, davrods_resource_p -> rods_env -> rodsZone, pool_p);

									if (metadata_p)
										{
											apr_status_t status = BuildDataPackage (dp_p, metadata_p, pool_p);

											if (json_object_set_new (dp_p, "name", json_string (davrods_resource_p -> root_dir)) == 0)
												{
													if (AddResources (dp_p, resource_p))
														{
															apr_status_t apr_status;
															char *dp_s = json_dumps (dp_p, JSON_INDENT (2));

															if (dp_s)
																{
																	if ((apr_status = apr_brigade_puts (bb_p, NULL, NULL, dp_s)) == APR_SUCCESS)
																		{
																			if ((apr_status = ap_pass_brigade (output_p, bb_p)) == APR_SUCCESS)
																				{

																				}
																		}

																	free (dp_s);
																}

														}
												}

										}		/* if (metadata_p) */

								}		/* if (collection_id_s) */

						}

					json_decref (dp_p);
				}		/* if (dp_p) */

			apr_brigade_destroy (bb_p);
		}		/* if (bb_p) */

	return res_p;
}


char *GetFDDataPackageRequestCollectionPath (const char *request_uri_s, apr_pool_t *pool_p)
{
	char *parent_uri_s = NULL;
	const size_t path_length = strlen (request_uri_s);
	const size_t fd_package_length = strlen (S_DATA_PACKAGE_S);

	if (path_length > fd_package_length)
		{
			const char *temp_s = request_uri_s + path_length - fd_package_length;

			if (* (temp_s - 1) == '/')
				{
					if (strncmp (temp_s, S_DATA_PACKAGE_S, fd_package_length) == 0)
						{
							parent_uri_s = apr_pstrndup (pool_p, request_uri_s, path_length - fd_package_length);
						}
				}
		}

	return parent_uri_s;
}


int IsFDDataPackageRequest (const char *request_uri_s, const davrods_dir_conf_t *conf_p)
{
	int fd_data_package_flag = 0;

	if ((conf_p -> theme_p) && (conf_p -> theme_p -> ht_show_fd_data_packages_flag))
		{
			/*
			 * Check if the last part of the filename matches "/datapackage.json"
			 */

			const size_t path_length = strlen (request_uri_s);
			const size_t fd_package_length = strlen (S_DATA_PACKAGE_S);

			if (path_length > fd_package_length)
				{
					const char *temp_s = request_uri_s + path_length - fd_package_length;

					if (* (temp_s - 1) == '/')
						{
							if (strncmp (temp_s, S_DATA_PACKAGE_S, fd_package_length) == 0)
								{
									/*
									 * @TODO: check that the collection exists
									 */
									fd_data_package_flag = 1;
								}
						}
				}
		}

	return fd_data_package_flag;
}


static apr_status_t BuildDataPackage (json_t *data_package_p, const apr_array_header_t *metadata_list_p, apr_pool_t *pool_p)
{
	apr_status_t status = APR_SUCCESS;
	const int size = metadata_list_p -> nelts;

	const char *license_name_key_s = "license";
	const char *license_name_value_s = NULL;

	const char *license_url_key_s = "license_url";
	const char *license_url_value_s = NULL;

	/*
	 * name

A short url-usable (and preferably human-readable) name of the package. This MUST be lower-case and contain only alphanumeric characters along with “.”, “_” or “-” characters. It will function as a unique identifier and therefore SHOULD be unique in relation to any registry in which this package will be deposited (and preferably globally unique).
	 */
	const char *name_key_s = "name";
	char *name_value_s = NULL;

	const char *title_key_s = "title";
	const char *title_value_s = NULL;

	const char *id_key_s = "id";
	const char *id_value_s = NULL;

	const size_t num_to_do = 5;
	size_t num_done = 0;

	if (size > 0)
		{
			int i = 0;

			/*
			 * Collect all of the metadata
			 */
			while ((i < size) && (num_done < num_to_do))
				{
					const IrodsMetadata *metadata_p = APR_ARRAY_IDX (metadata_list_p, i, IrodsMetadata *);

					if ((license_name_value_s == NULL) && (strcmp (metadata_p -> im_key_s, license_name_key_s) == 0))
						{
							license_name_value_s = metadata_p -> im_value_s;
							++ num_done;
						}
					else if ((license_url_value_s == NULL) && (strcmp (metadata_p -> im_key_s, license_url_key_s) == 0))
						{
							license_url_value_s = metadata_p -> im_value_s;
							++ num_done;
						}
					else if ((name_value_s == NULL) && (strcmp (metadata_p -> im_key_s, name_key_s) == 0))
						{
							name_value_s = metadata_p -> im_value_s;
							++ num_done;
						}
					else if ((title_value_s == NULL) && (strcmp (metadata_p -> im_key_s, title_key_s) == 0))
						{
							title_value_s = metadata_p -> im_value_s;
							++ num_done;
						}
					else if ((id_value_s == NULL) && (strcmp (metadata_p -> im_key_s, id_key_s) == 0))
						{
							id_value_s = metadata_p -> im_value_s;
							++ num_done;
						}

					++ i;
				}		/* while ((i < size) && (num_done < num_to_do)) */


			if (license_name_value_s && license_url_value_s)
				{
					if (!AddLicense (data_package_p, license_name_value_s, license_url_value_s, pool_p))
						{

						}
				}

			if (name_value_s)
				{
					char *converted_name_s = ConvertFDCompliantName (name_value_s);

					if (!SetJSONString (data_package_p, "name", converted_name_s, pool_p))
						{

						}

				}

			if (title_value_s)
				{
					if (!SetJSONString (data_package_p, "title", title_value_s, pool_p))
						{

						}
				}

			if (id_value_s)
				{
					if (!SetJSONString (data_package_p, "id", id_value_s, pool_p))
						{

						}
				}


		}		/* if (size > 0) */
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, pool_p, "No metadata results");
		}

	return status;
}


/*
 * Static definitions
 */

static const char *GetJSONString (const json_t *json_p, const char * const key_s)
{
	const char *value_s = NULL;
	json_t *value_p = json_object_get (json_p, key_s);

	if (value_p)
		{
			if (json_is_string (value_p))
				{
					value_s = json_string_value (value_p);
				}
		}

	return value_s;
}



static bool SetJSONString (json_t *json_p, const char * const key_s, const char * const value_s, apr_pool_t *pool_p)
{
	bool success_flag = false;

	if (value_s)
		{
			json_t *str_p = json_string (value_s);

			if (str_p)
				{
					if (json_object_set_new (json_p, key_s, str_p) == 0)
						{
							success_flag = true;
						}
					else
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, pool_p, "Failed to set \"%s\": \"%s\"", key_s, value_s);
							json_decref (str_p);
						}
				}
			else
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, pool_p, "Failed to create json string for \"%s\"", value_s);
				}

		}

	return success_flag;
}


static bool AddResources (json_t *fd_p, const dav_resource *resource_p)
{
	json_t *resources_json_p = GetResources (resource_p);

	if (resources_json_p)
		{
			if (json_object_set_new (fd_p, "resources", resources_json_p) == 0)
				{
					return true;
				}

			json_decref (resources_json_p);
		}

	return false;
}



static json_t *GetResources (const dav_resource *resource_p)
{
	json_t *resources_json_p = NULL;
	struct dav_resource_private *davrods_resource_p = (struct dav_resource_private *) resource_p -> info;
	request_rec *req_p = davrods_resource_p -> r;
	IRodsConfig irods_config;

	if (InitIRodsConfig (&irods_config, resource_p) == APR_SUCCESS)
		{
			resources_json_p = json_array ();

			if (resources_json_p)
				{
					davrods_dir_conf_t *conf_p = davrods_resource_p->conf;
					collHandle_t collection_handle;
					int status;
					char *path_s = apr_pstrcat (resource_p -> pool, davrods_resource_p -> rods_root);

					memset (&collection_handle, 0, sizeof (collHandle_t));

					// Open the collection
					status = rclOpenCollection (davrods_resource_p -> rods_conn, davrods_resource_p -> rods_path, LONG_METADATA_FG, &collection_handle);

					if (status >= 0)
						{
							collEnt_t coll_entry;

							memset (&coll_entry, 0, sizeof (collEnt_t));

							// Actually print the directory listing, one table row at a time.
							do
								{
									status = rclReadCollection (davrods_resource_p -> rods_conn, &collection_handle, &coll_entry);

									if (status >= 0)
										{
											/*
											 * Add resource
											 */

											if (!AddResource (&coll_entry, resources_json_p, davrods_resource_p -> rods_conn, resource_p -> pool))
												{
													status = -1;
												}
										}		/* if (status >= 0) */
									else if (status == CAT_NO_ROWS_FOUND)
										{
											// End of collection.
										}
									else
										{
											ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_EGENERAL,
																		req_p,
																		"rcReadCollection failed for collection <%s> with error <%s>",
																		davrods_resource_p->rods_path, get_rods_error_msg(status));
										}
								}
							while (status >= 0);


							rclCloseCollection (&collection_handle);
						}		/* if (collection_handle >= 0) */
					else
						{
							ap_log_rerror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, req_p, "rcOpenCollection failed: %d = %s", status, get_rods_error_msg (status));
						}

				}		/* if (resources_json_p) */

		}		/* if (SetIRodsConfig (&irods_config, exposed_root_s, davrods_root_path_s, metadata_link_s) == APR_SUCCESS) */



	return resources_json_p;
}


static bool AddResource (collEnt_t * const entry_p, json_t *resources_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	json_t *resource_p = NULL;

	if (entry_p -> objType == COLL_OBJ_T)
		{
			resource_p = PopulateResourceFromCollection (entry_p, connection_p, pool_p);
		}
	else if (entry_p -> objType == DATA_OBJ_T)
		{
			resource_p = PopulateResourceFromDataObject (entry_p, connection_p, pool_p);
		}

	if (resource_p)
		{
			if (json_array_append_new (resources_p, resource_p) == 0)
				{
					return true;
				}
			else
				{
					json_decref (resource_p);
				}
		}


	return false;
}


static json_t *PopulateResourceFromDataObject (collEnt_t * const entry_p,  rcComm_t *connection_p, apr_pool_t *pool_p)
{
	json_t *resource_p = json_object ();

	if (resource_p)
		{
			if (json_object_set_new (resource_p, "bytes", json_integer (entry_p -> dataSize)) == 0)
				{
					char *name_s = entry_p -> dataName;

					if (SetJSONString (resource_p, "path", name_s, pool_p))
						{
							const char dot = '.';
							char *dot_s = strrchr (name_s, dot);
							bool set_name_flag = false;

							if (dot_s)
								{
									/*
									 * Set the name as to the filename without the extension
									 */
									*dot_s = '\0';

									set_name_flag = SetJSONString (resource_p, "name", name_s, pool_p);

									*dot_s = dot;
								}
							else
								{
									set_name_flag = SetJSONString (resource_p, "name", name_s, pool_p);
								}

							if (set_name_flag)
								{
									char *checksum_s = GetChecksum (entry_p, connection_p, pool_p);

									if (checksum_s)
										{
											 SetJSONString (resource_p, "checksum", checksum_s, pool_p);
										}

									return resource_p;
								}		/* if (set_name == 0) */

						}		/* if (SetJSONString (resource_p, "path", name_s, pool_p)) */

				}		/* if (json_object_set_new (resource_p, "bytes", json_integer (entry_p -> dataSize)) == 0) */

			json_decref (resource_p);
		}		/* if (resource_p) */

	return NULL;
}


static json_t *PopulateResourceFromCollection (collEnt_t * const entry_p,  rcComm_t *connection_p, apr_pool_t *pool_p)
{
	json_t *resource_p = json_object ();

	if (resource_p)
		{
			char *name_s = strrchr (entry_p -> collName, '/');

			if (name_s)
				{
					++ name_s;

					if (*name_s == '\0')
						{
							name_s = NULL;
						}
				}

			if (!name_s)
				{
					name_s = entry_p -> collName;
				}

			if (SetJSONString (resource_p, "name", name_s, pool_p))
				{
					return resource_p;
				}

			json_decref (resource_p);
		}		/* if (resource_p) */

	return NULL;
}



static bool AddLicense (json_t *resource_p, const char *name_s, const char *url_s, apr_pool_t *pool_p)
{
	json_t *licenses_array_p = json_array ();

	if (licenses_array_p)
		{
			json_t *license_p = json_pack ("{s:s,s:s}", "name", name_s, "path", url_s);

			if (license_p)
				{
					if (json_array_append_new (licenses_array_p, license_p) == 0)
						{
							if (json_object_set_new (resource_p, "licenses", licenses_array_p) == 0)
								{
									return true;
								}
							else
								{
									ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to add licenses array to resource");
								}
						}
					else
						{
							json_decref (license_p);
							ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to append license object to array");
						}
				}
			else
				{
					ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create license object");
				}

			json_decref (licenses_array_p);
		}
	else
		{
			ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create licenses array");
		}

	return false;
}



/*
 * This MUST be lower-case and contain only alphanumeric characters along with “.”, “_” or “-” characters
 */

static char *ConvertFDCompliantName (char *name_s)
{
	char *c_p = name_s;

	for ( ; *c_p != '\0'; ++ c_p)
		{
			if (isupper (*c_p))
				{
					*c_p = tolower (*c_p);
				}
			else if (! (*c_p == '.') || (*c_p == '_') || (*c_p == '-'))
				{
					if (!isalnum (*c_p))
						{
							*c_p = '_';
						}
				}
		}

	return name_s;
}
