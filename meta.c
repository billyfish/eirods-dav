/*
 * meta.c
 *
 *  Created on: 26 Sep 2016
 *      Author: billy
 */

#include <stdlib.h>

#include "apr_strings.h"

#include "objStat.h"
#include "rodsGenQueryNames.h"


#include "repo.h"
#include "meta.h"
#include "common.h"
#include "rest.h"
#include "auth.h"

/*************************************/

static const int S_INITIAL_ARRAY_SIZE = 16;

static const char * const S_SEARCH_OPERATOR_EQUALS_S = "=";

static const char * const S_SEARCH_OPERATOR_LIKE_S = "like";

/**************************************/

static void InitGenQuery (genQueryInp_t *query_p, const char * const zone_s);

static int SetMetadataQuery (genQueryInp_t *query_p, const char * const zone_s);

static genQueryOut_t *ExecuteGenQuery (rcComm_t *connection_p, genQueryInp_t * const in_query_p);

static char *GetQuotedValue (const char * const input_s, const SearchOperator op, apr_pool_t *pool_p);


static int AddClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, const SearchOperator *where_ops_p, size_t num_where_columns, apr_pool_t *pool_p);


static int AddSelectClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p);


static int AddWhereClausesToQuery (genQueryInp_t *query_p, const int *where_columns_p, const char **where_values_ss, const SearchOperator *where_ops_p, size_t num_columns, apr_pool_t *pool_p);

static void ClearPooledMemoryFromGenQuery (genQueryInp_t *query_p);

static rodsObjStat_t *GetObjectStat (const char * const path_s, rcComm_t *connection_p);

void PrintBasicGenQueryOut( genQueryOut_t *genQueryOut);

static int CheckQueryResults (const genQueryOut_t * const results_p, const int min_rows, const int max_rows, const int num_attrs);

static int CompareIrodsMetadata (const void *v0_p, const void *v1_p);


/*************************************/



apr_array_header_t *GetMetadataForCollEntry (const dav_resource *resource_p, const collEnt_t *entry_p, const char *zone_s)
{
	return GetMetadata (resource_p -> info ->  rods_conn, entry_p -> objType, entry_p -> dataId, entry_p -> collName, zone_s, resource_p -> pool);
}


