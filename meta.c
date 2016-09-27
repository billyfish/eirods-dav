/*
 * meta.c
 *
 *  Created on: 26 Sep 2016
 *      Author: billy
 */

#include "repo.h"

/*************************************/

static void InitGenQuery (genQueryInp_t *query_p);

static genQueryOut_t *ExecuteQueryString (rcComm_t *connection_p, char *query_s);

static genQueryOut_t *ExecuteGenQuery (rcComm_t *connection_p, genQueryInp_t * const in_query_p);

/*************************************/

void GetMetadataForDataObject (const dav_resource *resource_p, const char * const data_id_s, struct apr_bucket_alloc_t *bucket_alloc_p)
{
	if (resource_p -> info -> rods_conn)
		{
	    apr_bucket_brigade *bucket_brigade_p = apr_brigade_create (resource_p -> pool, bucket_alloc_p);

	    if (bucket_brigade_p)
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
	    		apr_status_t status = apr_brigade_printf (bucket_brigade_p, NULL, NULL, "SELECT meta_id FROM r_objt_metamap WHERE object_id = '%s'", data_id_s);

	    		if (status == APR_SUCCESS)
	    			{
	    				char *query_s = NULL;
	    				apr_size_t query_length = 0;

	    				status = apr_brigade_pflatten (bucket_brigade_p, &query_s, &query_length, resource_p -> pool);

	    				if (status == APR_SUCCESS)
	    					{
	  	    				genQueryOut_t *out_query_p = ExecuteQueryString (resource_p -> info -> rods_conn, query_s);

	  	    				if (out_query_p)
	  	    					{
	  	    						int i;

	  	    						for (i = 0; i < out_query_p -> rowCnt; ++ i)
	  	    							{
	  	    								int j;

	  	    								/*
	  	    								 * Since our search was just for the meta_id values
	  	    								 * there should only be one attribute
	  	    								 */
	  	    								if (out_query_p -> attriCnt == 1)
	  	    									{

	  	  											/*
	  	  											 * Now we iterate over each of the meta_id keys,
	  	  											 * and get the metadata values
	  	  											 *
	  	  											 * SELECT meta_namespace, meta_attr_name, meta_attr_value, meta_attr_unit FROM r_meta_main WHERE meta_id = ' ';
	  	  											 */
	  	    										char *meta_id_s = out_query_p -> sqlResult [0].value;
	  	    									}

	  	    							}


	  	    					}

	    					}

	    			}

	    		apr_brigade_destroy (bucket_brigade_p);
	    	}		/* if (bucket_p) */


		}
}


static void InitGenQuery (genQueryInp_t *query_p)
{
	memset (query_p, 0, sizeof (genQueryInp_t));
	query_p -> maxRows = MAX_SQL_ROWS;
	query_p -> continueInx = 0;
}




static genQueryOut_t *ExecuteQueryString (rcComm_t *connection_p, char *query_s)
{
	genQueryInp_t in_query;
	genQueryOut_t *out_query_p = NULL;
	int status;

	/* Build the query */
	InitGenQuery (&in_query);

	/* Fill in the iRODS query structure */
	status = fillGenQueryInpFromStrCond (query_s, &in_query);

	if (status >= 0)
		{
			out_query_p = ExecuteGenQuery (connection_p, &in_query);
		}		/* if (status >= 0) */

	return out_query_p;
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
