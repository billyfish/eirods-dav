/*
 ** Copyright 2018 The Earlham Institute
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
 * rest.c
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#include <stdlib.h>
#include <string.h>

#define ALLOCATE_REST_CONSTANTS (1)
#include "rest.h"

#include "apr_escape.h"
#include "apr_tables.h"
#include "apr_strings.h"
#include "util_script.h"
#include "http_protocol.h"

#include "jansson.h"


#include "meta.h"
#include "auth.h"
#include "common.h"
#include "listing.h"
#include "repo.h"
#include "theme.h"

#include "debug.h"

#include "irods/mvUtil.h"

struct APICall;

typedef struct APICall
{
	const char *ac_action_s;
	int (*ac_callback_fn) (const struct APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);
} APICall;


/*
 * STATIC DECLARATIONS
 */

static int SearchMetadata (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int GetMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int AddMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int EditMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int DeleteMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int GetMatchingMetadataKeys (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int GetMatchingMetadataValues (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int GetInformationForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static int ListInformationForEntries (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);


static const char *GetIdParameter (apr_table_t *params_p, request_rec *req_p, rcComm_t *rods_connection_p, apr_pool_t *pool_p);


static apr_status_t EasyModifyMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s, const char *command_s);


static apr_status_t ModifyMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s, const char *command_s, const char *new_key_s, const char *new_value_s, const char *new_units_s);


static char *GetModValue (apr_table_t *params_p, const char *param_key_s, const char *value_prefix_s, const char *old_key_s, apr_pool_t *pool_p);


static apr_status_t AddDecodedJSONResponse (const APICall *call_p, apr_status_t status, const char *id_s, request_rec *req_p);



static OutputFormat GetRequestedOutputFormat (apr_table_t *params_p, apr_pool_t *pool_p, OutputFormat default_format);


static apr_status_t RunMetadataQuery (const int *where_columns_p, const char **where_values_ss, const SearchOperator *ops_p, const size_t num_where_columns, const int *select_columns_p, json_t *res_array_p, request_rec *req_p, davrods_dir_conf_t *config_p);


static bool GetSearchParameters (const char **key_ss, const char **value_ss, SearchOperator *op_p, apr_table_t *params_p, request_rec *req_p);


static int GetVirtualListingAsHTML (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);

static int GetSearchMetadataAsHTML (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);

static const char *GetFullPath (const char *path_s, request_rec *req_p, apr_pool_t *pool_p);

static void SetMimeTypeForOutputFormat (request_rec *req_p, const OutputFormat fmt);

/*
 * STATIC VARIABLES
 */

static APICall S_REST_API_ACTIONS_P [] =
{
	{ REST_METADATA_SEARCH_S, SearchMetadata },
	{ REST_METADATA_GET_S, GetMetadataForEntry },
	{ REST_METADATA_ADD_S, AddMetadataForEntry },
	{ REST_METADATA_DELETE_S, DeleteMetadataForEntry },
	{ REST_METADATA_EDIT_S, EditMetadataForEntry },
	{ REST_METADATA_MATCHING_KEYS_S, GetMatchingMetadataKeys },
	{ REST_METADATA_MATCHING_VALUES_S, GetMatchingMetadataValues },

	{ REST_GET_INFO_S, GetInformationForEntry },
	{ REST_LIST_S, ListInformationForEntries },

	{ NULL, NULL }
};


static APICall S_VIEW_ACTIONS_P [] =
{
	{ VIEW_LIST_S, GetVirtualListingAsHTML },
	{ VIEW_SEARCH_S, GetSearchMetadataAsHTML },

	{ NULL, NULL }
};


static const char * const S_HANDLER_NAME_S = "davrods-rest-handler";
static const char * const S_HANDLER_SET_VALUE_S = "true";

/*
 * API DEFINITIONS
 */

int EIRodsDavFixUps (request_rec *req_p)
{
	int res = DECLINED;

  /* First off, we need to check if this is a call for the "example-handler" handler.
   * If it is, we accept it and do our things, if not, we simply return DECLINED,
   * and the server will try somewhere else.
   */
	const char *handler_s = req_p -> handler;


	if (handler_s && (strcmp (handler_s, S_HANDLER_NAME_S) == 0))
		{
			if (req_p -> path_info)
				{
					davrods_dir_conf_t *config_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);

					if (config_p -> davrods_api_path_s)
						{
							const size_t api_path_length = strlen (config_p -> davrods_api_path_s);

							if (strncmp (config_p -> davrods_api_path_s, req_p -> path_info, api_path_length) == 0)
								{
									apr_table_set (req_p -> notes, handler_s, S_HANDLER_SET_VALUE_S);
									res = OK;
								}
						}

					if (res != OK)
						{
							if (config_p -> eirods_dav_views_path_s)
								{
									const size_t views_path_length = strlen (config_p -> eirods_dav_views_path_s);

									if (strncmp (config_p -> eirods_dav_views_path_s, req_p -> path_info, views_path_length) == 0)
										{
											apr_table_set (req_p -> notes, handler_s, S_HANDLER_SET_VALUE_S);
											res = OK;
										}
								}
						}
				}
		}

	return res;
}