apr_array_header_t *GetMetadata (rcComm_t *irods_connection_p, const objType_t object_type, const char *id_s, const char *coll_name_s, const char *zone_s, apr_pool_t *pool_p)
{
	apr_array_header_t *metadata_array_p = apr_array_make (pool_p, S_INITIAL_ARRAY_SIZE, sizeof (IrodsMetadata *));

	if (metadata_array_p)
		{
			int select_col = -1;
			int where_col = -1;
			const char *where_value_s = NULL;
			genQueryInp_t in_query;

			InitGenQuery (&in_query, zone_s);

			switch (object_type)
				{
					/*
					 * Get all of the meta_id values for a given object_id.
					 *
					 * in psql:
					 *
					 * 		SELECT meta_id FROM r_objt_metamap WHERE object_id = '10002';
					 *
					 * in iquest:
					 *
					 * 		iquest "SELECT META_DATA_ATTR_ID WHERE DATA_ID = '10002'";
					 *
					 */
					case DATA_OBJ_T:
						select_col = COL_META_DATA_ATTR_ID;
						where_col = COL_D_DATA_ID;
						where_value_s = id_s;
						break;

					/*
					 * For a collection we need to do an intermediate search to get the
					 * object id from the collection name
					 *
					 * SELECT coll_id FROM r_coll_main WHERE coll_name = ' ';
					 */
					case COLL_OBJ_T:
						{
							int select_columns_p [] = { COL_COLL_ID, -1};
							int where_columns_p [] = { COL_COLL_NAME };
							const char *where_values_ss [] = { coll_name_s };
							SearchOperator where_ops_p [] = { SO_EQUALS };

							genQueryOut_t *coll_id_results_p = RunQuery (irods_connection_p, select_columns_p, where_columns_p, where_values_ss, where_ops_p, 1, pool_p);

							if (coll_id_results_p)
								{
									fprintf (stderr, "collection results:\n");
									PrintBasicGenQueryOut (coll_id_results_p);

									if ((coll_id_results_p -> attriCnt == 1) && (coll_id_results_p -> rowCnt == 1))
										{
											char *coll_id_s = coll_id_results_p -> sqlResult [0].value;

											if (coll_id_s)
												{
													where_value_s = apr_pstrdup (pool_p, coll_id_s);

													if (where_value_s)
														{
															where_col = COL_COLL_ID;
															select_col = COL_META_COLL_ATTR_ID;
														}
												}
										}

									freeGenQueryOut (&coll_id_results_p);
								}
						}
						break;

					default:
						break;
				}		/* switch (object_type) */

			/*
			 * Did we get all of the required values?
			 */
			if ((select_col != -1) && (where_col != -1) && (where_value_s != NULL))
				{
					int success_code = addInxIval (& (in_query.selectInp), select_col, 1);

					if (success_code == 0)
						{
							char *condition_and_where_value_s = GetQuotedValue (where_value_s, SO_EQUALS, pool_p);

							success_code = addInxVal (& (in_query.sqlCondInp), where_col, condition_and_where_value_s);

							if (success_code == 0)
								{
									genQueryOut_t *meta_id_results_p = NULL;

									fprintf (stderr, "initial query:");
									printGenQI (&in_query);

									meta_id_results_p = ExecuteGenQuery (irods_connection_p, &in_query);

									if (meta_id_results_p)
										{
											/* Reset out input query */
											success_code = SetMetadataQuery (&in_query, zone_s);

											fprintf (stderr, "initial results:\n");
											PrintBasicGenQueryOut (meta_id_results_p);

											if (success_code == 0)
												{
													/*
													 * Since our search was just for the meta_id values
													 * there should only be one attribute
													 */
													if (meta_id_results_p -> attriCnt == 1)
														{
															int i;
															char *meta_id_s = meta_id_results_p -> sqlResult [0].value;

															/*
															 * Iterate over the metadata id results from our previous query and get
															 * the name value pairs
															 */
															for (i = 0; i < meta_id_results_p -> rowCnt; ++ i, meta_id_s += meta_id_results_p -> sqlResult [0].len)
																{

																	if (i == 0)
																		{
																			condition_and_where_value_s = GetQuotedValue (meta_id_s, SO_EQUALS, pool_p);
																			success_code = addInxVal (& (in_query.sqlCondInp), COL_META_DATA_ATTR_ID, condition_and_where_value_s);
																		}
																	else
																		{
																			condition_and_where_value_s = GetQuotedValue (meta_id_s, SO_EQUALS, pool_p);

																			if (condition_and_where_value_s)
																				{
																					//free (in_query.sqlCondInp.value [0]);
																					in_query.sqlCondInp.value [0] = condition_and_where_value_s;

																					success_code = 0;
																				}
																			else
																				{
																					success_code = -1;
																				}
																		}

																	/* Did we set the input query successfully? */
																	if (success_code == 0)
																		{
																			genQueryOut_t *metadata_query_results_p = NULL;

																			fprintf (stderr, "output %d: \"%s\"", i, meta_id_s);
																			printGenQI (&in_query);

																			metadata_query_results_p = ExecuteGenQuery (irods_connection_p, &in_query);

																			if (metadata_query_results_p)
																				{
																					fprintf (stderr, "output results:\n");
																					PrintBasicGenQueryOut (metadata_query_results_p);

																					/*
																					 * We requested 3 metadata attributes (name, value and units)
																					 * so make sure we have 3 here
																					 */
																					if (metadata_query_results_p -> attriCnt == 3)
																						{
																							int j;
																							char *key_s = metadata_query_results_p -> sqlResult [0].value;
																							char *value_s = metadata_query_results_p -> sqlResult [1].value;
																							char *units_s = metadata_query_results_p -> sqlResult [2].value;


																							for (j = 0; j < metadata_query_results_p -> rowCnt; ++ j)
																								{
																									IrodsMetadata *metadata_p = AllocateIrodsMetadata (key_s, value_s, units_s, pool_p);

																									if (metadata_p)
																										{
																											APR_ARRAY_PUSH (metadata_array_p, IrodsMetadata *) = metadata_p;
																										}

																									key_s += metadata_query_results_p -> sqlResult [0].len;
																									value_s += metadata_query_results_p -> sqlResult [1].len;
																									units_s += metadata_query_results_p -> sqlResult [2].len;
																								}

																						}

																					freeGenQueryOut (&metadata_query_results_p);
																				}		/* metadata_query_results_p */

																		}		/* if (success_code == 0) */

																}		/* for (i = 0; i < meta_id_results_p -> rowCnt; ++ i) */

														}		/* if (meta_id_results_p -> attriCnt == 1) */
													else
														{

														}

												}		/* if (success_code == 0) */

											freeGenQueryOut (&meta_id_results_p);
										}		/* if (meta_id_results_p) */

								}		/* if (success_code == 0) */

						}		/* if (success_code == 0) */

				}		/* if ((select_col != -1) && (where_col != -1) && (where_value_s != NULL)) */


			SortIRodsMetadataArray (metadata_array_p, CompareIrodsMetadata);

		}		/* if (metadata_array_p) */
	else
		{

		}

	return metadata_array_p;
}


