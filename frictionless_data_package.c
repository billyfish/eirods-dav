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

#include "apr_strings.h"

#include "frictionless_data_package.h"
#include "meta.h"
#include "repo.h"
#include "theme.h"

#include "httpd.h"
#include "http_protocol.h"

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

static bool AddResource (collEnt_t * const entry_p, json_t *resources_p, rcComm_t *connection_p, const char *data_package_root_path_s, apr_pool_t *pool_p);

static json_t *PopulateResourceFromDataObject (collEnt_t * const entry_p,  rcComm_t *connection_p, const char *data_package_root_path_s, apr_pool_t *pool_p);

static json_t *PopulateResourceFromCollection (collEnt_t * const entry_p,  rcComm_t *connection_p, const char *data_package_root_path_s, apr_pool_t *pool_p);

static bool AddLicense (json_t *resource_p, const char *name_s, const char *url_s, apr_pool_t *pool_p);

static bool AddAuthors (json_t *resource_p, const char *authors_s, apr_pool_t *pool_p);

static char *ConvertFDCompliantName (char *name_s);

static apr_status_t BuildDataPackage (json_t *data_package_p, const apr_table_t *metadata_list_p, const char *collection_name_s, const struct HtmlTheme *theme_p, apr_pool_t *pool_p);

static const char *GetRelativePath (const char *data_package_root_path_s, const char *collection_s, apr_pool_t *pool_p);

static char *GetMetadataValue (const char *full_key_s, const apr_table_t *metadata_table_p, apr_pool_t *pool_p);

/*
 * API definitions
 */


dav_error *DeliverFDDataPackage (const dav_resource *resource_p, ap_filter_t *output_p)
{
	dav_error *res_p = NULL;
	struct dav_resource_private *davrods_resource_p = (struct dav_resource_private *) resource_p -> info;
	apr_pool_t *pool_p = resource_p -> pool;
	request_rec *req_p = resource_p -> info -> r;
	apr_bucket_brigade *bb_p = apr_brigade_create (pool_p, output_p -> c -> bucket_alloc);

	ap_set_content_type (req_p, CONTENT_TYPE_JSON_S);

	if (bb_p)
		{
			json_t *dp_p = json_object ();

			if (dp_p)
				{
					char *collection_id_s = GetCollectionId (davrods_resource_p -> rods_path, davrods_resource_p -> rods_conn, pool_p);

					if (collection_id_s)
						{
							apr_table_t *metadata_p = GetMetadataAsTable (davrods_resource_p -> rods_conn, COLL_OBJ_T, collection_id_s, NULL, davrods_resource_p -> rods_env -> rodsZone, pool_p);

							if (metadata_p)
								{
									apr_status_t status;

									/* the local collection name */
									const char *collection_s = strrchr (davrods_resource_p -> rods_path, '/');

									if (collection_s)
										{
											/* move past the last slash */
											++ collection_s;

											/*
											 * Are we at the end of the string?
											 */
											if (*collection_s == '\0')
												{
													collection_s = NULL;
												}
										}

									status = BuildDataPackage (dp_p, metadata_p, collection_s, davrods_resource_p -> conf -> theme_p, pool_p);

									if (status == APR_SUCCESS)
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


															CacheDataPackage (collection_s, dp_s, davrods_resource_p -> rods_conn, pool_p);

															free (dp_s);
														}

												}
										}

								}		/* if (metadata_p) */

						}		/* if (collection_id_s) */

					json_decref (dp_p);
				}		/* if (dp_p) */

			apr_brigade_destroy (bb_p);
		}		/* if (bb_p) */

	return res_p;
}


static bool CacheDataPackageToDisk (const char *collection_s, const char *package_data_s, const char *output_path_s, apr_pool_t *pool_p)
{
	bool success_flag = false;

	return success_flag;
}