int EIRodsDavAPIHandler (request_rec *req_p)
{
	int res = DECLINED;


  /* First off, we need to check if this is a call for the "example-handler" handler.
   * If it is, we accept it and do our things, if not, we simply return DECLINED,
   * and the server will try somewhere else.
   */
	const char *handler_s = req_p -> handler;
	if (handler_s)
		{
			bool handle_flag = (strcmp (handler_s, "davrods-rest-handler") == 0);

			if (!handle_flag)
				{
					/*
					 * The DAV handler claims any request for itself by setting the handler to
					 * "dav-handler" even when it shouldn't
					 *
					 */
					if (strcmp (handler_s, "dav-handler") == 0)
						{
							const char *value_s = apr_table_get (req_p -> notes, S_HANDLER_NAME_S);

							if (value_s && (strcmp (value_s, S_HANDLER_SET_VALUE_S) == 0))
								{
									req_p -> handler = S_HANDLER_NAME_S;
									handle_flag = true;
								}
						}
				}

			if (handle_flag)
				{
					bool processed_flag = false;
					apr_table_t *params_p = NULL;

					davrods_dir_conf_t *config_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);


					/*
					 * Normally we would check if this is a call for the ei-rods-dav rest handler,
					 * but dav-handler will have gotten there first. So check it against our path
					 * and see if we are interested in it.
					 * If it is, we accept it and do our things, it not, we simply return DECLINED,
					 * and Apache will try somewhere else.
					 */
					if (req_p -> method_number == M_GET)
						{
							ap_args_to_table (req_p, &params_p);
							processed_flag = true;
						}
					else if (req_p -> method_number == M_POST)
						{
							apr_array_header_t *key_value_pairs_p = NULL;
							int local_res = ap_parse_form_data (req_p, NULL, &key_value_pairs_p, -1, HUGE_STRING_LEN);

							if ((local_res == OK) && (key_value_pairs_p))
								{
									apr_pool_t *pool_p = req_p -> pool;

									params_p = apr_table_make (pool_p, 1);

									if (params_p)
										{
									    while (key_value_pairs_p && !apr_is_empty_array (key_value_pairs_p))
									    	{
									        ap_form_pair_t *pair_p = (ap_form_pair_t *) apr_array_pop (key_value_pairs_p);
									        apr_size_t size;
									        apr_off_t length;
									        char *value_s = NULL;
									        char *key_s = apr_pstrdup (req_p -> pool, pair_p -> name);

									        if (key_s)
									        	{
											        /*
											         * Get the length of the value
											         */
											        apr_brigade_length (pair_p -> value, 1, &length);
											        size = (apr_size_t) length;


											        value_s = apr_palloc (req_p -> pool, size + 1);

											        if (value_s)
											        	{
																	apr_brigade_flatten (pair_p -> value, value_s, &size);

																	if (* (value_s + size) != '\0')
																		{
																			* (value_s + size) = '\0';
																		}

																	/*
																	 * Since both the key and value have already been copied
																	 * we can add them to the table directly rather than it
																	 * storing copies of these values.
																	 */
																	apr_table_addn (params_p, key_s, value_s);
											        	}

									        	}		/* if (key_s) */

									    	}		/* while (key_value_pairs_p && !apr_is_empty_array (key_value_pairs_p)) */

									  	processed_flag = true;
										}		/* if (params_p) */

								}		/* if ((res == OK) && (key_value_pairs_p)) */

							processed_flag = true;
						}

					if (processed_flag)
						{
							char *davrods_path_s = NULL;

							processed_flag = false;

							if (req_p -> path_info)
								{
									if (config_p -> davrods_api_path_s)
										{
											const size_t api_path_length = strlen (config_p -> davrods_api_path_s);

											if (strncmp (config_p -> davrods_api_path_s, req_p -> path_info, api_path_length) == 0)
												{
													/*
													 * Parse the uri from req_p -> path_info to get the API call
													 */
													const APICall *call_p = S_REST_API_ACTIONS_P;

													const char *path_s = (req_p -> path_info) + api_path_length;

													/* Get the Location path where davrods is hosted */
													const char *ptr = strstr (req_p -> uri, config_p -> davrods_api_path_s);

													if (ptr)
														{
															davrods_path_s = apr_pstrmemdup (req_p -> pool, req_p -> uri, ptr - (req_p -> uri));
														}

													while ((call_p != NULL) && (call_p -> ac_action_s != NULL))
														{
															size_t l = strlen (call_p -> ac_action_s);

															if (strncmp (path_s, call_p -> ac_action_s, l) == 0)
																{
																	res = call_p -> ac_callback_fn (call_p, req_p, params_p, config_p, davrods_path_s);

																	if (res == OK)
																		{
																			ap_set_content_type (req_p, "application/json");
																		}

																	/* force exit from loop */
																	call_p = NULL;
																}
															else
																{
																	++ call_p;
																}

														}		/* while (call_p -> ac_action_s != NULL) */

													processed_flag = true;

												}		/* if (strncmp (config_p -> davrods_api_path_s, req_p -> path_info, api_path_length) == 0) */

										}		/* if (config_p -> davrods_api_path_s) */


									if (!processed_flag)
										{
											if (config_p -> eirods_dav_views_path_s)
												{
													const size_t views_path_length = strlen (config_p -> eirods_dav_views_path_s);

													if (strncmp (config_p -> eirods_dav_views_path_s, req_p -> path_info, views_path_length) == 0)
														{
															const APICall *call_p = S_VIEW_ACTIONS_P;
															const char *path_s = (req_p -> path_info) + views_path_length;

															/* Get the Location path where davrods is hosted */
															const char *ptr = strstr (req_p -> uri, config_p -> eirods_dav_views_path_s);

															if (ptr)
																{
																	davrods_path_s = apr_pstrmemdup (req_p -> pool, req_p -> uri, ptr - (req_p -> uri));
																}

															while ((call_p != NULL) && (call_p -> ac_action_s != NULL))
																{
																	size_t l = strlen (call_p -> ac_action_s);

																	if (strncmp (path_s, call_p -> ac_action_s, l) == 0)
																		{
																			res = call_p -> ac_callback_fn (call_p, req_p, params_p, config_p, davrods_path_s);

																			if (res == OK)
																				{
																					ap_set_content_type (req_p, "text/html");
																				}

																			/* force exit from loop */
																			call_p = NULL;
																		}
																	else
																		{
																			++ call_p;
																		}

																}		/* while (call_p -> ac_action_s != NULL) */


														}		/* if (strncmp (config_p -> eirods_dav_views_path_s, req_p -> path_info, views_path_length) == 0) */

												}		/* if (config_p -> eirods_dav_views_path_s) */

										}		/* if (!processed_flag) */

								}		/* if (req_p -> path_info) */


							if (!apr_is_empty_table (params_p))
								{
									apr_table_clear (params_p);
								}

						}		/* if (processed_flag) */

				}		/* if handler_flag) */

		}		/* if (handler_s) */


	return res;
}


