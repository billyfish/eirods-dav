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


static const char *GetIdParameter (apr_table_t *params_p, request_rec *req_p, rcComm_t *rods_connection_p, apr_pool_t *pool_p);


static apr_status_t EasyModifyMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s, const char *command_s);


static apr_status_t ModifyMetadataForEntry (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s, const char *command_s, const char *new_key_s, const char *new_value_s, const char *new_units_s);


static char *GetModValue (apr_table_t *params_p, const char *param_key_s, const char *value_prefix_s, const char *old_key_s, apr_pool_t *pool_p);


static apr_status_t AddDecodedJSONResponse (const APICall *call_p, apr_status_t status, const char *id_s, request_rec *req_p);


static OutputFormat GetRequestedOutputFormat (apr_table_t *params_p, apr_pool_t *pool_p);


static apr_status_t ReadRequestBody (request_rec *req_p, apr_bucket_brigade *bucket_brigade_p);


static apr_status_t RunMetadataQuery (const int *where_columns_p, const char **where_values_ss, const SearchOperator *ops_p, const size_t num_where_columns, const int *select_columns_p, json_t *res_array_p, request_rec *req_p, davrods_dir_conf_t *config_p);

static IRodsObjectNode *GetMetadataHits (const char * const key_s, const char * const value_s, SearchOperator op, rcComm_t *rods_connection_p, apr_pool_t *pool_p);


/*
 * STATIC VARIABLES
 */

static APICall S_API_ACTIONS_P [] =
		{
				{ REST_METADATA_SEARCH_S, SearchMetadata },
				{ REST_METADATA_GET_S, GetMetadataForEntry },
				{ REST_METADATA_ADD_S, AddMetadataForEntry },
				{ REST_METADATA_DELETE_S, DeleteMetadataForEntry },
				{ REST_METADATA_EDIT_S, EditMetadataForEntry },
				{ REST_METADATA_MATCHING_KEYS_S, GetMatchingMetadataKeys },
				{ REST_METADATA_MATCHING_VALUES_S, GetMatchingMetadataValues },

				{ NULL, NULL }
		};



/*
 * API DEFINITIONS
 */

int EIRodsDavAPIHandler (request_rec *req_p)
{
	int res = DECLINED;

	//DebugRequest (req_p);

	/*
	 * Normally we would check if this is a call for the ei-rods-dav rest handler,
	 * but dav-handler will have gotten there first. So check it against our path
	 * and see if we are interested in it.
	 * If it is, we accept it and do our things, it not, we simply return DECLINED,
	 * and Apache will try somewhere else.
	 */
	if ((req_p -> method_number == M_GET) || (req_p -> method_number == M_POST))
		{
			davrods_dir_conf_t *config_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);

			if ((config_p -> davrods_api_path_s) && (req_p -> path_info))
				{
					const size_t api_path_length = strlen (config_p -> davrods_api_path_s);

					if (strncmp (config_p -> davrods_api_path_s, req_p -> path_info, api_path_length) == 0)
						{
							/*
							 * Parse the uri from req_p -> path_info to get the API call
							 */
							const APICall *call_p = S_API_ACTIONS_P;
							apr_table_t *params_p = NULL;
							const char *path_s = (req_p -> path_info) + api_path_length;
							char *davrods_path_s = NULL;

							/* Get the Location path where davrods is hosted */
							const char *ptr = strstr (req_p -> uri, config_p -> davrods_api_path_s);

							if (ptr)
								{
									davrods_path_s = apr_pstrmemdup (req_p -> pool, req_p -> uri, ptr - (req_p -> uri));
								}

							ap_args_to_table (req_p, &params_p);

							while ((call_p != NULL) && (call_p -> ac_action_s != NULL))
								{
									size_t l = strlen (call_p -> ac_action_s);

									if (strncmp (path_s, call_p -> ac_action_s, l) == 0)
										{
											res = call_p -> ac_callback_fn (call_p, req_p, params_p, config_p, davrods_path_s);

											/* force exit from loop */
											call_p = NULL;
										}
									else
										{
											++ call_p;
										}

								}		/* while (call_p -> ac_action_s != NULL) */


							if (!apr_is_empty_table (params_p))
								{
									apr_table_clear (params_p);
								}

						}		/* if (strncmp (config_p -> davrods_api_path_s, req_p -> path_info, api_path_length) == 0) */
					else
						{
							const size_t search_path_length = strlen (config_p -> davrods_search_path_s);

							if (strncmp (config_p -> davrods_search_path_s, req_p -> path_info, search_path_length) == 0)
								{
									//static IRodsObjectNode *GetMetadataHits (const char * const key_s, const char * const value_s, SearchOperator op, rcComm_t *rods_connection_p, apr_pool_t *pool_p)

									//res = DisplaySearchResults ();
								}		/* if (strncmp (config_p -> davrods_search_path_s, req_p -> path_info, search_path_length) == 0) */
						}

				}
		}



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



