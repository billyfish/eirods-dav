/**
 * \file
 * \brief     Common includes, functions and variables.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016, Utrecht University
 *
 * This file is part of Davrods.
 *
 * Davrods is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Davrods is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Davrods.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "common.h"
#include "config.h"
#include "prop.h"
#include "propdb.h"
#include "repo.h"
#include "auth.h"
#include "curl_util.h"
#include "meta.h"


#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK
#include "lock_local.h"
#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

#include <stdlib.h>

#include <http_request.h>
#include <http_protocol.h>
#include <ap_provider.h>

#include "apr_buckets.h"
#include "apr_escape.h"

#include <irods/rodsClient.h>




APLOG_USE_MODULE(davrods);


static char *ParseURIForVariables (const char *uri_s, char *current_id_s, rcComm_t *connection_p, request_rec *req_p, apr_pool_t *pool_p);


// Common utility functions {{{

const char *get_rods_error_msg(int rods_error_code) {
    char *submsg = NULL;
    return rodsErrorName(rods_error_code, &submsg);
}


// }}}
// DAV provider definition and registration {{{

#ifdef DAVRODS_ENABLE_PROVIDER_NOLOCKS

// The no-locking provider limits the DAV protocol to version 1. This
// can cause a slight increase in performance, but may prevent certain
// clients from connecting in read/write mode (e.g. Apple OS X).
const dav_provider davrods_dav_provider_nolocks = {
    &davrods_hooks_repository,
    &davrods_hooks_propdb,
    NULL, // locks   - disabled.
    NULL, // vsn     - unimplemented.
    NULL, // binding - unimplemented.
    NULL, // search  - unimplemented.

    NULL  // context - not needed.
};

#endif /* DAVRODS_ENABLE_PROVIDER_NOLOCKS */

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK

const dav_provider davrods_dav_provider_locallock = {
    &davrods_hooks_repository,
    &davrods_hooks_propdb,
    &davrods_hooks_locallock,
    NULL, // vsn     - unimplemented.
    NULL, // binding - unimplemented.
    NULL, // search  - unimplemented.

    NULL  // context - not needed.
};

#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

void davrods_dav_register(apr_pool_t *p) {

    // Register the namespace URIs.
    dav_register_liveprop_group(p, &davrods_liveprop_group);

    // Register the DAV providers.

#ifdef DAVRODS_ENABLE_PROVIDER_NOLOCKS
    dav_register_provider(p, DAVRODS_PROVIDER_NAME "-nolocks",   &davrods_dav_provider_nolocks);
#endif /* DAVRODS_ENABLE_PROVIDER_NOLOCKS */

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK
    dav_register_provider(p, DAVRODS_PROVIDER_NAME "-locallock", &davrods_dav_provider_locallock);
#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

#if !defined(DAVRODS_ENABLE_PROVIDER_NOLOCKS)   \
 && !defined(DAVRODS_ENABLE_PROVIDER_LOCALLOCK)
    #error No DAV provider enabled. Please define one of the DAVRODS_ENABLE_PROVIDER_.* switches.
#endif
}






void CloseBucketsStream (apr_bucket_brigade *bucket_brigade_p)
{
	apr_bucket *bucket_p = apr_bucket_eos_create (bucket_brigade_p -> bucket_alloc);

	if (bucket_p)
		{
			APR_BRIGADE_INSERT_TAIL (bucket_brigade_p, bucket_p);
		}
}


apr_status_t PrintBasicStringToBucketBrigade (const char *value_s, apr_bucket_brigade *brigade_p, request_rec *req_p, const char *file_s, const int line)
{
	apr_status_t status = apr_brigade_puts (brigade_p, NULL, NULL, value_s);

	if (status != APR_SUCCESS)
		{
			ap_log_rerror (file_s, line, APLOG_MODULE_INDEX, APLOG_ERR, status, req_p, "Failed to print \"%s\"", value_s);
		}

	return status;
}