const char *GetMinorId (const char *id_s)
{
	const char *minor_id_s = NULL;

	if (id_s)
		{
			const char *value_s = strchr (id_s, '.');

			if (value_s)
				{
					++ value_s;

					if (*value_s != '\0')
						{
							minor_id_s = value_s;
						}
				}
		}

	return minor_id_s;
}


IRodsObjectNode *GetMatchingIds (request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p)
{
	apr_pool_t *pool_p = req_p -> pool;
	IRodsObjectNode *root_node_p = NULL;
	rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

	if (rods_connection_p)
		{
			const char * const ids_s = GetParameterValue (params_p, "ids", pool_p);

			if (ids_s)
				{
					char *copied_ids_s = apr_pstrdup (pool_p, ids_s);

					if (copied_ids_s)
						{
							const char *sep_s = " ,";
							char *id_s = apr_strtok (copied_ids_s, sep_s, &copied_ids_s);
							IRodsObjectNode *current_node_p = NULL;

							while (id_s)
								{
									IRodsObjectNode *node_p = GetIRodsObjectNodeForId (id_s, rods_connection_p, pool_p);

									if (node_p)
										{
											if (current_node_p)
												{
													current_node_p -> ion_next_p = node_p;
												}
											else
												{
													root_node_p = node_p;
												}

											current_node_p = node_p;
										}

									id_s = apr_strtok (NULL, sep_s, &copied_ids_s);
								}		/* while (id_s) */


						}		/* if (copied_ids_s) */

				}		/* if (ids_s) */

		}		/* if (rods_connection_p) */


	if (root_node_p)
		{
			SortIRodsObjectNodeListIntoDirectoryOrder (root_node_p);
		}


	return root_node_p;
}


static int GetVirtualListingAsHTML (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;

	rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

	if (rods_connection_p)
		{
			IRodsObjectNode *root_node_p = GetMatchingIds (req_p, params_p, config_p);
			apr_pool_t *pool_p = req_p -> pool;
			char *result_s = NULL;
			apr_size_t result_length = 0;
			apr_bucket_brigade *bucket_brigade_p = apr_brigade_create (pool_p, req_p -> connection -> bucket_alloc);

			char *relative_uri_s = apr_pstrcat (pool_p, "the matching values", NULL);
			char *marked_up_relative_uri_s = apr_pstrcat (pool_p, "the matching values for the listing", NULL);

			const char *escaped_zone_s = config_p -> theme_p -> ht_zone_label_s ? config_p -> theme_p -> ht_zone_label_s : ap_escape_html (pool_p, config_p -> rods_zone);

			apr_status_t apr_status = PrintAllHTMLBeforeListing (NULL, escaped_zone_s, relative_uri_s, davrods_path_s, marked_up_relative_uri_s, NULL, rods_connection_p -> clientUser.userName, config_p, req_p, bucket_brigade_p, pool_p);


			char *metadata_root_link_s = apr_pstrcat (pool_p, davrods_path_s, config_p -> davrods_api_path_s, REST_METADATA_SEARCH_S, NULL);

			const char *exposed_root_s = GetRodsExposedPath (req_p);

			IRodsConfig irods_config;

			apr_status = SetIRodsConfig (&irods_config, exposed_root_s, davrods_path_s, metadata_root_link_s);


			if (root_node_p)
				{
					IRodsObjectNode *node_p = root_node_p;
					unsigned int i;

					while (node_p && (apr_status == APR_SUCCESS))
						{
							apr_status = PrintItem (config_p -> theme_p, node_p -> ion_object_p, &irods_config, i, bucket_brigade_p, pool_p, rods_connection_p, req_p);

							node_p = node_p -> ion_next_p;
							++ i;
						}

					FreeIRodsObjectNodeList (root_node_p);
				}		/* if (root_node_p) */

			apr_status = PrintAllHTMLAfterListing (rods_connection_p -> clientUser.userName, escaped_zone_s, davrods_path_s, config_p, NULL, rods_connection_p, req_p, bucket_brigade_p, pool_p);


			CloseBucketsStream (bucket_brigade_p);

			apr_status = apr_brigade_pflatten (bucket_brigade_p, &result_s, &result_length, pool_p);

			if (result_s)
				{
					/*
					 * Sometimes there is garbage at the end of this, and I don't know which apr_brigade_...
					 * method I need to get the terminating '\0' so have to do it explicitly.
					 */
					if (* (result_s + result_length) != '\0')
						{
							* (result_s + result_length) = '\0';
						}

					apr_brigade_destroy (bucket_brigade_p);

					ap_set_content_type (req_p, "text/html");

					ap_rputs (result_s, req_p);
					res = OK;
				}

		}		/* if (rods_connection_p) */

	return res;
}