static apr_status_t ReadRequestBody (request_rec *req_p, apr_bucket_brigade *bucket_brigade_p)
{
	apr_status_t status = APR_EGENERAL;
	int ret = ap_setup_client_block (req_p, REQUEST_CHUNKED_ERROR);

	if (ret == OK)
		{
			if (ap_should_client_block (req_p))
				{
					char         temp_s [HUGE_STRING_LEN];
					apr_off_t    len_read;

					status = APR_SUCCESS;

					while (((len_read = ap_get_client_block (req_p, temp_s, sizeof (temp_s))) > 0) && (status == APR_SUCCESS))
						{
							* (temp_s + len_read) = '\0';

							status = apr_brigade_puts (bucket_brigade_p, NULL, NULL, temp_s);
						}


					if ((len_read == -1) || (status == APR_EGENERAL))
						{
							ap_log_rerror_(APLOG_MARK, APLOG_ERR, APR_EGENERAL, req_p, "error getting client block");

							status = APR_EGENERAL;
						}

				}		/* if (ap_should_client_block (req_p)) */
			else
				{
					ap_log_rerror_(APLOG_MARK, APLOG_ERR, APR_EGENERAL, req_p, "ap_should_client_block failed");
				}

		}		/* if (ret == OK) */
	else
		{
			ap_log_rerror_(APLOG_MARK, APLOG_ERR, APR_EGENERAL, req_p, "Failed to set up client block to read request, %d", ret);
		}

	return status;
}


/*
 * STATIC DEFINITIONS
 */

