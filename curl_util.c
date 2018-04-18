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
 * curl_tool.c
 *
 *  Created on: 13 Apr 2018
 *      Author: billy
 */


/*
** Copyright 2014-2016 The Earlham Institute
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
 * This code is based on the simplessl.c example code from http://curl.haxx.se/libcurl/c/simplessl.html
 */


/***************************************************************************/

#include "curl_util.h"

#include <string.h>
#include <stdlib.h>

#include <curl/easy.h>

#include "httpd.h"
#include "http_log.h"

#include "apr_strings.h"

#include "common.h"


/*
#ifdef _DEBUG
	#define CURL_TOOLS_DEBUG	(STM_LEVEL_FINE)
#else
	#define CURL_TOOLS_DEBUG	(STM_LEVEL_NONE)
#endif
*/


typedef struct CURLParam
{
	CURLoption cp_opt;
	const char *cp_value_s;
} CURLParam;





/**
 * Allocate a new CURL object to a give CurlUtil and add optionally add a
 * write to buffer callback.
 *
 * @param tool_p The CurlUtil to set up the CURL connection for.
 * @param buffer_p If this is not NULL, then set the curl object
 * to write its response to this buffer. This can be NULL <code>NULL</code>.
 * @see AddCurlCallback
 * @return <code>true</code> upon success or <code>false</code> upon error.
 */
static bool SetupCurl (CurlUtil *tool_p, apr_pool_t *pool_p);


/**
 * Free a CURL object.
 *
 * @param curl_p The CURL object to free.
 */
static void FreeCurl (CURL *curl_p);

/**
 * @brief Set the cryptographic engine to use.
 *
 * @param curl_p The CurlUtil instance to set the SSL engine for.
 * @param cryptograph_engine_name_s The name of the cryptographic engine
 * to use. For the valid names see the CURL documentation.
 * @return <code>true</code> if the SSL engine name was set successfully,
 * <code>false</code> otherwise.
 * @memberof CurlUtil
 */
static bool SetSSLEngine (CurlUtil *curl_p, const char *cryptograph_engine_name_s);


/**
 * Add a callback to write the HTTP response for this CURL object to
 * the given buffer.
 *
 * @param buffer_p The ByteBuffer which will store the CURL object's response. *
 * @param curl_p The CurlUtil object to add the callback for.
 * @return <code>true</code> if the CurlUtil was updated successfully,
 * <code>false</code> otherwise.
 * @memberof CurlUtil
 */
static bool AddCurlCallback (CurlUtil *curl_tool_p, apr_pool_t *pool_p);


/**
 * Set the URI that the CurlUtil will try to get the html data from.
 *
 * @param tool_p The CurlUtil to update.
 * @param uri_s The URI to set the CurlUtil for.
 * @return <code>true</code> if the CurlUtil was updated successfully,
 * <code>false</code> otherwise.
 * @memberof CurlUtil
 */
static bool SetUriForCurlUtil (CurlUtil *tool_p, const char * const uri_s);


/**
 * @brief Run a CurlUtil.
 * This will get the CurlUtil to get all of the data from its given URI.
 *
 * @param tool_p The CurlUtil to run.
 * @return CURLE_OK if successful or an error code upon failure.
 * @memberof CurlUtil
 */
static CURLcode RunCurlUtil (CurlUtil *tool_p);


/**
 * @brief Set up a CurlUtil to do a JSON post when it is run.
 *
 * @param tool_p The CurlUtil to update.
 * @return <code>true</code> if the CurlUtil was updated successfully,
 * <code>false</code> otherwise.
 * @memberof CurlUtil
 */
static bool SetCurlUtilForJSONPost (CurlUtil *tool_p);


/**
 * @brief Get the downloaded data from a CurlUtil.
 *
 * If the CurlUtil has been run successfully, this will get a read-only
 * version of the downloaded data. RunCurlUtil() must have been
 * run prior to this.
 * @param tool_p The CurlUtil to get the data from.
 * @return The downloaded data or <code>NULL</code> upon error.
 * @see RunCurlUtil
 * @memberof CurlUtil
 */
static char *GetCurlUtilData (CurlUtil *tool_p);

/**
 * Add a key value pair to the request header that a CurlUtil will send when it is run.
 *
 * @param tool_p The CurlUtil to add the header to
 * @param key_s The key to add. This will be copied so this value does not need to stay in scope.
 * @param value_s The value to add. This will be copied so this value does not need to stay in scope.
 * @return <code>true</code> if the header values were set successfully, <code>false</code>
 * otherwise.
 * @memberof CurlUtil
 */
static bool SetCurlUtilHeader (CurlUtil *tool_p, const char *key_s, const char *value_s);




static size_t WriteToMemoryCallback (char *response_data_p, size_t block_size, size_t num_blocks, void *store_p);