static bool CacheDataPackageToIRODS (const char *collection_s, const char *package_data_s, rcComm_t *rods_conn_p, apr_pool_t *pool_p)
{
	bool success_flag = false;
	dataObjInp_t input;
	openedDataObjInp_t handle;
	char *full_path_s = apr_pstrcat (pool_p, collection_s, S_DATA_PACKAGE_S, NULL);

	memset (&handle, 0, sizeof (openedDataObjInp_t));
	memset (&input, 0, sizeof (dataObjInp_t));

	rstrcpy (input.objPath, full_path_s, MAX_NAME_LEN);

	input.openFlags = O_WRONLY;
	handle.l1descInx = rcDataObjOpen (rods_conn_p, &input);

	if (handle.l1descInx >= 0)
		{
			bytesBuf_t buffer;
			int status;
			const int data_length = strlen (package_data_s);

			bzero (&buffer, sizeof (bytesBuf_t));

			buffer.len = data_length;
			buffer.buf = package_data_s;

			status = rcDataObjWrite (rods_conn_p, &handle, &buffer);
			if (status == data_length)
				{
					success_flag = true;
				}
			else
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_EGENERAL, pool_p, "Failed to write buffer for cached datapackage at \"%s\", %d bytes out of %d", full_path_s, status, data_length);
				}

			status = rcDataObjClose (rods_conn_p, &handle);
			if (status < 0)
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_EGENERAL, pool_p, "Failed to close cached datapackage at \"%s\"", full_path_s);
				}
		}
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_EGENERAL, pool_p, "Failed to open cache datapackage at \"%s\"", full_path_s);
		}

	return success_flag;
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

	if ((conf_p -> theme_p) && (conf_p -> theme_p -> ht_show_fd_data_packages_flag > 0))
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


static apr_status_t BuildDataPackage (json_t *data_package_p, const apr_table_t *metadata_table_p, const char *collection_name_s, const struct HtmlTheme *theme_p, apr_pool_t *pool_p)
{
	apr_status_t status = APR_SUCCESS;

	if (!apr_is_empty_table (metadata_table_p))
		{
			const char *license_name_key_s = "license";
			const char *license_name_value_s = NULL;

			const char *license_url_key_s = "license_url";
			const char *license_url_value_s = NULL;

			/*
			 * name
			 *
			 * A short url-usable (and preferably human-readable) name of the package. This MUST be lower-case and contain
			 * only alphanumeric characters along with “.”, “_” or “-” characters. It will function as a unique identifier
			 * and therefore SHOULD be unique in relation to any registry in which this package will be deposited (and
			 * preferably globally unique).
			 */
			const char *name_key_s = "name";
			char *name_value_s = NULL;

			const char *description_key_s = "description";
			char *description_value_s = NULL;

			const char *authors_key_s = "authors";
			char *authors_value_s = NULL;

			const char *title_key_s = "title";
			const char *title_value_s = NULL;

			const char *id_key_s = "id";
			const char *id_value_s = NULL;

			if (theme_p)
				{
					/*
					 * Get any key names that have been configured.
					 */
					if (theme_p -> ht_fd_resource_license_name_key_s)
						{
							license_name_key_s = theme_p -> ht_fd_resource_license_name_key_s;
						}

					if (theme_p -> ht_fd_resource_license_url_key_s)
						{
							license_url_key_s = theme_p -> ht_fd_resource_license_url_key_s;
						}

					if (theme_p -> ht_fd_resource_name_key_s)
						{
							name_key_s = theme_p -> ht_fd_resource_name_key_s;
						}

					if (theme_p -> ht_fd_resource_description_key_s)
						{
							description_key_s = theme_p -> ht_fd_resource_description_key_s;
						}

					if (theme_p -> ht_fd_resource_authors_key_s)
						{
							authors_key_s = theme_p -> ht_fd_resource_authors_key_s;
						}

					if (theme_p -> ht_fd_resource_title_key_s)
						{
							title_key_s = theme_p -> ht_fd_resource_title_key_s;
						}

					if (theme_p -> ht_fd_resource_id_key_s)
						{
							id_key_s = theme_p -> ht_fd_resource_id_key_s;
						}
				}		/* if (theme_p) */


			license_name_value_s = GetMetadataValue (license_name_key_s, metadata_table_p, pool_p);
			license_url_value_s = GetMetadataValue (license_url_key_s, metadata_table_p, pool_p);
			name_value_s = GetMetadataValue (name_key_s, metadata_table_p, pool_p);
			title_value_s = GetMetadataValue (title_key_s, metadata_table_p, pool_p);
			id_value_s = GetMetadataValue (id_key_s, metadata_table_p, pool_p);
			description_value_s = GetMetadataValue (description_key_s, metadata_table_p, pool_p);
			authors_value_s = GetMetadataValue (authors_key_s, metadata_table_p, pool_p);


			if (license_name_value_s && license_url_value_s)
				{
					if (!AddLicense (data_package_p, license_name_value_s, license_url_value_s, pool_p))
						{

						}
				}

			if (!name_value_s)
				{
					/* use the directory name */
					name_value_s = apr_pstrdup (pool_p, collection_name_s);
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

			if (description_value_s)
				{
					if (!SetJSONString (data_package_p, "description", description_value_s, pool_p))
						{

						}
				}

			if (authors_value_s)
				{
					if (!AddAuthors (data_package_p, authors_value_s, pool_p))
						{

						}
				}

		}		/* if (!apr_is_empty_table (metadata_table_p)) */
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, pool_p, "No metadata results");
		}

	return status;
}


