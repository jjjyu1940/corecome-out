#ifndef ALPOS_STORAGE_H
#define ALPOS_STORAGE_H
#include <stdint.h>
#define ALPOS_PARTITION_MAX 4
struct alpos_partition { uint8_t type, bootable; uint32_t first_lba, sectors; };
void storage_init(void);
int storage_disk_present(void);
int storage_partition_count(void);
const struct alpos_partition *storage_partition_get(int index);
#endif