CurlUtil *AllocateCurlUtil (request_rec *req_p, apr_pool_t *pool_p)
{
	apr_bucket_brigade *buffer_p = apr_brigade_create (pool_p, req_p -> connection -> bucket_alloc);

	if (buffer_p)
		{
			CurlUtil *curl_tool_p = (CurlUtil *) malloc (sizeof (CurlUtil));

			if (curl_tool_p)
				{
					curl_tool_p -> ct_buffer_p = buffer_p;
					curl_tool_p -> ct_form_p = NULL;
					curl_tool_p -> ct_last_field_p = NULL;
					curl_tool_p -> ct_headers_list_p = NULL;
					curl_tool_p -> ct_pool_p = pool_p;
					curl_tool_p -> ct_req_p = req_p;

					if (SetupCurl (curl_tool_p, pool_p))
						{
							return curl_tool_p;
						}

					free (curl_tool_p);
				}		/* if (curl_tool_p) */

		}		/* if (buffer_p) */

	return NULL;
}



bool ResetCurlUtil (CurlUtil *tool_p)
{
	bool success_flag = false;
	apr_bucket_brigade *buffer_p = apr_brigade_create (tool_p -> ct_pool_p, tool_p -> ct_req_p -> connection -> bucket_alloc);

	if (buffer_p)
		{
			tool_p -> ct_buffer_p = buffer_p;
			success_flag = true;
		}

	return success_flag;
}


void FreeCurlUtil (CurlUtil *curl_tool_p)
{
	FreeCurl (curl_tool_p -> ct_curl_p);

	if (curl_tool_p -> ct_headers_list_p)
		{
			curl_slist_free_all (curl_tool_p -> ct_headers_list_p);
		}

	free (curl_tool_p);
}


static bool SetupCurl (CurlUtil *tool_p, apr_pool_t *pool_p)
{
	tool_p -> ct_curl_p = curl_easy_init ();

	if (tool_p -> ct_curl_p)
		{
			if (pool_p)
				{
					if (AddCurlCallback (tool_p, pool_p))
						{
							#if CURL_TOOLS_DEBUG >= STM_LEVEL_FINER
							curl_easy_setopt (tool_p -> ct_curl_p, CURLOPT_VERBOSE, 1L);
							#endif

							return true;
						}
					else
						{
							ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to add buffer callback for curl object\n");
						}
				}

			FreeCurl (tool_p -> ct_curl_p);
			tool_p -> ct_curl_p = NULL;
		}
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_ENOMEM, pool_p, "Failed to create curl object");
		}

	return false;
}


static void FreeCurl (CURL *curl_p)
{
	curl_easy_cleanup (curl_p);
}


char *SimpleCallGetRequest (request_rec *req_p, apr_pool_t *pool_p, const char *uri_s)
{
	char *result_s = NULL;
	CurlUtil *tool_p = AllocateCurlUtil (req_p, pool_p);

	if (tool_p)
		{
			result_s = CallGetRequest (tool_p, uri_s);
			FreeCurlUtil (tool_p);
		}


	return result_s;
}


char *CallGetRequest (CurlUtil *tool_p, const char *uri_s)
{
	char *result_s = NULL;

	if (SetUriForCurlUtil (tool_p, uri_s))
		{
			CURLcode res = RunCurlUtil (tool_p);

			if (res == CURLE_OK)
				{
					result_s = GetCurlUtilData (tool_p);
				}
			else
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, tool_p -> ct_pool_p,  "Failed to get data for CurlUtil from \"%s\", %s", uri_s, curl_easy_strerror (res));
				}
		}
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, tool_p -> ct_pool_p,  "Failed to set CurlUtil to call \"%s\"", uri_s);
		}

	return result_s;
}


bool SetUriForCurlUtil (CurlUtil *tool_p, const char * const uri_s)
{
	bool success_flag = false;
	CURLcode res = curl_easy_setopt (tool_p -> ct_curl_p, CURLOPT_URL, uri_s);

	if (res == CURLE_OK)
		{
			success_flag = true;
		}

	return success_flag;
}



CURLcode RunCurlUtil (CurlUtil *tool_p)
{
	CURLcode res = CURLE_OK;

	if (tool_p -> ct_headers_list_p)
		{
			res = curl_easy_setopt (tool_p -> ct_curl_p, CURLOPT_HTTPHEADER, tool_p -> ct_headers_list_p);
		}

	if (res == CURLE_OK)
		{
			res = curl_easy_perform (tool_p -> ct_curl_p);
		}

	return res;
}


static bool SetSSLEngine (CurlUtil *curl_p, const char *cryptograph_engine_name_s)
{
	bool success_flag = false;

	/* load the crypto engine */
	if (curl_easy_setopt (curl_p -> ct_curl_p, CURLOPT_SSLENGINE, cryptograph_engine_name_s) == CURLE_OK)
		{
			/* set the crypto engine as default */
			/* only needed for the first time you load a engine in a curl object... */
			if (curl_easy_setopt (curl_p -> ct_curl_p, CURLOPT_SSLENGINE_DEFAULT, 1L) == CURLE_OK)
				{
					success_flag = true;
				}
			else
				{
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, curl_p -> ct_pool_p,  "can't set crypto engine as default\n");
				}
		}
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, curl_p -> ct_pool_p, "can't set crypto engine %s\n", cryptograph_engine_name_s);
		}

	return success_flag;
}


