/*
 * compress.c
 *
 *  Created on: Mar 29, 2021
 *      Author: nanashi
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <windows.h>
#include <math.h>
#include "buffer.h"

typedef struct {
	int len;
	int pos;
} match_t;

static int compare_str(const void *a,const void *b)
{
	return (strcmp((char *)a,(char *)b));
}

static match_t find_match(ringb_t dict, winb_t lookahead)
{
	int lookahead_empty = 0;
	int dict_empty = 0;
	int full_match = 1;
	ringb_t _dict;
	match_t match;
	match.len = 0;

	/* Compare dict and lookahead data and find the longest match. */
	_dict = dict;
	while (!dict_empty && !lookahead_empty){
		if (ringb_pop(&dict, &dict_empty) != winb_pop(&lookahead, &lookahead_empty)){
			full_match = 0;
			break;
		}
		match.len++;
	}

	/* Look for repeats. */
	if (full_match){
		dict = _dict;
		while (!lookahead_empty){

			if (ringb_pop(&dict, &dict_empty) != winb_pop(&lookahead, &lookahead_empty))
				break;
			match.len++;

			if (dict_empty)
				dict = _dict;
		}
	}

	return match;
}

static match_t find_best_match(ringb_t dict, winb_t lookahead)
{
	match_t best_match, match;
	best_match.len = 0;
	ringb_t match_dict = dict;
	match_dict.tail = (dict.head - 1) & 0xfff;

	/* Go through the entire dictionary and test
	 * each position for the best match */
	while (1){
		match = find_match(match_dict, lookahead);

		if (match.len > best_match.len) {
			best_match.len = match.len;
			best_match.pos = match_dict.tail;

			/* Maximum size is 17 because nibble size + 3. */
			if (best_match.len > 17){
				best_match.len = 18;
				break;
			}
		}

		if (match_dict.tail == dict.tail)
			break;

		match_dict.tail = (match_dict.tail - 1) & 0xfff;				// propagate backwards through the buffer
	}

	return best_match;
}

static int compress_chunk(uint8_t* p_source, uint8_t* p_compressed, unsigned int size_src)
{
	uint8_t* p_comp_base = p_compressed;
	int is_empty = 0;
	int padding, cntr;
	uint16_t ctrl_word;													// control word to indicate when to copy data from dict
	uint8_t* p_ctrl_word;												// pointer to next control word in compressed stream
	match_t match;														// match between dict and lookahead buffer
	uint8_t temp_byte;

	/* Initialize ring buffers */
	ringb_t dict = ringb_init();
	winb_t lookahead = winb_init(p_source, size_src);

	ctrl_word = 0;
	p_ctrl_word = p_compressed;
	p_compressed++;
	cntr = 8;
	while (!is_empty){

		match = find_best_match(dict, lookahead);						// search best match between dict and lookahead

		if (match.len > 2){

			/* Generate length-distance pair. */
			*p_compressed++ = (uint8_t) ((match.pos & 0xff0) >> 4);
			*p_compressed = (uint8_t) ((match.pos & 0xf) << 4);
			*p_compressed++ |= (uint8_t) ((match.len - 3) & 0xf);

			/* Copy matched data to dict. */
			while (match.len--){
				ringb_insert(&dict, winb_advance(&lookahead, &is_empty));
			}

		} else {
			/* Indicate unmatched byte in control word and
			 * write to dict. */
			ctrl_word |= 0x100;
			temp_byte = winb_advance(&lookahead, &is_empty);
			*p_compressed++ = temp_byte;
			ringb_insert(&dict, temp_byte);
		}
		ctrl_word >>= 1;

		/* Control word is written every 8 entries. */
		if (!(--cntr)){
			cntr = 8;
			*p_ctrl_word = (uint8_t) (ctrl_word & 0xff);
			p_ctrl_word = p_compressed;
			p_compressed++;
		}
	}
	if (cntr == 8)
		p_compressed--;
	else
		*p_ctrl_word = (uint8_t) ((ctrl_word >> cntr) & 0xff);
	padding = (p_compressed - p_comp_base) % 4;
	padding = (padding > 0) ? 4 - padding : 0;
	while (padding--)
		*p_compressed++ = 0;

	return p_compressed - p_comp_base;
}

static int compress_file(FILE* f_infile, uint8_t* p_compressed, uint32_t* p_fsize_table)
{
	int rtn = 0;
	size_t f_size;
	uint8_t* p_source;

	/* find file size */
	fseek(f_infile, 0, SEEK_END);
	f_size = ftell(f_infile);
	*p_fsize_table = (uint32_t) f_size;
	fseek(f_infile, 0, SEEK_SET);

	/* allocate memory */
	p_source = malloc(f_size);
	fread(p_source, sizeof(uint8_t), f_size, f_infile);

	rtn = compress_chunk(p_source, p_compressed, (unsigned int) f_size);

	free(p_source);

	return rtn;
}