static char *GetMetadataValue (const char *full_key_s, const apr_table_t *metadata_table_p, apr_pool_t *pool_p)
{
	const char *sep_s = ",";
	char *context_s = NULL;
	char *copied_full_key_s = apr_pstrdup (pool_p, full_key_s);
	char *key_s = apr_strtok (copied_full_key_s, sep_s, &context_s);
	char *value_s = NULL;

	while (key_s)
		{
			char *next_value_s = NULL;

			if (strcmp (key_s, " ") == 0)
				{
					next_value_s = " ";
				}
			else if (strcmp (key_s, ".") == 0)
				{
					next_value_s = ".";
				}
			else if (strcmp (key_s, "\\n") == 0)
				{

					next_value_s = "\n";
				}
			else
				{
					const IrodsMetadata *metadata_p = (const IrodsMetadata *) apr_table_get (metadata_table_p, key_s);

					if (metadata_p)
						{
							next_value_s = metadata_p -> im_value_s;
						}
				}

			if (next_value_s)
				{
					if (value_s)
						{
							char *new_value_s = apr_pstrcat (pool_p, value_s, next_value_s, NULL);

							if (new_value_s)
								{
									value_s = new_value_s;
								}
							else
								{

								}
						}		/* if (value_s) */
					else
						{
							value_s = apr_pstrdup (pool_p, next_value_s);
						}

				}

			key_s = apr_strtok (NULL, sep_s, &context_s);
		}		/* while (key_s) */

	return value_s;
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
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, pool_p, "No value set for key \"%s\"", key_s);
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
					collHandle_t collection_handle;
					int status;

					memset (&collection_handle, 0, sizeof (collHandle_t));

					// Open the collection
					status = rclOpenCollection (davrods_resource_p -> rods_conn, davrods_resource_p -> rods_path, RECUR_QUERY_FG | LONG_METADATA_FG, &collection_handle);

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
											 * FD Data Packages don't appear to add the directory entries to the resources part.
											 */
											if (coll_entry.objType != COLL_OBJ_T) // || (strcmp (davrods_resource_p -> rods_path, coll_entry.collName) != 0))
												{
													/*
													 * Add resource
													 */

													if (!AddResource (&coll_entry, resources_json_p, davrods_resource_p -> rods_conn, davrods_resource_p -> rods_path, resource_p -> pool))
														{
															status = -1;
														}

												}		/* if (strcmp (davrods_resource_p -> rods_path, coll_entry.collName) != 0) */

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


