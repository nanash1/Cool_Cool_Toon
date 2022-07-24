/*
 * buffer.c
 *
 *  Created on: Mar 30, 2021
 *      Author: nanashi
 */


#include <stdint.h>
#include "buffer.h"

static uint8_t ringb_data[RB_MAX+1];

/**
 * @brief	Initialize ring buffer.
 * @return	Ring buffer structure.
 */
ringb_t ringb_init(void)
{
	ringb_t rtn;
	rtn.p_data = ringb_data;
	rtn.head = 4078;
	rtn.tail = 0;
	for(int i=0;i<4078;i++)							// pre-load buffer with 4078 zeros
		ringb_data[i] = 0;
	return rtn;
}

/**
 * @brief	Pop byte from buffer.
 * @param	buff		Pointer to buffer.
 * @param	is_empy		Set to 1 if buffer is empty, else 0.
 * @return	Buffer byte.
 */
uint8_t ringb_pop(ringb_t* buff, int* is_empty)
{
	uint8_t rtn = buff->p_data[buff->tail];
	buff->tail = (buff->tail + 1) & RB_MAX;
	*is_empty = buff->tail == buff->head;
	return rtn;
}

/**
 * @brief	Get byte from buffer.
 * @param	buff		Pointer to buffer.
 * @param	idx			Buffer index (wrapped around if too big).
 * @return	Buffer byte.
 */
uint8_t ringb_get(ringb_t* buff, int idx)
{
	return buff->p_data[idx & RB_MAX];
}

/**
 * @brief	Insert byte into buffer.
 * @param	buff		Pointer to buffer.
 * @param	byte		New byte.
 * @return	Nothing.
 */
void ringb_insert(ringb_t* buff, uint8_t byte)
{
	buff->p_data[buff->head] = byte;
	buff->head = (buff->head + 1) & RB_MAX;
	if (buff->head == buff->tail)
		buff->tail = (buff->tail + 1) & RB_MAX;
}

/**
 * @brief	Initialize window buffer.
 * @param	p_data		Pointer to data.
 * @param	size		Size of data.
 * @return	Window buffer structure.
 */
winb_t winb_init(uint8_t* p_data, int size)
{
	winb_t rtn;
	rtn.p_data = p_data;
	rtn.size = size;
	rtn.tail = 0;
	rtn.head = RB_MAX;
	return rtn;
}

/**
 * @brief	Pop byte from buffer.
 * @param	buff		Pointer to buffer.
 * @param	is_empy		Set to 1 if buffer is empty, else 0.
 * @return	Buffer byte.
 */
uint8_t winb_pop(winb_t* buff, int* is_empty)
{
	uint8_t rtn = buff->p_data[buff->tail];
	buff->tail++;
	*is_empty = buff->tail == buff->head;
	return rtn;
}

/**
 * @brief	Advance buffer by one byte and return byte that
 * 			was shifted out.
 * @param	buff		Pointer to buffer.
 * @param	is_empy		Set to 1 if buffer is empty, else 0.
 * @return	Buffer byte.
 */
uint8_t winb_advance(winb_t* buff, int* is_empty)
{
	uint8_t rtn = buff->p_data[buff->tail];
	buff->size--;
	buff->p_data++;
	*is_empty = buff->size == 0;
	if (buff->head > buff->size)
		buff->head = buff->size;
	return rtn;
}