int raw_compress(FILE *f_infile, FILE *f_outfile)
{
	uint32_t dump;
	int csize;
	uint8_t *p_compressed = malloc(20971520);
	if (p_compressed == NULL) return EXIT_FAILURE;
	csize = compress_file(f_infile, p_compressed, &dump);
	fwrite(p_compressed, sizeof(uint8_t), csize, f_outfile);
	free(p_compressed);

	return EXIT_SUCCESS;
}

int compress_folder(FILE* fcompressed, const char* dir)
{
	void *malloc_tble[4];
	int malloc_cntr = 0;
	uint8_t zeros[0x20] = {0};
	uint32_t header[8] = {0};
	int num_files = 0;
	int comp_size = 0;
	int mult, padding, fcomp_size, i;
	FILE *fimage;
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;
	char sPath[MAX_PATH], sfile[MAX_PATH], *sfile_lst;
	sprintf(sPath, "%s*.PVM", dir);
	uint8_t *p_compressed;
	uint32_t *p_table_sizes_base, *p_table_sizes;
	uint32_t *p_table_offsets_base, *p_table_offsets;

	/* Count number of files in source folder. */
    if((hFind = FindFirstFile(sPath, &FindFileData)) == INVALID_HANDLE_VALUE)
    {
        printf("Path not found: [%s]\n", dir);
        FindClose(hFind);
        return EXIT_FAILURE;
    }
    num_files = 1;
	while(FindNextFile(hFind, &FindFileData)){
		num_files++;
	}

	/* Read file names and sort them */
	sfile_lst = malloc(MAX_PATH*num_files);
	if (sfile_lst == NULL){
		FindClose(hFind);
		return EXIT_FAILURE;
	} else {
		malloc_tble[malloc_cntr++] = sfile_lst;
	}
	i = 0;
	hFind = FindFirstFile(sPath, &FindFileData);
	do {
		memcpy(sfile_lst+(MAX_PATH*i), FindFileData.cFileName, MAX_PATH);
		i += 1;
	} while(FindNextFile(hFind, &FindFileData));
	hFind = FindFirstFile(sPath, &FindFileData);
	qsort(sfile_lst, num_files, MAX_PATH, compare_str);

	/* Allocate memory for pointer table and compressed data. */
	p_table_sizes_base = malloc(num_files*4);
	if (p_table_sizes_base == NULL){
		for (i=0;i<malloc_cntr;i++) free(malloc_tble[i]);
		FindClose(hFind);
		return EXIT_FAILURE;
	} else {
		malloc_tble[malloc_cntr++] = p_table_sizes_base;
	}
	p_table_sizes = p_table_sizes_base;
	p_table_offsets_base = malloc(num_files*4);
	if (p_table_offsets_base == NULL){
		for (i=0;i<malloc_cntr;i++) free(malloc_tble[i]);
		FindClose(hFind);
		return EXIT_FAILURE;
	} else {
		malloc_tble[malloc_cntr++] = p_table_offsets_base;
	}
	p_table_offsets = p_table_offsets_base;
	p_compressed = malloc(20971520);
	if (p_compressed == NULL){
		for (i=0;i<malloc_cntr;i++) free(malloc_tble[i]);
		FindClose(hFind);
		return EXIT_FAILURE;
	} else {
		malloc_tble[malloc_cntr++] = p_compressed;
	}

	for (i=0;i<num_files;i++){
		sprintf(sfile, "%s%s", dir, sfile_lst+(i*MAX_PATH));
		printf("Compressing %s\n", sfile_lst+(i*MAX_PATH));
		fimage = fopen(sfile,"rb");
		*p_table_offsets = (uint32_t) comp_size;
		p_table_offsets++;
		fcomp_size = compress_file(fimage, p_compressed+comp_size, p_table_sizes++);
		comp_size += fcomp_size;
		mult = (int) ceil((float) fcomp_size / 32.0);
		padding = (mult*0x20) - fcomp_size;
		memcpy(p_compressed+comp_size, zeros, padding);
		comp_size += padding;
		fclose(fimage);
	};

	/* Write header */
	int comp_start = 0x20+(num_files*8);
	mult = (int) ceil((float) comp_start / 32.0);
	padding = (mult*0x20) - comp_start;
	memcpy(header, "MBIN", 4);
	header[1] = (uint32_t) num_files;
	header[2] = 1;
	header[3] = 0x20+(num_files*4);
	header[5] = 0x20;
	for (int i=0;i<num_files;i++) p_table_offsets_base[i] += comp_start+padding;

	/* Write data to file. */
	fwrite(header, sizeof(uint32_t), 8, fcompressed);
	fwrite(p_table_sizes_base, sizeof(uint32_t), num_files, fcompressed);
	fwrite(p_table_offsets_base, sizeof(uint32_t), num_files, fcompressed);
	fwrite(zeros, sizeof(uint8_t), padding, fcompressed);
	fwrite(p_compressed, sizeof(uint8_t), comp_size, fcompressed);

	for (i=0;i<malloc_cntr;i++) free(malloc_tble[i]);

	FindClose(hFind);
    return num_files;
}