apr_status_t PrintFileToBucketBrigade (const char *filename_s, apr_bucket_brigade *brigade_p, request_rec *req_p, const char *file_s, const int line)
{
	apr_status_t status = APR_EGENERAL;
	FILE *in_f = fopen (filename_s, "r");

	if (in_f)
		{
			int loop = 1;

			while (loop)
				{
					#define BUFFER_SIZE (1025)

					char buffer_s [BUFFER_SIZE];
					size_t num_read	= fread (buffer_s, 1, BUFFER_SIZE - 1, in_f);

					if ((num_read == BUFFER_SIZE) || (ferror (in_f) == 0))
						{
							status = apr_brigade_write (brigade_p, NULL, NULL, buffer_s, num_read);

							if (status != APR_SUCCESS)
								{
									ap_log_rerror (file_s, line, APLOG_MODULE_INDEX, APLOG_ERR, status, req_p, "Failed to store %s from file", buffer_s, filename_s);
									loop = 0;
								}
						}
					else
						{
							loop = 0;
							ap_log_rerror (file_s, line, APLOG_MODULE_INDEX, APLOG_ERR, status, req_p, "Failed to get contents of %s", filename_s);
						}

					if (feof (in_f) != 0)
						{
							loop = 0;
						}

				}

			fclose (in_f);
		}
	else
		{
			ap_log_rerror (file_s, line, APLOG_MODULE_INDEX, APLOG_ERR, status, req_p, "Failed to open %s", filename_s);
		}

	return status;
}



apr_status_t PrintWebResponseToBucketBrigade (const char *uri_s, char *current_id_s, apr_bucket_brigade *brigade_p, rcComm_t *connection_p, request_rec *req_p, const char *file_s, const int line)
{
	apr_status_t status = APR_SUCCESS;
	char *parsed_uri_s = ParseURIForVariables (uri_s, current_id_s, connection_p, req_p, req_p -> pool);

	if (parsed_uri_s)
		{
			char *result_s = SimpleCallGetRequest (req_p, req_p -> pool, parsed_uri_s);

			if (result_s)
				{
					PrintBasicStringToBucketBrigade (result_s, brigade_p, req_p, __FILE__, __LINE__);
				}
		}

	return status;
}



static char *ParseURIForVariables (const char *uri_s, char *current_id_s, rcComm_t *connection_p, request_rec *req_p, apr_pool_t *pool_p)
{
	apr_status_t status;
	char *result_s = NULL;
	char *current_var_start_p;
	const char *prev_p = uri_s;
	const char * const var_start_s = "@{";
	const char * const var_end_s = "}";
	const size_t start_length = strlen (var_start_s);
	const size_t end_length = strlen (var_end_s);
	const char *metadata_prefix_s = "metadata:";
	const size_t metadata_prefix_length = strlen (metadata_prefix_s);
	apr_bucket_brigade *buffer_p = apr_brigade_create (pool_p, req_p -> connection -> bucket_alloc);


	if (buffer_p)
		{
			bool success_flag = true;
			apr_array_header_t *metadata_array_p = NULL;

			while (success_flag && ((current_var_start_p = strstr (prev_p, var_start_s)) != NULL))
				{
					char *current_var_end_p = strstr (current_var_start_p, var_end_s);

					if (current_var_end_p)
						{
							char *current_var_s = NULL;

							current_var_start_p += start_length;

							current_var_s = apr_pstrndup (pool_p, current_var_start_p, current_var_end_p - current_var_start_p);

							if (current_var_s)
								{
									status = apr_brigade_write (buffer_p, NULL, NULL, prev_p, current_var_start_p - start_length - prev_p);

									if (status == APR_SUCCESS)
										{
											if (strcmp (current_var_s, "id") == 0)
												{
													if (current_id_s)
														{
															if ((status = apr_brigade_puts (buffer_p, NULL, NULL, current_id_s)) != APR_SUCCESS)
																{

																}		/* if ((status = apr_brigade_puts (buffer_p, NULL, NULL, current_id_s)) != APR_SUCCESS) */
														}
												}		/*( if (strcmp (current_var_s, "id") == 0) */
											else if (strncmp (current_var_s, metadata_prefix_s, metadata_prefix_length) == 0)
												{
													char *metadata_key_s = current_var_s + metadata_prefix_length;

													if (metadata_key_s)
														{
															/* We have a metadata key to search for */
															if (!metadata_array_p)
																{
																	metadata_array_p = GetMetadataForId (current_id_s, connection_p, req_p, pool_p);
																}

															if (metadata_array_p)
																{
																	const int last_index = metadata_array_p -> nelts;

																	if (last_index >= 0)
																		{
																			int i;

																			for (i = 0; i < last_index; ++ i)
																				{
																					const IrodsMetadata *metadata_p = APR_ARRAY_IDX (metadata_array_p, i, IrodsMetadata *);

																					if (strcmp (metadata_key_s, metadata_p -> im_key_s) == 0)
																						{
																							status = apr_brigade_puts (buffer_p, NULL, NULL, metadata_p -> im_value_s);
																						}

																				}		/* for (i = 0; i < last_index; ++ i */

																		}		/* if (size > 0) */

																}		/* if (metadata_array_p) */

														}		/* if (metadata_key_s) */

												}		/* else if (strncmp (current_var_s, metadata_prefix_s, metadata_prefix_length) == 0) */
											else
												{

												}


											success_flag = (status == APR_SUCCESS);
										}		/* if (status == APR_SUCCESS) */


								}		/* if (current_var_s) */

							prev_p = current_var_end_p + end_length;
						}		/* if (current_var_end_p) */

				}		/* while (success_flag && ((current_var_start_p = strstr (prev_p, var_start_s)) != NULL)) */


			if (success_flag)
				{
					size_t length;

					status = apr_brigade_puts (buffer_p, NULL, NULL, prev_p);

					CloseBucketsStream (buffer_p);

					apr_brigade_pflatten (buffer_p, &result_s, &length, pool_p);

					/*
					 * Sometimes there is garbage at the end of this, and I don't know which apr_brigade_...
					 * method I need to get the terminating '\0' so have to do it explicitly.
					 */
					if (* (result_s + length) != '\0')
						{
							* (result_s + length) = '\0';
						}

				}

		}		/* if (buffer_p) */


	return result_s;
}


