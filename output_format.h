/*
** Copyright 2014-2018 The Earlham Institute
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
 * output_format.h
 *
 *  Created on: 26 Jul 2018
 *      Author: billy
 */

#ifndef OUTPUT_FORMAT_H_
#define OUTPUT_FORMAT_H_

typedef enum OutputFormat
{
	OF_HTML,
	OF_JSON,
	OF_TSV,
	OF_CSV,
	OF_NUM_FORMATS
} OutputFormat;


#endif /* OUTPUT_FORMAT_H_ */
