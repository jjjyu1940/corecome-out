#ifndef ALPOS_FAT32_H
#define ALPOS_FAT32_H
#include <stdint.h>
#define ALPOS_FAT32_NAME_MAX 13
#define ALPOS_FAT32_FILE_MAX 4096U
struct alpos_fat32_info {
    uint32_t partition_lba, total_sectors, sectors_per_fat, root_cluster;
    uint16_t bytes_per_sector, reserved_sectors;
    uint8_t sectors_per_cluster, fat_count;
};
struct alpos_fat32_entry { char name[ALPOS_FAT32_NAME_MAX]; uint32_t first_cluster, size; uint8_t attributes; };
int fat32_mount_partition(unsigned partition_index, struct alpos_fat32_info *info);
int fat32_root_entry(const struct alpos_fat32_info *info, unsigned index, struct alpos_fat32_entry *entry);
int fat32_read_file(const struct alpos_fat32_info *info, const struct alpos_fat32_entry *entry, uint8_t *out, uint32_t capacity);
#endif
