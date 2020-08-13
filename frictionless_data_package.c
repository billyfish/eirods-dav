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
#include "httpd.h"

#include "irods/rcConnect.h"

#include "jansson.h"


static const char * const S_DATA_PACKAGE_S = "datapackage.json";


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
					if (json_object_set_new (dp_p, "name", json_string (davrods_resource_p -> root_dir)) == 0)
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

					json_decref (dp_p);
				}

		}		/* if (bb_p) */




	return res_p;
}


int IsFDDataPackageRequest (const char *request_uri_s)
{
	int fd_data_package_flag = 0;

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


	return fd_data_package_flag;
}

