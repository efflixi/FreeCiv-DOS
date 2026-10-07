#ifndef FC_TEST_MOVEDATA_H
#define FC_TEST_MOVEDATA_H

#include <stddef.h>

void dosmemget(unsigned long address, size_t size, void *data);
void dosmemput(const void *data, size_t size, unsigned long address);
void movedata(unsigned int source_selector, unsigned int source_offset,
              unsigned int destination_selector,
              unsigned int destination_offset, size_t size);

#endif
