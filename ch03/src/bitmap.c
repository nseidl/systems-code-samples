#include "bitmap.h"

// get the bit_pos'th bit, starting at map_start
int get_bit(void* map_start, int bit_pos) {
	char word = *((char *) map_start + bit_pos/8);
	unsigned int word_index = bit_pos % 8;
	return (word >> word_index) & 1;
}

// set the bit_pos'th bit to val, starting at map_start
void set_bit(void* map_start, unsigned int bit_pos, int val) {
	val = val & 1; // get first bit of val
	char *word = ((char *) map_start + bit_pos/8); // get the word
	unsigned int word_index = bit_pos % 8; // get the index of the bit in the word
	*word = (*word & ~(1 << word_index)) | (val << word_index);
}

