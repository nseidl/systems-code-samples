#ifndef BITMAP_H
#define BITMAP_H

int get_bit(void* map_start, int bit_pos);
void set_bit(void* map_start, unsigned int bit_pos, int val);

#endif