static int GetSearchMetadataAsHTML (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	const char *key_s = NULL;
	const char *value_s = NULL;
	SearchOperator op = SO_LIKE;

	if (GetSearchParameters (&key_s, &value_s, &op, params_p, req_p))
		{
			rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

			if (rods_connection_p)
				{
					char *res_s = DoMetadataSearch (key_s, value_s, op, rods_connection_p, config_p, req_p, davrods_path_s);

					if (res_s)
						{
							ap_set_content_type (req_p, "text/html");

							ap_rputs (res_s, req_p);
							res = OK;
						}
				}
		}

	return res;
}



static bool GetSearchParameters (const char **key_ss, const char **value_ss, SearchOperator *op_p, apr_table_t *params_p, request_rec *req_p)
{
	bool success_flag = false;
	apr_pool_t *pool_p = req_p -> pool;
	const char * const key_s = GetParameterValue (params_p, "key", pool_p);

	if (key_s)
		{
			const char * const value_s = GetParameterValue (params_p, "value", pool_p);

			if (value_s)
				{
					SearchOperator op = SO_LIKE;
					const char *op_s = apr_table_get (params_p, "op");

					*key_ss = key_s;
					*value_ss = value_s;

					if (op_s)
						{
							apr_status_t status = GetSearchOperatorFromString (op_s, &op);

							if (status == APR_SUCCESS)
								{
									success_flag = true;
								}
							else
								{
									ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "error %d: Unknown operator \"%s\"", status, op_s);
								}
						}
					else
						{
							success_flag = true;
						}

					if (success_flag)
						{
							*op_p = op;
						}
				}
		}

	return success_flag;
}

/*
 * STATIC DEFINITIONS
 */

static int SearchMetadata (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	const char *key_s = NULL;
	const char *value_s = NULL;
	SearchOperator op = SO_LIKE;


	if (GetSearchParameters (&key_s, &value_s, &op, params_p, req_p))
		{
			rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

			if (rods_connection_p)
				{
					IRodsObjectNode *node_p = GetMatchingMetadataHits (key_s, value_s, op, rods_connection_p, pool_p);

					if (node_p)
						{
							IRodsConfig irods_config;

							char *metadata_root_link_s = apr_pstrcat (pool_p, davrods_path_s, config_p -> eirods_dav_views_path_s, NULL);

							const char *exposed_root_s = GetRodsExposedPath (req_p);


							SetIRodsConfig (&irods_config, exposed_root_s, davrods_path_s, metadata_root_link_s);

							PrintIRodsObjectNodesToJSON (node_p, &irods_config, req_p);
							FreeIRodsObjectNodeList (node_p);
						}
					else
						{
							ap_rputs ("[]", req_p);
						}

					res = OK;
				}		/* if (rods_connection_p) */

		}

	return res;
}




static OutputFormat GetRequestedOutputFormat (apr_table_t *params_p, apr_pool_t *pool_p, OutputFormat default_format)
{
	OutputFormat format = default_format;
	const char * const format_s = GetParameterValue (params_p, "output_format", pool_p);

	if (format_s)
		{
			if (strcmp (format_s, "json") == 0)
				{
					format = OF_JSON;
				}
			else if (strcmp (format_s, "html") == 0)
				{
					format = OF_HTML;
				}
			else if (strcmp (format_s, "tsv") == 0)
				{
					format = OF_TSV;
				}
			else if (strcmp (format_s, "csv") == 0)
				{
					format = OF_CSV;
				}
		}

	return format;
}