static bool AddCurlCallback (CurlUtil *curl_tool_p, apr_pool_t *pool_p)
{
	bool success_flag = true;

	curl_write_callback callback_fn = WriteToMemoryCallback;
	const CURLParam params [] =
		{
			{ CURLOPT_WRITEFUNCTION, (const char *)  callback_fn },
			{ CURLOPT_WRITEDATA, (const char *) (curl_tool_p -> ct_buffer_p) },

			/* set default user agent */
	    { CURLOPT_USERAGENT,  "libcurl-agent/1.0" },

	    /* set timeout */
	   // { CURLOPT_TIMEOUT, (const char *) 5 },

	    /* enable location redirects */
	    { CURLOPT_FOLLOWLOCATION, (const char *) 1},

	    /* set maximum allowed redirects */
	    { CURLOPT_MAXREDIRS, (const char *) 1 },

			{ CURLOPT_LASTENTRY, (const char *) NULL }
		};

	const CURLParam *param_p = params;


	while (success_flag && (param_p -> cp_value_s))
		{
			CURLcode ret = curl_easy_setopt (curl_tool_p -> ct_curl_p, param_p -> cp_opt, (void *) param_p -> cp_value_s);

			if (ret == CURLE_OK)
				{
					++ param_p;
				}
			else
				{
					success_flag = false;
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, curl_tool_p -> ct_pool_p,  "Failed to to set CURL option \"%d\"\n", param_p -> cp_opt);
				}

		}		/* while (continue_flag && param_p) */


	return success_flag;
}



static bool SetCurlUtilHeader (CurlUtil *tool_p, const char *key_s, const char *value_s)
{
	bool success_flag = false;
	char *header_s = apr_pstrcat (tool_p -> ct_pool_p, key_s, ":", value_s, NULL);

	if (header_s)
		{
			tool_p -> ct_headers_list_p = curl_slist_append (tool_p -> ct_headers_list_p, header_s);

			success_flag = true;
		}
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, tool_p -> ct_pool_p,  "Failed to construct header for \"%s\" and \"%s\"", key_s, value_s);
		}

	return success_flag;
}


static bool SetCurlUtilForJSONPost (CurlUtil *tool_p)
{
	bool success_flag = true;

	const CURLParam params [] =
		{
///			{ CURLOPT_POST, 1 },
			{ CURLOPT_HTTPHEADER, (const char *) (tool_p -> ct_headers_list_p)  },
			{ CURLOPT_LASTENTRY, (const char *) NULL }
		};

	const CURLParam *param_p = params;

	tool_p -> ct_headers_list_p = curl_slist_append (tool_p -> ct_headers_list_p, "Accept: application/json");
	tool_p -> ct_headers_list_p = curl_slist_append (tool_p -> ct_headers_list_p, "Content-Type: application/json");

	while (success_flag && (param_p -> cp_value_s))
		{
			CURLcode ret = curl_easy_setopt (tool_p, param_p -> cp_opt, param_p -> cp_value_s);

			if (ret == CURLE_OK)
				{
					++ param_p;
				}
			else
				{
					success_flag = false;
					ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, tool_p -> ct_pool_p,  "Failed to set CURL option \"%s\" to \"%s\"\n", param_p -> cp_opt, param_p -> cp_value_s);
				}

		}		/* while (continue_flag && param_p) */

	return success_flag;
}


char *GetURLEscapedString (CurlUtil *tool_p, const char *src_s)
{
	char *escaped_s = curl_easy_escape (tool_p -> ct_curl_p, src_s, 0);

	if (!escaped_s)
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, tool_p -> ct_pool_p,  "Failed to created URL escaped version of \"%s\"\n", src_s);
		}

	return escaped_s;
}


void FreeURLEscapedString (char *value_s)
{
	curl_free (value_s);
}




char *GetCurlUtilData (CurlUtil *tool_p)
{
	char *result_s;
	apr_size_t result_length;
	apr_status_t status;

	CloseBucketsStream (tool_p -> ct_buffer_p);

	status = apr_brigade_pflatten (tool_p -> ct_buffer_p, &result_s, &result_length, tool_p -> ct_pool_p);


	/*
	 * Sometimes there is garbage at the end of this, and I don't know which apr_brigade_...
	 * method I need to get the terminating '\0' so have to do it explicitly.
	 */
	if (* (result_s + result_length) != '\0')
		{
			* (result_s + result_length) = '\0';
		}

	return result_s;
}





static size_t WriteToMemoryCallback (char *response_data_s, size_t block_size, size_t num_blocks, void *store_p)
{
	size_t total_size = block_size * num_blocks;
	size_t result = total_size;
	apr_bucket_brigade *buffer_p = (apr_bucket_brigade *) store_p;

	apr_status_t status = apr_brigade_write (buffer_p, NULL, NULL, response_data_s, total_size);

	if (status != APR_SUCCESS)
		{
			result = 0;
		}

	return result;
}