void SortIRodsMetadataArray (apr_array_header_t *metadata_array_p, int (*compare_fn) (const void *v0_p, const void *v1_p))
{
	qsort (metadata_array_p -> elts, metadata_array_p -> nelts, metadata_array_p -> elt_size, compare_fn);
}


genQueryOut_t *RunQuery (rcComm_t *connection_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, const SearchOperator *where_ops_p, size_t num_where_columns, apr_pool_t *pool_p)
{
	genQueryOut_t *out_query_p = NULL;
	genQueryInp_t in_query;
	int success_code;

	if (InitGenQuery (&in_query, NULL) == 0)
		{
			success_code = AddClausesToQuery (&in_query, select_columns_p, where_columns_p, where_values_ss, where_ops_p, num_where_columns, pool_p);

			if (success_code == 0)
				{
					int status = rcGenQuery (connection_p, &in_query, &out_query_p);

					/* Did we run it successfully? */
					if (status == 0)
						{
							//printBasicGenQueryOut (out_query_p, "result: \"%s\" \"%s\"\n");
						}
					else if (status == CAT_NO_ROWS_FOUND)
						{
							printf ("No rows found\n");
						}
					else if (status < 0 )
						{
							printf ("error status: %d\n", status);
						}

				}
		}		/* if (InitGenQuery (&in_query, NULL) == 0) */
	else
		{

		}

	ClearPooledMemoryFromGenQuery (&in_query);
	clearGenQueryInp (&in_query);

	return out_query_p;
}


static int AddClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, const SearchOperator *where_ops_p, size_t num_where_columns, apr_pool_t *pool_p)
{
	int success_code = AddSelectClausesToQuery (query_p, select_columns_p);

	if (success_code == 0)
		{
			success_code = AddWhereClausesToQuery (query_p, where_columns_p, where_values_ss, where_ops_p, num_where_columns, pool_p);
		}

	return success_code;
}


static int AddSelectClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p)
{
	int success_code = 0;

	while ((*select_columns_p != -1) && (success_code == 0))
		{
			success_code = addInxIval (& (query_p -> selectInp), *select_columns_p, 1);

			if (success_code == 0)
				{
					++ select_columns_p;
				}
			else
				{
				}
		}

	return success_code;
}


static int AddWhereClausesToQuery (genQueryInp_t *query_p, const int *where_columns_p, const char **where_values_ss, const SearchOperator *where_ops_p, size_t num_columns, apr_pool_t *pool_p)
{
	int success_code = 0;

	if (where_columns_p && where_values_ss)
		{
			while ((num_columns > 0) && (success_code == 0))
				{
					char *quoted_id_s = GetQuotedValue (*where_values_ss, where_ops_p ? *where_ops_p : SO_EQUALS, pool_p);

					if (quoted_id_s)
						{
							success_code = addInxVal (& (query_p -> sqlCondInp), *where_columns_p, quoted_id_s);

							if (success_code == 0)
								{
									++ where_columns_p;
									++ where_values_ss;

									if (where_ops_p)
										{
											++ where_ops_p;
										}

									-- num_columns;
								}
							else
								{

								}
						}
					else
						{

						}
				}
		}

	return success_code;
}