static bool AddResource (collEnt_t * const entry_p, json_t *resources_p, rcComm_t *connection_p, const char *data_package_root_path_s, apr_pool_t *pool_p)
{
	json_t *resource_p = NULL;

	if (entry_p -> objType == COLL_OBJ_T)
		{
			resource_p = PopulateResourceFromCollection (entry_p, connection_p, data_package_root_path_s, pool_p);
		}
	else if (entry_p -> objType == DATA_OBJ_T)
		{
			resource_p = PopulateResourceFromDataObject (entry_p, connection_p, data_package_root_path_s, pool_p);
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


static json_t *PopulateResourceFromDataObject (collEnt_t * const entry_p, rcComm_t *connection_p, const char *data_package_root_path_s, apr_pool_t *pool_p)
{
	json_t *resource_p = json_object ();

	if (resource_p)
		{
			if (json_object_set_new (resource_p, "bytes", json_integer (entry_p -> dataSize)) == 0)
				{
					const char *relative_collection_s = GetRelativePath (data_package_root_path_s, entry_p -> collName, pool_p);
					char *name_s = NULL;

					if (strcmp (relative_collection_s, entry_p -> collName) == 0)
						{
							name_s = entry_p -> dataName;
						}
					else
						{
							name_s = apr_pstrcat (pool_p, relative_collection_s, "/",  entry_p -> dataName, NULL);
						}


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


/**
 * Get the relative path for a collection
 *
 * @param data_package_root_path_s The iRODS root path that we are using
 * @param collection_s The collection path
 * @return The relative path for the given collection.
 */
static const char *GetRelativePath (const char *data_package_root_path_s, const char *collection_s, apr_pool_t *pool_p)
{
	const char *relative_path_s = NULL;

	if (strcmp (data_package_root_path_s, collection_s) != 0)
		{
			const size_t data_package_root_path_length = strlen (data_package_root_path_s);

			if (strncmp (data_package_root_path_s, collection_s, data_package_root_path_length) == 0)
				{
					relative_path_s = collection_s + data_package_root_path_length;

					/*
					 * move past any trailing slash for the trailing slash
					 */
					if (*relative_path_s == '/')
						{
							++ relative_path_s;
						}
				}
			else
				{
					ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to determine subcollection relative path for \"%s\" and \"%s\"", data_package_root_path_s, collection_s);

				}

		}		/* if (strcmp (data_package_root_path_s, collection_s) != 0) */
	else
		{
			relative_path_s = data_package_root_path_s;
		}

	return relative_path_s;
}


static json_t *PopulateResourceFromCollection (collEnt_t * const entry_p, rcComm_t *connection_p, const char *data_package_root_path_s, apr_pool_t *pool_p)
{
	json_t *resource_p = json_object ();

	if (resource_p)
		{
			//char *name_s = entry_p -> collName;
			const char *name_s = GetRelativePath (data_package_root_path_s, entry_p -> collName, pool_p);

			if (name_s)
				{
					if (*name_s == '/')
						{
							++ name_s;

							if (*name_s == '\0')
								{
									name_s = NULL;
								}
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
 * Split the value by commas
 */
static bool AddAuthors (json_t *resource_p, const char *authors_s, apr_pool_t *pool_p)
{
	json_t *authors_array_p = json_array ();

	if (authors_array_p)
		{
			char *copied_authors_s = apr_pstrdup (pool_p, authors_s);
			char *state_p;
			char *author_s = apr_strtok (copied_authors_s, ",", &state_p);

			while (author_s)
				{
					/* Scroll past any initial whitespace */
					while (isspace (*author_s))
						{
							++ author_s;
						}

					if (*author_s != '\0')
						{
							json_t *author_p = json_pack ("{s:s,s:s}", "title", author_s, "role", "author");

							if (author_p)
								{
									if (json_array_append_new (authors_array_p, author_p) != 0)
										{
											json_decref (author_p);
											ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to append author \"%s\" to array", author_s);
										}
								}

							author_s = apr_strtok (NULL, ",", &state_p);
						}
					else
						{
							author_s = NULL;
						}
				}

			if (json_object_set_new (resource_p, "contributors", authors_array_p) == 0)
				{
					return true;
				}
			else
				{
					ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to add contributors array to resource");
				}

			json_decref (authors_array_p);
		}		/* if (authors_array_p) */
	else
		{
			ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create authors array");
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