static int ListInformationForEntries (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	IRodsObjectNode *root_node_p = GetMatchingIds (req_p, params_p, config_p);

	if (root_node_p)
		{
			IRodsConfig irods_config;
			char *metadata_root_link_s = apr_pstrcat (pool_p, davrods_path_s, config_p -> eirods_dav_views_path_s, NULL);
			const char *exposed_root_s = GetRodsExposedPath (req_p);

			SetIRodsConfig (&irods_config, exposed_root_s, davrods_path_s, metadata_root_link_s);

			PrintIRodsObjectNodesToJSON (root_node_p, &irods_config, req_p);
			FreeIRodsObjectNodeList (root_node_p);

			res = OK;
		}		/* if (root_node_p) */
	else
		{
			ap_rputs ("[]", req_p);
		}


	return res;
}


static int GetInformationForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

	if (rods_connection_p)
		{
			const char * const path_s = GetParameterValue (params_p, "path", pool_p);

			if (path_s)
				{
					const char *full_path_s = GetFullPath (path_s, req_p, pool_p);

					if (full_path_s)
						{
							rodsObjStat_t *stat_p = GetObjectStat (full_path_s, rods_connection_p, pool_p);

							if (stat_p)
								{
									char *result_s = NULL;

									json_t *obj_p = json_object ();

									if (obj_p)
										{
											char *id_s = NULL;
											const char *type_s = "";
											
											switch (stat_p -> objType)
												{
													case DATA_OBJ_T:
														type_s = "1.";
														break;
														
													case COLL_OBJ_T:
														type_s = "2.";
														break;
														
													default:
														break;
												}
																				
											id_s = apr_pstrcat (pool_p, type_s, stat_p -> dataId, NULL);

											if (id_s)
												{
													if (json_object_set_new (obj_p, "id", json_string (id_s)) == 0)
														{
															if (json_object_set_new (obj_p, "path", json_string (path_s)) == 0)
																{
																	result_s = json_dumps (obj_p, JSON_INDENT (2));

																	if (result_s)
																		{
																			ap_rputs (result_s, req_p);
																			free (result_s);
																		}
																	else
																		{
																			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to dump json for %s", full_path_s);
																		}

																	res = OK;
																}
															else
																{
																	ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to add \"path\": \"%s\" to json object for %s", path_s, full_path_s);
																}
														}													
													else
														{
															ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to add \"id\": \"%s\" to json object for %s", id_s, full_path_s);
														}
												}
											else
												{
													ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to create id string from \"%s\" and \"%s\" for %s", type_s, stat_p -> dataId, full_path_s);
												}

										}		/* if (obj_p) */
									else
										{
											ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to create JSON object for %s", full_path_s);
										}

									freeRodsObjStat (stat_p);
								}		/* if (stat_p) */
							else
								{
									ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to  stat full path for \"%s\"", full_path_s);
								}

						}		/* if (full_path_s) */
					else
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to get full path for \"%s\"", path_s);
						}

				}		/* if (path_s) */
			else
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Missing path variable for %s", req_p -> uri);
				}
		}

	return res;
}



static int GetMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	apr_bucket_brigade *bucket_brigade_p = apr_brigade_create (pool_p, req_p -> connection -> bucket_alloc);

	if (bucket_brigade_p)
		{
			rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

			if (rods_connection_p)
				{
					const char * const id_s = GetIdParameter (params_p, req_p, rods_connection_p, pool_p);

					if (id_s)
						{
							OutputFormat format = GetRequestedOutputFormat (params_p, pool_p, OF_JSON);
							int editable_flag = GetEditableFlag (config_p -> theme_p, params_p, pool_p);

							apr_status_t status = GetMetadataTableForId ((char *) id_s, config_p, rods_connection_p, req_p, pool_p, bucket_brigade_p, format, editable_flag);

							if (status == APR_SUCCESS)
								{
									apr_size_t len = 0;
									char *result_s = NULL;

									CloseBucketsStream (bucket_brigade_p);

									apr_status_t apr_status = apr_brigade_pflatten (bucket_brigade_p, &result_s, &len, pool_p);

									if (apr_status == APR_SUCCESS)
										{
											/*
											 * Sometimes there is garbage at the end of this, and I don't know which apr_brigade_...
											 * method I need to get the terminating '\0' so have to do it explicitly.
											 */
											if (* (result_s + len) != '\0')
												{
													* (result_s + len) = '\0';
												}

											if (result_s)
												{
													ap_rputs (result_s, req_p);
												}

											SetMimeTypeForOutputFormat (req_p, format);

											res = OK;
										}
								}
						}

					apr_brigade_destroy (bucket_brigade_p);
				}

		}

	return res;
}



static int DeleteMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = HTTP_INTERNAL_SERVER_ERROR;
	apr_status_t status = EasyModifyMetadataForEntry (call_p, req_p, params_p, config_p, davrods_path_s, "rm");

	if (status == APR_SUCCESS)
		{
			res = OK;
		}

	return res;
}


static int AddMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	return EasyModifyMetadataForEntry (call_p, req_p, params_p, config_p, davrods_path_s, "add");
}



