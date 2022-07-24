/*
 * expand.c
 *
 *  Created on: Mar 29, 2021
 *      Author: nanashi
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "buffer.h"

/**
 * @brief 	Expands a chunk from the compressed stream.
 * @param 	p_compressed			Pointer to compressed stream.
 * @param	p_expanded				Expanded data is written to this pointer.
 * @return	Nothing.
 */
static uint32_t expand_chunk(uint8_t* p_compressed, uint8_t* p_expanded, int size_expanded)
{
	/* Declare variables. */
	ringb_t dict = ringb_init();
	int pos_compressed = 0;											// position in compressed stream; first 4 bytes are the expanded size
	int pos_expanded = 0;											// position in the expanded stream
	uint8_t data_byte, temp_byte1, temp_byte2, dict_byte;
	uint16_t ctrl_word;
	int cpy_len, cpy_pos;
	ctrl_word = 0;													// the control word determines the number of bytes to copy

	while(pos_expanded < size_expanded){

		ctrl_word >>= 1;											// shift out lsb of control word

		/* Load a new control word every 8 bytes. */
		if (!(ctrl_word & 0x100)){
			ctrl_word = (uint16_t) p_compressed[pos_compressed++];
			ctrl_word |= 0xff00;
		}

		/* If lsb bit flag of the control is set copy a byte
		 * from the input to the output stream. If not then
		 * read length-distance pair and copy data from the
		 * dictionary. */
		if (ctrl_word & 0x1){
			data_byte = p_compressed[pos_compressed++];
			p_expanded[pos_expanded++] = data_byte;
			ringb_insert(&dict, data_byte);
		} else {

			/* Read length-distance pair. */
			temp_byte1 = p_compressed[pos_compressed++];
			temp_byte2 = p_compressed[pos_compressed++];
			cpy_len = (int) (temp_byte2 & 0xf) + 3;
			cpy_pos = ((int) (temp_byte2& 0xf0)) >> 4;
			cpy_pos |= (int) (temp_byte1 << 4);

			/* Copy data from dictionary. */
			while (cpy_len-- > 0){
				dict_byte = ringb_get(&dict, cpy_pos++);
				p_expanded[pos_expanded++] = dict_byte;
				ringb_insert(&dict, dict_byte);
			}
		}
	}

	return pos_expanded;
}

int raw_expand(FILE *f_infile, FILE *f_outfile, int size)
{

	/* read file into buffer */
	fseek(f_infile, 0, SEEK_END);
	size_t fsize = ftell(f_infile);
	fseek(f_infile, 0, SEEK_SET);
	uint8_t* p_compressed = malloc(fsize);
	if (p_compressed == NULL) return EXIT_FAILURE;
	fread(p_compressed, sizeof(uint8_t), fsize, f_infile);

	uint8_t *p_expanded = calloc(size, sizeof(uint8_t));
	if (p_expanded == NULL){
		free(p_compressed);
		return EXIT_FAILURE;
	}

	uint32_t size_expanded = expand_chunk(p_compressed, p_expanded, size);
	fwrite(p_expanded, sizeof(uint8_t), size_expanded, f_outfile);
	free(p_compressed);
	free(p_expanded);

	return EXIT_SUCCESS;
}

/**
 * @brief	Expands chunks from *.PVB file
 * @param	f_infile		Input File.
 * @param	mode			Mode selection string. "start:end" or "all".
 * @return 	Status
 */
int expand_files(FILE* f_infile, const char* mode, const char* folder)
{
	FILE* f_outfile;
	char fname[2060];
	uint32_t size_expanded;
	int start, end;
	uint8_t* p_expanded;

	/* read file into buffer */
	fseek(f_infile, 0, SEEK_END);
	size_t fsize = ftell(f_infile);
	fseek(f_infile, 0, SEEK_SET);
	uint8_t* p_compressed = malloc(fsize);
	if(p_compressed == NULL) return EXIT_FAILURE;
	fread(p_compressed, sizeof(uint8_t), fsize, f_infile);

	/* check magic bytes */
	char magic[5];
	memcpy(magic, p_compressed, 4);
	magic[4] = 0;
	if (strcmp(magic, "MBIN")){
		printf("%s is not a valid PVB file.\n",fname);
		free(p_compressed);
		return EXIT_FAILURE;
	}

	/* set meta data */
	uint32_t file_num = *((uint32_t*) (p_compressed+4));
	uint32_t* p_fpos_base = (uint32_t*) (p_compressed + *((uint32_t*) (p_compressed+0xc)));
	uint32_t* p_fsize_base = (uint32_t*) (p_compressed + *((uint32_t*) (p_compressed+0x14)));

	/* Decode mode argument. */
	char* _mode = strdup(mode);
	char* str_start = strtok(_mode, ":");
	char* str_end = strtok(NULL, ":");
	if (strcmp(str_start, "all")){

		if (strlen(str_start) == 0)
			start = 0;
		else
			start = atoi(str_start);

		if (strlen(str_start) == 0)
			end = 10;
		else
			end = atoi(str_end);
	} else {
		start = 0;
		end = file_num;
	}

	if (end > file_num) end = file_num;

	for (int image = start; image < end; image++){
		sprintf(fname, "%s%04d.PVM", folder, image);

		if ((f_outfile=fopen(fname,"wb"))==NULL) {
			printf("Error opening output %s. Make sure the target folder exists.\n",fname);
			free(p_compressed);
			return EXIT_FAILURE;
		}

		printf("Expanding %s\n", fname);

		/* Allocate memory. */
		p_expanded = calloc(p_fsize_base[image], sizeof(uint8_t));
		if (p_expanded == NULL){
			free(p_compressed);
			return EXIT_FAILURE;
		}

		/* Expand chunk and write to file. */
		size_expanded = expand_chunk(p_compressed+p_fpos_base[image], p_expanded, (int) p_fsize_base[image]);
		fwrite(p_expanded, sizeof(uint8_t), size_expanded, f_outfile);

		free(p_expanded);

		fclose(f_outfile);
	}
	free(p_compressed);

	return EXIT_SUCCESS;
}
