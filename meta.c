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


void PrintBasicGenQueryOut( genQueryOut_t *genQueryOut);

/*************************************/



apr_array_header_t *GetMetadata (const dav_resource *resource_p, const collEnt_t *entry_p)
{
	apr_array_header_t *metadata_array_p = apr_array_make (resource_p -> pool, S_INITIAL_ARRAY_SIZE, sizeof (IrodsMetadata *));

	if (metadata_array_p)
		{
			int select_col = -1;
			int where_col = -1;
			char *where_value_s = NULL;
			int success_code;
			genQueryInp_t in_query;

			InitGenQuery (&in_query);

			switch (entry_p -> objType)
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
						where_value_s = entry_p -> dataId;
						break;

					/*
					 * For a collection we need to do an intermediate search to get the
					 * object id from the collection name
					 *
					 * SELECT coll_id FROM r_coll_main WHERE coll_name = ' ';
					 */
					case COLL_OBJ_T:
						{
							genQueryInp_t collection_id_query;

							InitGenQuery (&collection_id_query);

							success_code = addInxIval (& (collection_id_query.selectInp), COL_COLL_ID, 1);

							if (success_code == 0)
								{
									char *quoted_id_s = GetQuotedValue (entry_p -> collName, resource_p -> pool);

									if (quoted_id_s)
										{
											success_code = addInxVal (& (collection_id_query.sqlCondInp), COL_COLL_NAME, quoted_id_s);

											if (success_code == 0)
												{
													genQueryOut_t *coll_id_results_p = NULL;

													fprintf (stderr, "collection query:");
													printGenQI (&collection_id_query);

													coll_id_results_p = ExecuteGenQuery (resource_p -> info -> rods_conn, &collection_id_query);

													if (coll_id_results_p)
														{
															fprintf (stderr, "collection results:\n");
															PrintBasicGenQueryOut (coll_id_results_p);

															if ((coll_id_results_p -> attriCnt == 1) && (coll_id_results_p -> rowCnt == 1))
																{
																	char *coll_id_s = coll_id_results_p -> sqlResult [0].value;

																	if (coll_id_s)
																		{
																			where_value_s = apr_pstrdup (resource_p -> pool, coll_id_s);

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

										}
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
							char *condition_and_where_value_s = GetQuotedValue (where_value_s, resource_p -> pool);

							success_code = addInxVal (& (in_query.sqlCondInp), where_col, condition_and_where_value_s);

							if (success_code == 0)
								{
									genQueryOut_t *meta_id_results_p = NULL;

									fprintf (stderr, "initial query:");
									printGenQI (&in_query);

									meta_id_results_p = ExecuteGenQuery (resource_p -> info -> rods_conn, &in_query);

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
																			condition_and_where_value_s = GetQuotedValue (meta_id_s, resource_p -> pool);
																			success_code = addInxVal (& (in_query.sqlCondInp), COL_META_DATA_ATTR_ID, condition_and_where_value_s);
																		}
																	else
																		{
																			condition_and_where_value_s = GetQuotedValue (meta_id_s, resource_p -> pool);

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

																			metadata_query_results_p = ExecuteGenQuery (resource_p -> info -> rods_conn, &in_query);

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
																									IrodsMetadata *metadata_p = AllocateIrodsMetadata (key_s, value_s, units_s, resource_p -> pool);

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