static char *GetModValue (apr_table_t *params_p, const char *param_key_s, const char *value_prefix_s, const char *old_key_s, apr_pool_t *pool_p)
{
	char *mod_value_s = NULL;
	const char *value_s = GetParameterValue (params_p, param_key_s, pool_p);

	if (value_s && (strlen (value_s) > 0))
		{
			const char *old_value_s = GetParameterValue (params_p, old_key_s, pool_p);

			if (value_s && (strlen (value_s) > 0) && (strcmp (old_value_s, value_s) != 0))
				{
					mod_value_s = apr_pstrcat (pool_p, value_prefix_s, value_s, NULL);

					if (!mod_value_s)
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, pool_p, "Failed to create modified value for \"%s\"", value_s);
						}
				}
		}

	return mod_value_s;
}


static int EditMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;

	const char *new_key_s = GetModValue (params_p, "new_key", "n:", "key", pool_p);
	const char *new_value_s = GetModValue (params_p, "new_value", "v:", "value", pool_p);
	const char *new_units_s = GetModValue (params_p, "new_units", "u:", "units", pool_p);

	if (new_key_s || new_value_s || new_units_s)
		{
			const char *args_ss [3] = { "", "", "" };
			const char **arg_ss = args_ss;

			if (new_key_s)
				{
					*arg_ss = new_key_s;
					++ arg_ss;
				}

			if (new_value_s)
				{
					*arg_ss = new_value_s;
					++ arg_ss;
				}

			if (new_units_s)
				{
					*arg_ss = new_units_s;
				}

			res = ModifyMetadataForEntry (call_p, req_p, params_p, config_p, davrods_path_s, "mod", *args_ss, * (args_ss + 1), * (args_ss + 2));
		}
	else
		{
			/* nothing to do */
		}


	return res;
}


static apr_status_t EasyModifyMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s, const char *command_s)
{
	return ModifyMetadataForEntry (call_p, req_p, params_p, config_p, davrods_path_s, command_s, "", "", "");
}


static apr_status_t AddDecodedJSONResponse (const APICall *call_p, apr_status_t status, const char *id_s, request_rec *req_p)
{
	apr_status_t local_status = APR_EGENERAL;
	json_t *res_p = json_object ();

	if (res_p)
		{
			if (json_object_set_new (res_p, "call", json_string (call_p -> ac_action_s)) == 0)
				{
					if (json_object_set_new (res_p, "id", json_string (id_s)) == 0)
						{
							if (json_object_set_new (res_p, "status", json_string ((status == APR_SUCCESS) ? "true" : "false")) == 0)
								{
									char *res_s = json_dumps (res_p, JSON_INDENT (2));

									if (res_s)
										{
											if (ap_rputs (res_s, req_p) > 0)
												{
													ap_set_content_type (req_p, "application/json");

													local_status = APR_SUCCESS;
												}

											free (res_s);
										}
								}
						}
				}

			json_decref (res_p);
		}


	return local_status;
}


static void SetMimeTypeForOutputFormat (request_rec *req_p, const OutputFormat fmt)
{
	const char *content_type_s = "text/plain";

	switch (fmt)
		{
			case OF_CSV:
				content_type_s = "text/csv";
				break;

			case OF_HTML:
				content_type_s = "text/html";
				break;

			case OF_JSON:
				content_type_s = "application/json";
				break;

			case OF_TSV:
				content_type_s = "text/tab-separated-values";
				break;

			default:
				break;
		}

	ap_set_content_type (req_p, content_type_s);
}


