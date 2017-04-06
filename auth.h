/**
 * \file
 * \brief     HTTP Basic auth provider for iRODS.
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
#ifndef _RODS_AUTH_H
#define _RODS_AUTH_H

#include "mod_auth.h"
#include "mod_davrods.h"

#include "irods/rodsConnect.h"


const char *GetDavrodsMemoryPoolKey (void);


const char *GetUsernameKey (void);


const char *GetRodsEnvKey (void);


const char *GetConnectionKey (void);


void davrods_auth_register(apr_pool_t *p);


apr_pool_t *GetDavrodsMemoryPool (request_rec *req_p);


authn_status GetIRodsConnection (request_rec *req_p, rcComm_t **connection_pp, const char *username_s, const char *password_s);


apr_status_t RodsLogout (request_rec *req_p);


#endif /* _RODS_AUTH_H */
