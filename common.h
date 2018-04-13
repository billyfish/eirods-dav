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
#ifndef _DAVRODS_COMMON_H_
#define _DAVRODS_COMMON_H_

#include "apr_buckets.h"

#include "mod_davrods.h"
#include "config.h"

#include "irods/rcConnect.h"
#include "apr_tables.h"


// I'm not sure why, but the format string apr.h generates on my machine (%lu)
// causes compiler warnings. It seems that gcc wants us to use 'llu' instead,
// however the apr sprintf function does not support the long-long notation.
// -Wformat warnings related to %lu parameters can probably be ignored.
#define DAVRODS_SIZE_T_FMT APR_SIZE_T_FMT
//#define DAVRODS_SIZE_T_FMT "llu"

/**
 * \brief Get the iRODS error message for the given iRODS status code.
 *
 * \param rods_error_code
 *
 * \return an error description provided by iRODS
 */
const char *get_rods_error_msg(int rods_error_code);


void davrods_dav_register(apr_pool_t *p);


void CloseBucketsStream (apr_bucket_brigade *bucket_brigade_p);


apr_status_t PrintBasicStringToBucketBrigade (const char *value_s, apr_bucket_brigade *brigade_p, request_rec *req_p, const char *file_s, const int line);

apr_status_t PrintFileToBucketBrigade (const char *filename_s, apr_bucket_brigade *brigade_p, request_rec *req_p, const char *file_s, const int line);

apr_status_t PrintWebResponseToBucketBrigade (const char *uri_s, const char * const current_id_s, apr_bucket_brigade *brigade_p, rcComm_t *connection_p, request_rec *req_p, const char *file_s, const int line);


rcComm_t *GetIRODSConnectionFromPool (apr_pool_t *pool_p);

rcComm_t *GetIRODSConnectionForPublicUser (request_rec *req_p, apr_pool_t *davrods_pool_p, davrods_dir_conf_t *conf_p);


rodsEnv *GetRodsEnvFromPool (apr_pool_t *pool_p);


const char *GetUsernameFromPool (apr_pool_t *pool_p);

apr_table_t *MergeAPRTables (apr_table_t *table1_p, apr_table_t *table2_p, apr_pool_t *pool_p);


const char *GetParameterValue (apr_table_t *params_p, const char * const param_s, apr_pool_t *pool_p);

#endif /* _DAVRODS_COMMON_H_ */
