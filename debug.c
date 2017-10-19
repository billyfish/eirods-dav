/*
** Copyright 2014-2017 The Earlham Institute
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
 * debug.c
 *
 *  Created on: 17 Oct 2017
 *      Author: billy
 */

#include "debug.h"
#include "auth.h"

#include "http_log.h"

#include "apr_pools.h"


static const char *S_KEYS_WITH_HEX_VALUES_SS [] = { "davrods_pool", "rods_conn", NULL };


static int PrintTableEntry (void *data_p, const char *key_s, const char *value_s);

static int PrintHashEntry (void *data_p, const void *key_p, apr_ssize_t key_length, const void *value_p);

static void DebugPoolUserData (apr_pool_t *pool_p, request_rec *req_p, const char * message_s);


void DebugRequest (request_rec *req_p)
{
	apr_pool_t *davrods_mem_pool_p = NULL;

	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, req_p, "BEGIN req_p: \"%X\"", req_p);
	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, req_p, "req_p -> unparsed_uri: \"%s\"", req_p -> unparsed_uri);
	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, req_p, "req_p -> connection: \"%X\"", req_p -> connection);
	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, req_p, "req_p -> connection -> pool: \"%X\"", req_p -> connection -> pool);

	DebugAPRTable (req_p -> headers_in, "req_p -> headers_in", req_p);
	DebugAPRTable (req_p -> headers_out, "req_p -> headers_out", req_p);

	DebugAPRTable (req_p -> err_headers_out, "req_p -> err_headers_out", req_p);
	DebugAPRTable (req_p -> subprocess_env, "req_p -> subprocess_env", req_p);
	DebugAPRTable (req_p -> notes, "req_p -> notes", req_p);


	DebugPoolUserData (req_p -> connection -> pool, req_p, "req_p -> connection -> pool -> user_data");

	davrods_mem_pool_p = GetDavrodsMemoryPool (req_p);

	if (davrods_mem_pool_p)
		{
			DebugPoolUserData (davrods_mem_pool_p, req_p, "DavrodsMemoryPool");
		}

	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_SUCCESS, req_p, "END req_p: \"%X\"", req_p);
}


void DebugAPRTable (const apr_table_t *table_p, const char * const message_s, request_rec *req_p)
{
	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "BEGIN %s", message_s);

	if (table_p)
		{
			apr_table_do (PrintTableEntry, req_p, table_p, NULL);
		}

	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "END %s", message_s);
}



static void DebugPoolUserData (apr_pool_t *pool_p, request_rec *req_p, const char * message_s)
{
	/*
	 * as apr_pool_t is a private structure we have to jump to the user_data
	 * using an offset which in this case is 72 bytes
	 */
	const size_t USERDATA_OFFSET = 72;
	char *data_p = (char *) pool_p;

	DebugAPRHash (* ((apr_hash_t **) (data_p + USERDATA_OFFSET)), message_s, req_p);
}


void DebugAPRHash (const apr_hash_t *hash_p, const char * const message_s, request_rec *req_p)
{
	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "BEGIN %s", message_s);

	if (hash_p)
		{
			apr_hash_do (PrintHashEntry, req_p, hash_p);
		}

	ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "END %s", message_s);
}



static int PrintTableEntry (void *data_p, const char *key_s, const char *value_s)
{
	int res = 1;
	request_rec *req_p = (request_rec *) data_p;

	if (req_p)
		{
			ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "\"%s\"=\"%s\"", key_s, value_s);
		}
	else
		{
			res = 0;
		}

	return res;
}


static int PrintHashEntry (void *data_p, const void *key_p, apr_ssize_t key_length, const void *value_p)
{
	int res = TRUE;
	request_rec *req_p = (request_rec *) data_p;

	if (req_p)
		{
			const char *key_s = (const char *) key_p;

			int print_as_string_flag = 1;
			const char **check_ss = S_KEYS_WITH_HEX_VALUES_SS;

			while	(*check_ss && print_as_string_flag)
				{
					if (strcmp (key_s, *check_ss) == 0)
						{
							print_as_string_flag = 0;
						}
					else
						{
							++ check_ss;
						}
				}

			if (print_as_string_flag)
				{
					const char *value_s = (const char *) value_p;

					ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "\"%s\"=\"%s\"", key_s, value_s);
				}
			else
				{
					ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_INFO, APR_SUCCESS, req_p, "\"%s\"=\"%.16X\"", key_s, value_p);
				}
		}
	else
		{
			res = 0;
		}

	return res;
}