rcComm_t *GetIRODSConnectionFromRequest (request_rec *req_p)
{
	rcComm_t *connection_p = NULL;
	davrods_dir_conf_t *conf_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);

	if (conf_p)
		{
			// Obtain iRODS connection.
			apr_pool_t *pool_p = GetDavrodsMemoryPool (req_p);

			if (pool_p)
				{
					const char *username_s = NULL;
					const char *password_s = NULL;
					const char *hash_s = NULL;
					apr_status_t status = GetSessionAuth (req_p, &username_s, &password_s, &hash_s);

					if (status == APR_SUCCESS)
						{
							rcComm_t *pool_connection_p = GetIRODSConnectionFromPool (pool_p);

							if (pool_connection_p)
								{
									/*
									 * check that the username for the cached connection
									 * matches the one on the request.
									 */
									if (pool_connection_p -> clientUser.userName && username_s)
										{
											if (strcmp (pool_connection_p -> clientUser.userName, username_s) == 0)
												{
													connection_p = pool_connection_p;
												}
											else
												{
													ap_log_rerror (APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, req_p, "usernames differ so won't use cached connection");
												}
										}
									else
										{
											ap_log_rerror (APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, req_p, "missing a username so can't check cached connection");
										}

								}		/* if (connection_p) */
							else
								{
									ap_log_rerror (APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, req_p, "No existing connection");
								}


							if (!connection_p)
								{
									/*
									 * If we are running behind a proxy, the iRODS connection in the pool
									 * can sometimes get lost. So we check to see whether there are any
									 * valid authentication authentication details in the request's session.
									 */
									if (username_s && password_s)
										{
											status = GetIRodsConnection (req_p, &connection_p, username_s, password_s);

											if (status != APR_SUCCESS)
												{

												}
										}
								}


						}		/* if (status == APR_SUCCESS) */
					else
						{
							ap_log_rerror (APLOG_MARK, APLOG_DEBUG, APR_EGENERAL, req_p, "Failed to get session variables");

						}

					if (!connection_p)
						{
							connection_p = GetIRODSConnectionForPublicUser (req_p, pool_p, conf_p);
							username_s = conf_p -> davrods_public_username_s;
						}

				}

		}		/* if (conf_p) */

	return connection_p;
}


