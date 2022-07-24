/*
 * buffer.h
 *
 *  Created on: Mar 30, 2021
 *      Author: nanashi
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#define RB_MAX		4095

typedef struct {
	uint8_t* p_data;	/* Pointer to data. */
	int head;			/* Head index. */
	int tail;			/* Tail index. */
} ringb_t;

/**
 * @brief	Initialize ring buffer.
 * @return	Ring buffer structure.
 */
ringb_t ringb_init(void);

/**
 * @brief	Pop byte from buffer.
 * @param	buff		Pointer to buffer.
 * @param	is_empy		Set to 1 if buffer is empty, else 0.
 * @return	Buffer byte.
 */
uint8_t ringb_pop(ringb_t* buff, int* is_empty);

/**
 * @brief	Get byte from buffer.
 * @param	buff		Pointer to buffer.
 * @param	idx			Buffer index (wrapped around if too big).
 * @return	Buffer byte.
 */
uint8_t ringb_get(ringb_t* buff, int idx);

/**
 * @brief	Insert byte into buffer.
 * @param	buff		Pointer to buffer.
 * @param	byte		New byte.
 * @return	Nothing.
 */
void ringb_insert(ringb_t* buff, uint8_t byte);

typedef struct {
	uint8_t* p_data;	/* Pointer to data. */
	int size;			/* Data size */
	int tail;			/* Window tail index. */
	int head;			/* Window head index. */
} winb_t;

/**
 * @brief	Initialize window buffer.
 * @param	p_data		Pointer to data.
 * @param	size		Size of data.
 * @return	Window buffer structure.
 */
winb_t winb_init(uint8_t* p_data, int size);

/**
 * @brief	Pop byte from buffer.
 * @param	buff		Pointer to buffer.
 * @param	is_empy		Set to 1 if buffer is empty, else 0.
 * @return	Buffer byte.
 */
uint8_t winb_pop(winb_t* buff, int* is_empty);

/**
 * @brief	Advance buffer by one byte and return byte that
 * 			was shifted out.
 * @param	buff		Pointer to buffer.
 * @param	is_empy		Set to 1 if buffer is empty, else 0.
 * @return	Buffer byte.
 */
uint8_t winb_advance(winb_t* buff, int* is_empty);

#endif /* BUFFER_H_ */
