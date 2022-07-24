/*
 * dc_rah2_comp_tool.h
 *
 *  Created on: Mar 29, 2021
 *      Author: nanashi
 */

#ifndef DC_CCT_COMP_TOOL_H_
#define DC_CCT_COMP_TOOL_H_

#include <stdio.h>
#include <stdint.h>

/**
 * @brief	Expands chunks from *.CGD file
 * @param	f_infile		Input File.
 * @param	mode			Mode selection string. "start:end" or "all".
 * @return 	Status
 */
int expand_files(FILE* f_infile, const char* mode, const char* folder);
int raw_expand(FILE *f_infile, FILE *f_outfile, int size);
int raw_compress(FILE *f_infile, FILE *f_outfile);
int compress_folder(FILE* fcompressed, const char* dir);

#endif /* DC_CCT_COMP_TOOL_H_ */