static int SearchMetadata (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	const char * const key_s = GetParameterValue (params_p, "key", pool_p);

	ap_set_content_type (req_p, "application/json");

	if (key_s)
		{
			const char * const value_s = GetParameterValue (params_p, "value", pool_p);

			if (value_s)
				{
					SearchOperator op = SO_LIKE;
					const char *op_s = apr_table_get (params_p, "op");
					rcComm_t *rods_connection_p = GetIRODSConnectionForAPI (req_p, config_p);

					if (op_s)
						{
							apr_status_t status = GetSearchOperatorFromString (op_s, &op);

							if (status != APR_SUCCESS)
								{
									ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "error %d: Unknown operator \"%s\"", status, op_s);
								}
						}

					if (rods_connection_p)
						{
							IRodsObjectNode *node_p = GetMetadataHits (key_s, value_s, op, rods_connection_p, pool_p);

							if (node_p)
								{
									IRodsConfig irods_config;

									char *metadata_root_link_s = apr_pstrcat (pool_p, davrods_path_s, config_p -> davrods_search_path_s, NULL);

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

		}

	return res;
}




static IRodsObjectNode *GetMetadataHits (const char * const key_s, const char * const value_s, SearchOperator op, rcComm_t *rods_connection_p, apr_pool_t *pool_p)
{
	/*
	 * SELECT meta_id FROM r_meta_main WHERE meta_attr_name = ' ' AND meta_attr_value = ' ';
	 *
	 * SELECT object_id FROM r_objt_metamap WHERE meta_id = ' ';
	 *
	 * object_id is for data object and collection
	 *
	 * Get the full path to the object and then use
	 *
	 * rcObjStat     (rcComm_t *conn, dataObjInp_t *dataObjInp, rodsObjStat_t **rodsObjStatOut)
	 *
	 * to get the info we need for a listing
	 */
	int num_where_columns = 2;
	int where_columns_p [] =  { COL_META_DATA_ATTR_NAME, COL_META_DATA_ATTR_VALUE };
	const char *where_values_ss [] = { key_s, value_s };
	SearchOperator ops_p [] = { SO_EQUALS, op };
	int select_columns_p [] =  { COL_META_DATA_ATTR_ID, -1, -1};
	genQueryOut_t *meta_id_results_p = NULL;
	IRodsObjectNode *root_node_p = NULL;
	IRodsObjectNode *current_node_p = NULL;

	/*
	 * SELECT meta_id FROM r_meta_main WHERE meta_attr_name = ' ' AND meta_attr_value = ' ';
	 */
	meta_id_results_p = RunQuery (rods_connection_p, select_columns_p, where_columns_p, where_values_ss, ops_p, num_where_columns, 0, pool_p);

	if (meta_id_results_p)
		{

			if (meta_id_results_p -> attriCnt == 1)
				{
					int i;
					const int meta_results_inc = meta_id_results_p -> sqlResult [0].len;
					const char *meta_id_s = meta_id_results_p -> sqlResult [0].value;

					/*
					 * SELECT object_id FROM r_objt_metamap WHERE meta_id = ' ';
					 */

					for (i = 0; i < meta_id_results_p -> rowCnt; ++ i, meta_id_s += meta_results_inc)
						{
							/*
							 * Get all of the matching collections first
							 */
							int object_id_select_columns_p [] = { COL_COLL_ID, -1 };
							genQueryOut_t *id_results_p;

							where_columns_p [0] = COL_META_COLL_ATTR_ID;
							where_values_ss [0] = meta_id_s;

							/*
							 * Get all of the matching collections
							 *
							 * SELECT object_id FROM r_objt_metamap WHERE meta_id = ' ';
							 */

							id_results_p = RunQuery (rods_connection_p, object_id_select_columns_p, where_columns_p, where_values_ss, NULL, 1, 0, pool_p);

							if (id_results_p)
								{
									if (id_results_p -> rowCnt > 0)
										{
											int j;
											int num_select_columns = 1;

											/* we only searched for 1 attribute so we want the 1st result */
											const char *id_s = id_results_p -> sqlResult[0].value;
											const int inc = id_results_p -> sqlResult[0].len;
											select_columns_p [0] = COL_COLL_NAME;
											select_columns_p [1] = -1;

											where_columns_p [0] = COL_COLL_ID;
											num_where_columns = 1;

											for (j = 0; j < id_results_p -> rowCnt; ++ j, id_s += inc)
												{
													/*
													 *
													 * The id can be the data id, coll id, etc. so we have to work
													 * out what it is
													 *
													 * Start by testing if the id refers to a collection
													 *
													 * 		SELECT coll_name FROM r_coll_main WHERE coll_id = '10001';
													 *
													 */

													genQueryOut_t *collection_name_results_p = NULL;

													where_values_ss [0] = id_s;

													collection_name_results_p = RunQuery (rods_connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, num_where_columns, 0, pool_p);

													if (collection_name_results_p)
														{
															if (collection_name_results_p -> rowCnt > 0)
																{
																	if (collection_name_results_p -> attriCnt == num_select_columns)
																		{
																			const char *collection_s = collection_name_results_p -> sqlResult [0].value;
																			rodsObjStat_t *stat_p;

																			stat_p = GetObjectStat (collection_s, rods_connection_p, pool_p);

																			if (stat_p)
																				{
																					IRodsObjectNode *node_p = AllocateIRodsObjectNode (COLL_OBJ_T, id_s, NULL, collection_s, stat_p -> ownerName, NULL, stat_p -> modifyTime, stat_p -> objSize);

																					if (node_p)
																						{
																							current_node_p = node_p;

																							if (current_node_p)
																								{
																									current_node_p -> ion_next_p = node_p;
																								}
																							else
																								{
																									root_node_p = node_p;
																								}
																						}


																					freeRodsObjStat (stat_p);
																				}		/* if (stat_p) */

																		}		/* if (collection_name_results_p -> attriCnt == num_select_columns) */

																}		/* if (collection_name_results_p -> rowCnt > 0) */

															freeGenQueryOut (&collection_name_results_p);
														}		/* if (collection_name_results_p) */



												}		/* for (j = 0; j < id_results_p -> rowCnt; ++ j) */

										}		/* if (id_results_p -> rowCnt > 0) */

									freeGenQueryOut (&id_results_p);
								}		/* if (id_results_p) */


							/*
							 * Get all of the data objects
							 */
							object_id_select_columns_p [0] = COL_D_DATA_ID;

							where_columns_p [0] = COL_META_DATA_ATTR_ID;
							where_values_ss [0] = meta_id_s;

							id_results_p = RunQuery (rods_connection_p, object_id_select_columns_p, where_columns_p, where_values_ss, NULL, 1, 0, pool_p);

							if (id_results_p)
								{
									if (id_results_p -> rowCnt > 0)
										{
											int j;

											/* we only searched for 1 attribute so we want the 1st result */
											const char *id_s = id_results_p -> sqlResult [0].value;
											const int inc = id_results_p -> sqlResult [0].len;
											int num_select_columns = 2;

											select_columns_p [0] = COL_DATA_NAME;
											select_columns_p [1] = COL_D_COLL_ID;
											select_columns_p [2] = -1;

											where_columns_p [0] = COL_D_DATA_ID;

											for (j = 0; j < id_results_p -> rowCnt; ++ j, id_s += inc)
												{
													genQueryOut_t *data_id_results_p = NULL;

													where_values_ss [0] = id_s;

													num_select_columns = 2;

													/*
													 * Testing as data object id.
													 *
													 * 		SELECT data_name, coll_id FROM r_data_main where data_id = 10001;
													 *
													 */
													data_id_results_p = RunQuery (rods_connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, 1, 0, pool_p);

													if (data_id_results_p)
														{
															if (data_id_results_p -> rowCnt == 1)
																{
																	/* we have a data id match */
																	if (data_id_results_p -> attriCnt == num_select_columns)
																		{
																			sqlResult_t *sql_p = data_id_results_p -> sqlResult;
																			const char *data_name_s = sql_p -> value;
																			const char *coll_id_s = (++ sql_p) -> value;
																			genQueryOut_t *coll_id_results_p = NULL;
																			int coll_id_select_columns_p [] = { COL_COLL_NAME, -1 };
																			int num_coll_id_select_columns = 1;
																			int coll_id_where_columns_p [] = { COL_COLL_ID };
																			const char *coll_id_where_values_ss [] = { coll_id_s };
																			int num_coll_id_where_columns = 1;

																			/*
																			 * We have the local data object name, we now need to get the collection name
																			 * and join the two together
																			 */
																			coll_id_results_p = RunQuery (rods_connection_p, coll_id_select_columns_p, coll_id_where_columns_p, coll_id_where_values_ss, NULL, num_coll_id_where_columns, 0, pool_p);

																			if (coll_id_results_p)
																				{
																					if (coll_id_results_p -> rowCnt == 1)
																						{
																							/* we have a coll id match */
																							if (coll_id_results_p -> attriCnt == num_coll_id_select_columns)
																								{
																									const char *collection_s = coll_id_results_p -> sqlResult [0].value;
																									char *irods_data_path_s = apr_pstrcat (pool_p, collection_s, "/", data_name_s, NULL);

																									if (irods_data_path_s)
																										{
																											rodsObjStat_t *stat_p;

																											stat_p = GetObjectStat (irods_data_path_s, rods_connection_p, pool_p);

																											if (stat_p)
																												{
																													IRodsObjectNode *node_p = AllocateIRodsObjectNode (DATA_OBJ_T, id_s, data_name_s, collection_s, stat_p -> ownerName, stat_p -> rescHier, stat_p -> modifyTime, stat_p -> objSize);

																													if (node_p)
																														{
																															current_node_p = node_p;

																															if (current_node_p)
																																{
																																	current_node_p -> ion_next_p = node_p;
																																}
																															else
																																{
																																	root_node_p = node_p;
																																}
																														}


																													freeRodsObjStat (stat_p);
																												}

																										}		/* if (irods_data_path_s) */

																								}		/* if (coll_id_results_p -> attriCnt == num_select_columns) */

																						}		/* if (coll_id_results_p -> rowCnt == 1) */

																					freeGenQueryOut (&coll_id_results_p);
																				}		/* if (coll_id_results_p) */

																		}

																}		/* if (data_id_results_p -> rowCnt == 1) */

															freeGenQueryOut (&data_id_results_p);
														}		/* if (data_id_results_p) */



												}		/* for (j = 0; j < id_results_p -> rowCnt; ++ j) */

										}		/* if (id_results_p -> rowCnt > 0) */


									freeGenQueryOut (&id_results_p);
								}		/* if (id_results_p) */

						}		/* for (i = 0; i < meta_id_results_p -> rowCnt; ++ i, meta_id_s += meta_results_inc) */



				}		/* if (meta_id_results_p -> attriCnt == 1) */

			freeGenQueryOut (&meta_id_results_p);
		}		/* if (meta_id_results_p) */

	return root_node_p;
}


static OutputFormat GetRequestedOutputFormat (apr_table_t *params_p, apr_pool_t *pool_p)
{
	OutputFormat format = OF_HTML;
	const char * const format_s = GetParameterValue (params_p, "output_format", pool_p);

	if (format_s)
		{
			if (strcmp (format_s, "json") == 0)
				{
					format = OF_JSON;
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
							OutputFormat format = GetRequestedOutputFormat (params_p, pool_p);
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
															ap_set_content_type (req_p, "application/json");

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
																	ap_set_content_type (req_p, "application/json");

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


static const char *GetIdParameter (apr_table_t *params_p, request_rec *req_p, rcComm_t *rods_connection_p, apr_pool_t *pool_p)
{
	const char *id_s = GetParameterValue (params_p, "id", pool_p);

	if (!id_s)
		{
			const char *path_s = GetParameterValue (params_p, "path", pool_p);

			if (path_s)
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
	rcComm_t *rods_connection_p = NULL;
	apr_pool_t *davrods_pool_p = GetDavrodsMemoryPool (req_p);

	if (davrods_pool_p)
		{
			rods_connection_p = GetIRODSConnectionFromPool (davrods_pool_p);

			if (!rods_connection_p)
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

