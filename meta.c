/*
 * meta.c
 *
 *  Created on: 26 Sep 2016
 *      Author: billy
 */

#include "repo.h"
#include "rodsGenQueryNames.h"
#include "meta.h"

#include "apr_strings.h"

/*************************************/

static const int S_INITIAL_ARRAY_SIZE = 16;

static void InitGenQuery (genQueryInp_t *query_p);

static int SetMetadataQuery (genQueryInp_t *query_p);

static genQueryOut_t *ExecuteGenQuery (rcComm_t *connection_p, genQueryInp_t * const in_query_p);

static char *GetQuotedValue (const char * const input_s, apr_pool_t *pool_p);

static genQueryOut_t *RunQuery (rcComm_t *connection_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, const size_t num_where_columns, apr_pool_t *pool_p);

static int AddClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, const size_t num_where_columns, apr_pool_t *pool_p);


static int AddSelectClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p);

static int AddWhereClausesToQuery (genQueryInp_t *query_p, const int *where_columns_p, const char **where_values_ss, const size_t num_columns, apr_pool_t *pool_p);

static void ClearPooledMemoryFromGenQuery (genQueryInp_t *query_p);


void PrintBasicGenQueryOut( genQueryOut_t *genQueryOut);

/*************************************/



apr_array_header_t *GetMetadataForCollEntry (const dav_resource *resource_p, const collEnt_t *entry_p)
{
	return GetMetadata (resource_p, entry_p -> objType, entry_p -> dataId, entry_p -> collName);
}


apr_array_header_t *GetMetadata (const dav_resource *resource_p, const objType_t object_type, const char *id_s, const char *coll_name_s)
{
	apr_pool_t *pool_p = resource_p -> pool;
	rcComm_t *irods_connection_p = resource_p -> info -> rods_conn;
	apr_array_header_t *metadata_array_p = apr_array_make (pool_p, S_INITIAL_ARRAY_SIZE, sizeof (IrodsMetadata *));

	if (metadata_array_p)
		{
			int select_col = -1;
			int where_col = -1;
			const char *where_value_s = NULL;
			genQueryInp_t in_query;

			InitGenQuery (&in_query);

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
							int select_columns_p [] = { COL_COLL_ID };
							int where_columns_p [] = { COL_COLL_NAME };
							const char *where_values_ss [] = { coll_name_s };

							genQueryOut_t *coll_id_results_p = RunQuery (irods_connection_p, select_columns_p, where_columns_p, where_values_ss, 1, pool_p);

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
							char *condition_and_where_value_s = GetQuotedValue (where_value_s, pool_p);

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
											success_code = SetMetadataQuery (&in_query);

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
																			condition_and_where_value_s = GetQuotedValue (meta_id_s, pool_p);
																			success_code = addInxVal (& (in_query.sqlCondInp), COL_META_DATA_ATTR_ID, condition_and_where_value_s);
																		}
																	else
																		{
																			condition_and_where_value_s = GetQuotedValue (meta_id_s, pool_p);

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

		}		/* if (metadata_array_p) */

	return metadata_array_p;
}