char *DoMetadataSearch (const char * const key_s, const char *value_s, const SearchOperator op, const char * const username_s, const char * const relative_uri_s, apr_pool_t *pool_p, rcComm_t *connection_p, struct apr_bucket_alloc_t *bucket_allocator_p, davrods_dir_conf_t *conf_p, request_rec *req_p, const char *davrods_path_s)
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
	char *result_s = NULL;
	apr_size_t result_length = 0;

	int num_where_columns = 2;
	int where_columns_p [] =  { COL_META_DATA_ATTR_NAME, COL_META_DATA_ATTR_VALUE };
	const char *where_values_ss [] = { key_s, value_s };
	SearchOperator ops_p [] = { SO_EQUALS, op };
	int select_columns_p [] =  { COL_META_DATA_ATTR_ID, -1, -1};
	genQueryOut_t *meta_id_results_p = NULL;
	struct HtmlTheme *theme_p = & (conf_p -> theme);
	apr_bucket_brigade *bucket_brigade_p = apr_brigade_create (pool_p, bucket_allocator_p);

	apr_status_t apr_status = PrintAllHTMLBeforeListing (theme_p, relative_uri_s, username_s, conf_p -> rods_zone, req_p, bucket_brigade_p, pool_p);

	/*
	 * SELECT meta_id FROM r_meta_main WHERE meta_attr_name = ' ' AND meta_attr_value = ' ';
	 */
	meta_id_results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, ops_p, num_where_columns, pool_p);

	if (meta_id_results_p)
		{

			if (meta_id_results_p -> attriCnt == 1)
				{
					int i;
					const int meta_results_inc = meta_id_results_p -> sqlResult [0].len;
					const char *meta_id_s = meta_id_results_p -> sqlResult [0].value;

					char *metadata_root_link_s = apr_pstrcat (pool_p, davrods_path_s, conf_p -> davrods_api_path_s, REST_METADATA_PATH_S, NULL);

					const char *exposed_root_s = GetRodsExposedPath (req_p);

					IRodsConfig irods_config;

					if (SetIRodsConfig (&irods_config, exposed_root_s, davrods_path_s, metadata_root_link_s))
						{
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

									id_results_p = RunQuery (connection_p, object_id_select_columns_p, where_columns_p, where_values_ss, NULL, 1, pool_p);

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

															collection_name_results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, num_where_columns, pool_p);

															if (collection_name_results_p)
																{
																	if (collection_name_results_p -> rowCnt > 0)
																		{
																			if (collection_name_results_p -> attriCnt == num_select_columns)
																				{
																					const char *collection_s = collection_name_results_p -> sqlResult [0].value;
																					rodsObjStat_t *stat_p;

																					stat_p = GetObjectStat (collection_s, connection_p);

																					if (stat_p)
																						{
																							IRodsObject irods_obj;

																							SetIRodsObject (&irods_obj, COLL_OBJ_T, id_s, NULL, collection_s, stat_p -> ownerName, stat_p -> modifyTime, stat_p -> objSize);

																							apr_status = PrintItem (theme_p, &irods_obj, &irods_config, bucket_brigade_p, pool_p, connection_p, req_p);

																							if (apr_status != APR_SUCCESS)
																								{

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

									id_results_p = RunQuery (connection_p, object_id_select_columns_p, where_columns_p, where_values_ss, NULL, 1, pool_p);

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
															data_id_results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, NULL, 1, pool_p);

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
																					coll_id_results_p = RunQuery (connection_p, coll_id_select_columns_p, coll_id_where_columns_p, coll_id_where_values_ss, NULL, num_coll_id_where_columns, pool_p);

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

																													stat_p = GetObjectStat (irods_data_path_s, connection_p);

																													if (stat_p)
																														{
																															IRodsObject irods_obj;

																															SetIRodsObject (&irods_obj, DATA_OBJ_T, id_s, data_name_s, collection_s, stat_p -> ownerName, stat_p -> modifyTime, stat_p -> objSize);

																															apr_status = PrintItem (theme_p, &irods_obj, &irods_config, bucket_brigade_p, pool_p, connection_p, req_p);

																															if (apr_status != APR_SUCCESS)
																																{

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

						}		/* if (SetIRodsConfig (&irods_config, exposed_root_s, root_path_s, metadata_root_link_s)) */
					else
						{

						}

				}		/* if (meta_id_results_p -> attriCnt == 1) */

			freeGenQueryOut (&meta_id_results_p);
		}		/* if (meta_id_results_p) */


	apr_status = PrintAllHTMLAfterListing (theme_p, req_p, bucket_brigade_p, pool_p);


	CloseBucketsStream (bucket_brigade_p);

	apr_status = apr_brigade_pflatten (bucket_brigade_p, &result_s, &result_length, pool_p);

	/*
	 * Sometimes there is garbage at the end of this, and I don't know which apr_brigade_...
	 * method I need to get the terminating '\0' so have to do it explicitly.
	 */
	if (* (result_s + result_length) != '\0')
		{
			* (result_s + result_length) = '\0';
		}

	apr_brigade_destroy(bucket_brigade_p);

	return result_s;
}



static rodsObjStat_t * GetObjectStat (const char * const path_s, rcComm_t *connection_p)
{
	dataObjInp_t inp;
	rodsObjStat_t *stat_p = NULL;
	int status;

	memset (&inp, 0, sizeof (dataObjInp_t));
	rstrcpy (inp.objPath, path_s, MAX_NAME_LEN);

	status = rcObjStat (connection_p, &inp, &stat_p);

	if (status < 0)
		{
			/* .... handle the error */
		}

	return stat_p;
}



static int CheckQueryResults (const genQueryOut_t * const results_p, const int min_rows, const int max_rows, const int num_attrs)
{
	int ret = 1;

	if (results_p)
		{
			if (min_rows >= 0)
				{
					ret = (results_p -> rowCnt >= min_rows) ? 1: 0;
				}

			if (ret == 1)
				{
					if (max_rows >= 0)
						{
							ret = (results_p -> rowCnt <= max_rows) ? 1: 0;
						}
				}

			if (ret == 1)
				{
					if (num_attrs >= 0)
						{
							ret = (results_p -> attriCnt == num_attrs) ? 1: 0;
						}
				}

		}
	else
		{
			ret = 0;
		}

	return ret;
}


const char *GetSearchOperatorAsString (const SearchOperator op)
{
	const char *op_s = NULL;

	switch (op)
		{
			case SO_EQUALS:
				op_s = S_SEARCH_OPERATOR_EQUALS_S;
				break;

			case SO_LIKE:
				op_s = S_SEARCH_OPERATOR_LIKE_S;
				break;

			default:
			//	ap_log_rerror  ();
				break;

		}

	return op_s;
}


apr_status_t GetSearchOperatorFromString (const char *op_s, SearchOperator *op_p)
{
	apr_status_t res = APR_BADARG;

	if (op_s)
		{
			if (strcmp (S_SEARCH_OPERATOR_EQUALS_S, op_s) == 0)
				{
					*op_p = SO_EQUALS;
					res = APR_SUCCESS;
				}
			else if (strcmp (S_SEARCH_OPERATOR_LIKE_S, op_s) == 0)
				{
					*op_p = SO_LIKE;
					res = APR_SUCCESS;
				}
		}


	return res;
}


static char *GetQuotedValue (const char * const input_s, const SearchOperator op, apr_pool_t *pool_p)
{
	char *output_s = NULL;
	const char *op_s = GetSearchOperatorAsString (op);

	if (op_s)
		{
			const size_t input_length = strlen (input_s);
			size_t l = 4 + input_length + strlen (op_s);

			/*
			 * For "like" searches, we need to append the wildcard if
			 * it is not already there
			 */
			if (op == SO_LIKE)
				{
					l += 2;
				}

			output_s = (char *) apr_palloc (pool_p, l * sizeof (char));

			if (output_s)
				{
					if (op == SO_LIKE)
						{
							sprintf (output_s, "%s \'%%%s%%\'", op_s, input_s);
						}
					else
						{
							sprintf (output_s, "%s \'%s\'", op_s, input_s);
						}

					* (output_s + l - 1) = '\0';
				}
		}


	return output_s;
}


static int InitGenQuery (genQueryInp_t *query_p, const char * const zone_s)
{
	int res = 0;

	memset (query_p, 0, sizeof (genQueryInp_t));
	query_p -> maxRows = MAX_SQL_ROWS;
	query_p -> continueInx = 0;

	//query_p -> options = UPPER_CASE_WHERE;

	if (zone_s)
		{
			res = addKeyVal (& (query_p -> condInput), ZONE_KW, zone_s);
		}

	return res;
}


static void ClearPooledMemoryFromGenQuery (genQueryInp_t *query_p)
{
	memset (query_p -> sqlCondInp.value, 0, (query_p -> sqlCondInp.len) * sizeof (char *));
}


static int SetMetadataQuery (genQueryInp_t *query_p, const char * const zone_s)
{
	int success_code;

	/* Reset out input query */
	InitGenQuery (query_p, zone_s);

	/*
	 * Add the values that we want:
	 *
	 * 	meta_attr_name
	 * 	meta_attr_value
	 * 	meta_attr_unit
	 *
	 * SELECT meta_namespace, meta_attr_name, meta_attr_value, meta_attr_unit FROM r_meta_main WHERE meta_id = ' ';
	 */

	success_code = addInxIval (& (query_p -> selectInp), COL_META_DATA_ATTR_NAME, 1);

	if (success_code == 0)
		{
			success_code = addInxIval (& (query_p -> selectInp), COL_META_DATA_ATTR_VALUE, 1);

			if (success_code == 0)
				{
					success_code = addInxIval (& (query_p -> selectInp), COL_META_DATA_ATTR_UNITS, 1);

				}
		}

	return success_code;
}


static genQueryOut_t *ExecuteGenQuery (rcComm_t *connection_p, genQueryInp_t * const in_query_p)
{
	genQueryOut_t *out_query_p = NULL;
	int status = rcGenQuery (connection_p, in_query_p, &out_query_p);

	/* Did we run it successfully? */
	if (status == 0)
		{
#if QUERY_DEBUG >= STM_LEVEL_FINER
#endif
		}
	else if (status == CAT_NO_ROWS_FOUND)
		{

			printf ("No rows found\n");
		}
	else if (status < 0 )
		{
			printf ("error status: %d\n", status);
		}
	else
		{
			//printBasicGenQueryOut (out_query_p, "result: \"%s\" \"%s\"\n");
		}

	return out_query_p;
}



/* this is a debug routine; it just prints the genQueryInp
   structure */
int printGenQI( genQueryInp_t *genQueryInp ) {
	int i, len;
	int *ip1, *ip2;
	char *cp;
	char **cpp;


	fprintf(stderr, "maxRows=%d\n", genQueryInp->maxRows );

	len = genQueryInp->selectInp.len;
	fprintf(stderr, "sel len=%d\n", len );
	ip1 = genQueryInp->selectInp.inx;
	ip2 = genQueryInp->selectInp.value;
	for ( i = 0; i < len; i++ ) {
			fprintf(stderr, "sel inx [%d]=%d\n", i, *ip1 );
			fprintf(stderr, "sel val [%d]=%d\n", i, *ip2 );
			ip1++;
			ip2++;
	}

	len = genQueryInp->sqlCondInp.len;
	fprintf(stderr, "sqlCond len=%d\n", len );
	ip1 = genQueryInp->sqlCondInp.inx;
	cpp = genQueryInp->sqlCondInp.value;
	cp = *cpp;
	for ( i = 0; i < len; i++ ) {
			fprintf(stderr, "sel inx [%d]=%d\n", i, *ip1 );
			fprintf(stderr, "sel val [%d]=:%s:\n", i, cp );
			ip1++;
			cpp++;
			cp = *cpp;
	}
	return 0;
}

void PrintBasicGenQueryOut( genQueryOut_t *genQueryOut)
{
	int i;

	for (i = 0; i < genQueryOut -> rowCnt; ++ i)
		{
			int j;

			fprintf(stderr,  "\n----\n" );

			for (j = 0; j < genQueryOut->attriCnt; j++ )
				{
					char *result_s = genQueryOut->sqlResult[j].value;
					result_s += i * genQueryOut->sqlResult[j].len;
					fprintf(stderr,  "i: %d, j: %d -> \"%s\"\n", i, j, result_s );
			}
	}

	fprintf(stderr,  "\n----\n" );
}


IrodsMetadata *AllocateIrodsMetadata (const char * const key_s, const char * const value_s, const char * const units_s, apr_pool_t *pool_p)
{
	IrodsMetadata *metadata_p = NULL;

	if (key_s)
		{
			if (value_s)
				{
					char *copied_key_s = apr_pstrdup (pool_p, key_s);

					if (copied_key_s)
						{
							char *copied_value_s = apr_pstrdup (pool_p, value_s);

							if (copied_value_s)
								{
									char *copied_units_s = NULL;

									if (units_s)
										{
											copied_units_s = apr_pstrdup (pool_p, units_s);
										}

									metadata_p = apr_palloc (pool_p, sizeof (IrodsMetadata));

									if (metadata_p)
										{
											metadata_p -> im_key_s = copied_key_s;
											metadata_p -> im_value_s = copied_value_s;
											metadata_p -> im_units_s = copied_units_s;
										}
								}
						}
				}
		}

	return metadata_p;
}


/*
 * Sort the metadata keys into alphabetical order
 */
static int CompareIrodsMetadata (const void *v0_p, const void *v1_p)
{
	int res = 0;
	IrodsMetadata *md0_p = * ((IrodsMetadata **) v0_p);
	IrodsMetadata *md1_p = * ((IrodsMetadata **) v1_p);

	res = strcasecmp (md0_p -> im_key_s, md1_p -> im_key_s);

	if (res == 0)
		{
			res = strcasecmp (md0_p -> im_value_s, md1_p -> im_value_s);
		}

	return res;
}



apr_status_t PrintMetadata (const apr_array_header_t *metadata_list_p, apr_bucket_brigade *bb_p, const char *metadata_search_link_s, apr_pool_t *pool_p)
{
	apr_status_t status = apr_brigade_puts (bb_p, NULL, NULL, "<ul class=\"metadata\">");

	if (status == APR_SUCCESS)
		{
			const int size = metadata_list_p -> nelts;
			int i;

			for (i = 0; i < size; ++ i)
				{
					const IrodsMetadata *metadata_p = APR_ARRAY_IDX (metadata_list_p, i, IrodsMetadata *);

					apr_brigade_puts (bb_p, NULL, NULL, "<li>");

					if (metadata_search_link_s)
						{
							char *escaped_key_s = ap_escape_urlencoded (pool_p, metadata_p -> im_key_s);
							char *escaped_value_s = ap_escape_urlencoded (pool_p, metadata_p -> im_value_s);

							apr_brigade_printf (bb_p, NULL, NULL, "<a href=\"%s?key=%s&amp;value=%s\">", metadata_search_link_s, escaped_key_s, escaped_value_s);
						}

					apr_brigade_printf (bb_p, NULL, NULL, "<span class=\"key\">%s</span>: <span class=\"value\">%s</span>", metadata_p -> im_key_s, metadata_p -> im_value_s);

					if (metadata_p -> im_units_s)
						{
							apr_brigade_printf (bb_p, NULL, NULL, "<span class=\"units\">%s</span>", metadata_p -> im_units_s);
						}


					if (metadata_search_link_s)
						{
							apr_brigade_puts (bb_p, NULL, NULL, "</a>");
						}


					apr_brigade_puts (bb_p, NULL, NULL, "</li>");
				}

			apr_brigade_puts (bb_p, NULL, NULL, "</ul>");

		}

	return status;
}



apr_array_header_t *GetAllDataObjectMetadataKeys (apr_pool_t *pool_p, rcComm_t *connection_p)
{
	apr_array_header_t *metadata_keys_p = NULL;
	int select_columns_p [3] = { COL_META_DATA_ATTR_NAME, COL_META_DATA_ATTR_VALUE, -1};
	genQueryOut_t *results_p = RunQuery (connection_p, select_columns_p, NULL, NULL, NULL, 0, pool_p);

	if (results_p)
		{
			if (results_p -> rowCnt > 0)
				{

				}
		}

	return metadata_keys_p;
}