static apr_status_t ModifyMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s, const char *command_s, const char *arg_0_s, const char *arg_1_s, const char *arg_2_s)
{
	apr_status_t res = APR_EGENERAL;
	apr_pool_t *pool_p = req_p -> pool;
	apr_bucket_brigade *bucket_brigade_p = apr_brigade_create (pool_p, req_p -> connection -> bucket_alloc);

	if (bucket_brigade_p)
		{
			rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

			if (rods_connection_p)
				{
					const char * const id_s = GetIdParameter (params_p, req_p, rods_connection_p, pool_p);

					if (id_s)
						{
							const char * const key_s = GetParameterValue (params_p, "key", pool_p);

							if (key_s)
								{
									const char * const value_s = GetParameterValue (params_p, "value", pool_p);

									if (value_s)
										{
											IRodsObject irods_obj;
											apr_status_t apr_res;

											InitIRodsObject (&irods_obj);

											apr_res = SetIRodsObjectFromIdString (&irods_obj, id_s, rods_connection_p, pool_p);

											if (apr_res == APR_SUCCESS)
												{
													const char *type_s = NULL;
													modAVUMetadataInp_t mod;

													mod.arg0 = (char *) command_s;

													switch (irods_obj.io_obj_type)
													{
														case DATA_OBJ_T:
															type_s = "-d";
															break;

														case COLL_OBJ_T:
															type_s = "-C";
															break;

														default:
															break;
													}


													if (type_s)
														{
															int status;
															const char *units_s = GetParameterValue (params_p, "units", pool_p);
															char *full_name_s = apr_pstrcat (req_p -> pool, irods_obj.io_collection_s, "/", irods_obj.io_data_s, NULL);

															mod.arg1 = (char *) type_s;
															mod.arg2 = full_name_s;
															mod.arg3 = (char *) key_s;
															mod.arg4 = (char *) value_s;


															if (units_s && (strlen (units_s) > 0))
																{
																	mod.arg5 = (char *) units_s;
																	mod.arg6 = (char *) arg_0_s;
																	mod.arg7 = (char *) arg_1_s;
																	mod.arg8 = (char *) arg_2_s;
																}
															else
																{
																	mod.arg5 = (char *) arg_0_s;
																	mod.arg6 = (char *) arg_1_s;
																	mod.arg7 = (char *) arg_2_s;
																	mod.arg8 = (char *) "";
																}

															mod.arg9 = "";

															status = rcModAVUMetadata (rods_connection_p, &mod);

															if (status == 0)
																{
																	res = APR_SUCCESS;
																}
															else
																{
																	const char *error_s = rodsErrorName (status, NULL);

																	if (error_s)
																		{
																			ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p,
																										 "rcModAVUMetadata failed, error: %s for args \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
																										 error_s, mod.arg1, mod.arg2, mod.arg3, mod.arg4, mod.arg5, mod.arg6, mod.arg7, mod.arg8, mod.arg9);
																		}
																	else
																		{
																			ap_log_perror (APLOG_MARK, APLOG_ERR, APR_EGENERAL, pool_p,
																										 "rcModAVUMetadata failed, error: %d for args \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
																										 status, mod.arg1, mod.arg2, mod.arg3, mod.arg4, mod.arg5, mod.arg6, mod.arg7, mod.arg8, mod.arg9);
																		}

																	res = APR_EGENERAL;
																}

															AddDecodedJSONResponse (call_p, res, id_s, req_p);

														}		/* if (type_s) */
													else
														{
															ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to get object type for id \"%s\"", id_s);
														}

												}		/* if (apr_res == APR_SUCCESS) */
											else
												{
													ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to get iRODS object for id \"%s\"", id_s);
												}

										}
									else
										{
											ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to get value parameter from \"%s\"", req_p -> uri);
										}


								}
							else
								{
									ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to get key parameter from \"%s\"", req_p -> uri);
								}
						}
					else
						{
							ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to get id parameter from \"%s\"", req_p -> uri);
						}

				}
			else
				{
					ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to get iRODS connection");
				}
		}
	else
		{
			ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "Failed to allocate bucket brigade");
		}


	return res;
}




static int GetMatchingMetadataKeys (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	const char * const key_s = GetParameterValue (params_p, "key", pool_p);

	if (key_s)
		{
			char *where_value_s = apr_pstrcat (pool_p, "%", key_s, "%", NULL);

			if (where_value_s)
				{
					json_t *res_p = json_object ();

					if (res_p)
						{
							json_t *keys_array_p = json_array ();

							if (keys_array_p)
								{
									if (json_object_set_new (res_p, "keys", keys_array_p) == 0)
										{
											const int num_where_columns = 1;
											int where_columns = COL_META_DATA_ATTR_NAME;
											const char *where_values_ss [1] = { where_value_s };
											SearchOperator op = SO_LIKE;
											int select_columns_p [] =  { COL_META_DATA_ATTR_NAME, -1};

											apr_status_t status = RunMetadataQuery (&where_columns, where_values_ss, &op, num_where_columns, select_columns_p, keys_array_p, req_p, config_p);

											if (status == APR_SUCCESS)
												{
													char *result_s = json_dumps (res_p, JSON_INDENT (2));

													if (result_s)
														{
															ap_rputs (result_s, req_p);
															res = OK;
															free (result_s);
														}		/* if (result_s) */
													else
														{
															ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "json_dumps failed");
														}

												}


										}		/* if (json_object_set_new (res_p, "keys", keys_array_p) == 0) */
									else
										{
											json_decref (keys_array_p);
											ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to add JSON keys array to response");
										}

								}		/* if (keys_array_p) */
							else
								{
									ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create JSON keys array for response");
								}

							json_decref (res_p);
						}		/* if (res_p) */
					else
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create JSON object for response");
						}

				}		/* if (where_value_s) */


		}		/* if (key_s) */


	return res;
}



static int GetMatchingMetadataValues (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	const char * const key_s = GetParameterValue (params_p, "key", pool_p);

	if (key_s)
		{
			const char * const value_s = GetParameterValue (params_p, "value", pool_p);

			char *where_value_s = apr_pstrcat (pool_p, "%", value_s, "%", NULL);

			if (where_value_s)
				{
					json_t *res_p = json_object ();

					if (res_p)
						{
							if (json_object_set_new (res_p, "key", json_string (key_s)) == 0)
								{
									json_t *values_array_p = json_array ();

									if (values_array_p)
										{
											if (json_object_set_new (res_p, "values", values_array_p) == 0)
												{
													const int num_where_columns = 2;
													int where_columns_p [] =  { COL_META_DATA_ATTR_NAME, COL_META_DATA_ATTR_VALUE };
													const char *where_values_ss [] = { key_s, where_value_s };
													SearchOperator ops_p [] =  {SO_EQUALS, SO_LIKE };
													int select_columns_p [] =  { COL_META_DATA_ATTR_VALUE, -1};

													apr_status_t status = RunMetadataQuery (where_columns_p, where_values_ss, ops_p, num_where_columns, select_columns_p, values_array_p, req_p, config_p);

													if (status == APR_SUCCESS)
														{
															char *result_s = json_dumps (res_p, JSON_INDENT (2));

															if (result_s)
																{
																	ap_rputs (result_s, req_p);
																	res = OK;
																	free (result_s);
																}		/* if (result_s) */
															else
																{
																	ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "json_dumps failed");
																}

														}
												}		/* if (json_object_set_new (res_p, "values", values_array_p) == 0) */
											else
												{
													json_decref (values_array_p);
													ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to add JSON keys array to response");
												}

										}		/* if (values_array_p) */
									else
										{
											ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create JSON keys array for response");
										}

								}		/* if (json_object_set_new (res_p, "key", json_string (key_s)) == 0) */
							else
								{
									ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to add \"key\": \"%s\" to JSON response", key_s);
								}

							json_decref (res_p);
						}		/* if (res_p) */
					else
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to create JSON object for response");
						}
				}

		}

	return res;
}