static genQueryOut_t *RunQuery (rcComm_t *connection_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, size_t num_where_columns, apr_pool_t *pool_p)
{
	genQueryOut_t *out_query_p = NULL;
	genQueryInp_t in_query;
	int success_code;

	InitGenQuery (&in_query);

	success_code = AddClausesToQuery (&in_query, select_columns_p, where_columns_p, where_values_ss, num_where_columns, pool_p);

	if (success_code == 0)
		{
			int status = rcGenQuery (connection_p, &in_query, &out_query_p);

			/* Did we run it successfully? */
			if (status == 0)
				{

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

		}

	ClearPooledMemoryFromGenQuery (&in_query);
	clearGenQueryInp (&in_query);

	return out_query_p;
}


static int AddClausesToQuery (genQueryInp_t *query_p, const int *select_columns_p, const int *where_columns_p, const char **where_values_ss, size_t num_where_columns, apr_pool_t *pool_p)
{
	int success_code = AddSelectClausesToQuery (query_p, select_columns_p);

	if (success_code == 0)
		{
			success_code = AddWhereClausesToQuery (query_p, where_columns_p, where_values_ss, num_where_columns, pool_p);
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


static int AddWhereClausesToQuery (genQueryInp_t *query_p, const int *where_columns_p, const char **where_values_ss, size_t num_columns, apr_pool_t *pool_p)
{
	int success_code = 0;

	while ((num_columns > 0) && (success_code == 0))
		{
			char *quoted_id_s = GetQuotedValue (*where_values_ss, pool_p);

			if (quoted_id_s)
				{
					success_code = addInxVal (& (query_p -> sqlCondInp), *where_columns_p, quoted_id_s);

					if (success_code == 0)
						{
							++ where_columns_p;
							++ where_values_ss;
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

	return success_code;
}


void DoMetadataSearch (const char * const key_s, const char *value_s, apr_pool_t *pool_p, rcComm_t *connection_p, struct apr_bucket_alloc_t *bucket_allocator_p)
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
	int select_columns_p [] =  { COL_META_DATA_ATTR_ID, -1};
	genQueryInp_t meta_id_query;
	genQueryOut_t *meta_id_results_p = NULL;

	// Make brigade.
	apr_bucket_brigade *buckets_p = apr_brigade_create (pool_p, bucket_allocator_p);
	apr_bucket *bucket_p;

	InitGenQuery (&meta_id_query);

	/*
	 * SELECT meta_id FROM r_meta_main WHERE meta_attr_name = ' ' AND meta_attr_value = ' ';
	 */
	meta_id_results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, num_where_columns, pool_p);

	if (meta_id_results_p)
		{
			fprintf (stderr, "meta_id_results_p:\n");
			PrintBasicGenQueryOut (meta_id_results_p);

			if (meta_id_results_p -> attriCnt == 1)
				{
					int i;

					for (i = 0; i < meta_id_results_p -> rowCnt; ++ i)
						{
							int where_columns_p [] = { COL_META_DATA_ATTR_ID, -1 };
							const char *where_values_ss [] =  { meta_id_results_p -> sqlResult [i].value };
							int object_id_select_columns_p [] = { COL_D_DATA_ID, -1 };
							genQueryOut_t *object_id_results_p = RunQuery (connection_p, object_id_select_columns_p, where_columns_p, where_values_ss, 1, pool_p);

							if (object_id_results_p)
								{
									int j;

									for (j = 0; j < object_id_results_p -> rowCnt; ++ j)
										{
											char *path_s = NULL;
											int num_select_columns = 4;
											int select_columns_p [] = { COL_D_DATA_PATH, COL_D_OWNER_NAME, COL_DATA_SIZE, COL_D_MODIFY_TIME, -1};
											genQueryOut_t *data_id_results_p = NULL;
											const char *id_s = object_id_results_p -> sqlResult [j].value;

											where_values_ss [0] = id_s;
											where_columns_p [0] = COL_D_DATA_ID;

											/*
											 * The id can be the data id, coll id, etc. so we have to work
											 * out what it is
											 *
											 * Start with testing it as data object id.
											 *
											 * 		SELECT data_id, data_path, data_owner_name, data_size, modify_ts FROM r_data_main where data_id = 10001;
											 *
											 */
											data_id_results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, 1, pool_p);

											if (data_id_results_p)
												{
													if (data_id_results_p -> rowCnt > 0)
														{
															if (data_id_results_p -> attriCnt == num_select_columns)
																{
																	/* we have a data id match */
																	const char *path_s = NULL;
																	const char *collection_s = NULL;
																	const char *owner_s = NULL;
																	const char *modified_s = NULL;
																	rodsLong_t size = 0;
																	struct HtmlTheme *theme_p = NULL;
																	const dav_resource *resource_p = NULL;

																	int success_code = PrintItem (theme_p, DATA_OBJ_T, id_s, path_s, collection_s, owner_s, modified_s, size, buckets_p, pool_p, resource_p);


																}
														}

													freeGenQueryOut (&data_id_results_p);
												}

											if (!path_s)
												{
													/*
													 * See if the id refers to a collection
													 *
													 * 		SELECT coll_id, coll_name, coll_owner_name, modify_ts FROM r_coll_main WHERE coll_id = '10001';
													 *
													 */
													select_columns_p [0] = COL_COLL_NAME;
													select_columns_p [1] = COL_COLL_OWNER_NAME;
													select_columns_p [2] = COL_COLL_MODIFY_TIME;

													where_columns_p [0] = COL_D_COLL_ID;

													num_select_columns = 3;

													data_id_results_p = RunQuery (connection_p, select_columns_p, where_columns_p, where_values_ss, 1, pool_p);

													if (data_id_results_p)
														{
															if (data_id_results_p -> rowCnt > 0)
																{
																	if (data_id_results_p -> attriCnt == num_select_columns)
																		{
																			/* we have a data id match */
																		}
																}

															freeGenQueryOut (&data_id_results_p);
														}
												}


											freeGenQueryOut (&data_id_results_p);
										}

									freeGenQueryOut (&object_id_results_p);

								}		/* if (object_id_results_p) */

						}
				}
		}
}


static char *GetQuotedValue (const char * const input_s, apr_pool_t *pool_p)
{
	size_t input_length = strlen (input_s);
	char *output_s = (char *) apr_palloc (pool_p, (input_length + 5) * sizeof (char));

	sprintf (output_s, "= \'%s\'", input_s);
	* (output_s + input_length + 4) = '\0';

	return output_s;
}


static void InitGenQuery (genQueryInp_t *query_p)
{
	memset (query_p, 0, sizeof (genQueryInp_t));
	query_p -> maxRows = MAX_SQL_ROWS;
	query_p -> continueInx = 0;
}


static void ClearPooledMemoryFromGenQuery (genQueryInp_t *query_p)
{
	memset (query_p -> sqlCondInp.value, 0, (query_p -> sqlCondInp.len) * sizeof (char *));
}


static int SetMetadataQuery (genQueryInp_t *query_p)
{
	int success_code;

	/* Reset out input query */
	InitGenQuery (query_p);

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