rcComm_t *GetIRODSConnectionForPublicUser (request_rec *req_p, apr_pool_t *davrods_pool_p, davrods_dir_conf_t *conf_p)
{
	rcComm_t *connection_p = NULL;

	/*
   * For publicly-accessible iRODS instances, check_rods will never have been called, so we'll need
   * to get the memory pool and iRODS connection for the public user.
   */
	if (conf_p -> davrods_public_username_s)
		{
			authn_status status = GetIRodsConnection (req_p, &connection_p, conf_p -> davrods_public_username_s, conf_p -> davrods_public_password_s ? conf_p -> davrods_public_password_s : "");

			if (status != AUTH_GRANTED)
				{
					ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_ECONNREFUSED, req_p, "error %d: Failed to connect to iRODS as public user \"%s\"", status, conf_p -> davrods_public_username_s);

					WHISPER ("GetIRodsConnection failed for anonymous user");
				}
		}

	return connection_p;
}


rcComm_t *GetIRODSConnectionFromPool (apr_pool_t *pool_p)
{
	rcComm_t *connection_p = NULL;
	void *ptr = NULL;
	const char *key_s = GetConnectionKey ();
	apr_status_t status = apr_pool_userdata_get (&ptr, key_s, pool_p);

	if (status == APR_SUCCESS)
		{
			if (ptr)
				{
					connection_p = (rcComm_t *) ptr;
				}
			else
				{
					status = apr_pool_userdata_get (&ptr, GetUsernameKey (), pool_p);
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, pool_p, "username \"%s\"", (char *) ptr);

					status = apr_pool_userdata_get (&ptr, GetDavrodsMemoryPoolKey (), pool_p);
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, pool_p, "mem pool \"%X\"", ptr);

				}
		}
	else
		{

		}

	return connection_p;
}


rodsEnv *GetRodsEnvFromPool (apr_pool_t *pool_p)
{
	rodsEnv *env_p = NULL;
	void *ptr = NULL;
	apr_status_t status = apr_pool_userdata_get (&ptr, GetRodsEnvKey (), pool_p);

	if (status == APR_SUCCESS)
		{
			if (ptr)
				{
					env_p = (rodsEnv *) ptr;
				}
		}

	return env_p;
}


const char *GetUsernameFromPool (apr_pool_t *pool_p)
{
	const char *username_s = NULL;
	void *ptr = NULL;
	apr_status_t status = apr_pool_userdata_get (&ptr, GetUsernameKey (), pool_p);

	if (status == APR_SUCCESS)
		{
			if (ptr)
				{
					username_s = (const char *) ptr;
				}
		}

	return username_s;
}



apr_table_t *MergeAPRTables (apr_table_t *table1_p, apr_table_t *table2_p, apr_pool_t *pool_p)
{
	apr_table_t *merged_table_p = NULL;

	if (table2_p)
		{
			if (table1_p)
				{
					merged_table_p = apr_table_overlay (pool_p, table1_p, table2_p);
				}
			else
				{
					merged_table_p = table2_p;
				}
		}
	else
		{
			if (table1_p)
				{
					merged_table_p = table1_p;
				}
			else
				{
					merged_table_p = NULL;
				}
		}

	return merged_table_p;
}


const char *GetParameterValue (apr_table_t *params_p, const char * const param_s, apr_pool_t *pool_p)
{
	const char *value_s = NULL;
	const char * const raw_value_s = apr_table_get (params_p, param_s);
	const char *forbid_s = NULL;
	const char *reserved_s = NULL;

	if (raw_value_s)
		{
			value_s = apr_punescape_url (pool_p, raw_value_s, forbid_s, reserved_s, 1);
		}

	return value_s;
}


char *GetChecksum (collEnt_t *coll_entry_p, rcComm_t *connection_p, apr_pool_t *pool_p)
{
	char *checksum_s = NULL;
	dataObjInp_t obj_inp;
	const char *full_path_s = apr_pstrcat (pool_p, coll_entry_p -> collName, "/", coll_entry_p -> dataName, NULL);
	size_t length = strlen (full_path_s);
	int status;

	memset (&obj_inp, 0, sizeof (dataObjInp_t));

	if (length >= MAX_NAME_LEN)
		{
			length = MAX_NAME_LEN - 1;
		}

	strncpy (obj_inp.objPath, full_path_s, length);

	status = rcDataObjChksum (connection_p, &obj_inp, &checksum_s);

	if (status >= 0)
		{
			coll_entry_p -> chksum = checksum_s;
		}
	else
		{
			ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to rcDataObjChksum for \"%s\"", full_path_s);
		}

	return checksum_s;
}