static const char *GetFullPath (const char *path_s, request_rec *req_p, apr_pool_t *pool_p)
{
	const char *full_path_s = NULL;

	if (*path_s == '/')
		{
			full_path_s = path_s;
		}
	else
		{
			const char *root_path_s = GetRodsExposedPath (req_p);

			if (root_path_s)
				{
					size_t l = strlen (root_path_s);

					if (* (root_path_s + l - 1) == '/')
						{
							full_path_s = apr_pstrcat (pool_p, root_path_s, path_s, NULL);
						}
					else
						{
							full_path_s = apr_pstrcat (pool_p, root_path_s, "/", path_s, NULL);
						}
				}
			else
				{
					full_path_s = path_s;
				}
		}

	return full_path_s;
}


static const char *GetIdParameter (apr_table_t *params_p, request_rec *req_p, rcComm_t *rods_connection_p, apr_pool_t *pool_p)
{
	const char *id_s = GetParameterValue (params_p, "id", pool_p);

	if (!id_s)
		{
			const char *path_s = GetParameterValue (params_p, "path", pool_p);

			if (path_s)
				{
					const char *full_path_s = GetFullPath (path_s, req_p, pool_p);

					if (full_path_s)
						{
							rodsObjStat_t *stat_p = GetObjectStat (full_path_s, rods_connection_p, pool_p);

							if (stat_p)
								{
									char *prefix_s = apr_itoa (pool_p, stat_p -> objType);

									if (prefix_s)
										{
											id_s = apr_pstrcat (pool_p, prefix_s, ".", stat_p -> dataId, NULL);

											if (!id_s)
												{
													ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_ENOMEM, pool_p, "Failed to copy id \"%s\" for path \"%s\"", stat_p -> dataId, full_path_s);
												}
										}
									else
										{
											ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_ENOMEM, pool_p, "Failed to get prefix string for %d", stat_p -> objType);
										}

									freeRodsObjStat (stat_p);
								}
							else
								{
									ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to get status for path \"%s\"", full_path_s);
								}

						}
					else
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to get full path for \"%s\"", req_p -> unparsed_uri);
						}


				}
			else
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to get path or id parameter from \"%s\"", req_p -> unparsed_uri);
				}
		}

	return id_s;
}



rcComm_t *GetIRODSConnectionForAPI (request_rec *req_p, davrods_dir_conf_t *config_p)
{
	rcComm_t *rods_connection_p = GetIRODSConnectionFromRequest (req_p);

	if (!rods_connection_p)
		{
			apr_pool_t *davrods_pool_p = GetDavrodsMemoryPool (req_p);

			if (davrods_pool_p)
				{
					rods_connection_p  = GetIRODSConnectionForPublicUser (req_p, davrods_pool_p, config_p);
				}
		}

	return rods_connection_p;
}


static apr_status_t RunMetadataQuery (const int *where_columns_p, const char **where_values_ss, const SearchOperator *ops_p, const size_t num_where_columns, const int *select_columns_p, json_t *res_array_p, request_rec *req_p, davrods_dir_conf_t *config_p)
{
	apr_status_t status = APR_EGENERAL;
	genQueryOut_t *meta_results_p = NULL;
	rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

	if (rods_connection_p)
		{
			meta_results_p = RunQuery (rods_connection_p, select_columns_p, where_columns_p, where_values_ss, ops_p, num_where_columns, 0, req_p -> pool);

			if (meta_results_p)
				{
					status = APR_SUCCESS;

					if (meta_results_p -> rowCnt > 0)
						{
							sqlResult_t *sql_p = & (meta_results_p -> sqlResult [0]);
							char *value_s = sql_p -> value;
							int result_length = sql_p -> len;
							int i;

							for (i = meta_results_p -> rowCnt - 1; i >= 0; -- i, value_s += result_length)
								{
									if (json_array_append_new (res_array_p, json_string (value_s)) != 0)
										{
											ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, req_p -> pool, "Failed to add \"%s\" to keys list", value_s);

											status = APR_EGENERAL;
											i = -1;		/* force exit from loop */
										}
								}

						}		/* if (meta_results_p -> rowCnt > 0) */
				}

			freeGenQueryOut (&meta_results_p);
		}

	return status;
}

