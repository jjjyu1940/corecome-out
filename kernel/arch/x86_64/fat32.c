#include "fat32.h"
#include "storage.h"

#define FAT32_ATTR_VOLUME 0x08U
#define FAT32_ATTR_DIR 0x10U
#define FAT32_ATTR_LFN 0x0FU
#define FAT32_EOC 0x0FFFFFF8U
#define FAT32_BAD 0x0FFFFFF7U
#define FAT32_MAX_CHAIN 65536U

static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int power_of_two(uint8_t x) { return x && !(x & (uint8_t)(x - 1)); }

int fat32_mount_partition(unsigned index, struct alpos_fat32_info *out) {
    const struct alpos_partition *part = storage_partition_get((int)index);
    uint8_t b[512]; uint32_t total, fat_size, root, data_start, clusters;
    if (!part || !out || (part->type != 0x0B && part->type != 0x0C)) return -1;
    if (part->first_lba >= 0x10000000U || part->sectors > 0x10000000U - part->first_lba) return -2;
    if (!storage_read_sector(part->first_lba, b)) return -2;
    if (le16(b + 510) != 0xAA55 || le16(b + 11) != 512) return -3;
    if (!power_of_two(b[13]) || b[13] > 128 || b[16] == 0 || b[16] > 2) return -4;
    if (le16(b + 17) != 0 || le16(b + 19) != 0 || le16(b + 22) != 0) return -5;
    total = le16(b + 19); if (!total) total = le32(b + 32);
    fat_size = le32(b + 36); root = le32(b + 44);
    if (!total || !fat_size || root < 2 || total > part->sectors) return -6;
    data_start = (uint32_t)b[16] * fat_size + le16(b + 14);
    if (data_start >= total) return -7;
    clusters = (total - data_start) / b[13];
    if (clusters < 65525U || clusters >= 0x0FFFFFF5U || root >= clusters + 2U) return -8;
    out->partition_lba = part->first_lba; out->total_sectors = total;
    out->sectors_per_fat = fat_size; out->root_cluster = root;
    out->bytes_per_sector = 512; out->reserved_sectors = le16(b + 14);
    out->sectors_per_cluster = b[13]; out->fat_count = b[16];
    return 0;
}

static int next_cluster(const struct alpos_fat32_info *info, uint32_t cluster, uint32_t *next) {
    uint8_t sector[512]; uint32_t fat_sector, offset;
    if (!info || !next || cluster < 2U) return -1;
    fat_sector = (cluster * 4U) / 512U; offset = (cluster * 4U) % 512U;
    if (fat_sector >= info->sectors_per_fat) return -1;
    if (!storage_read_sector(info->partition_lba + info->reserved_sectors + fat_sector, sector)) return -1;
    *next = le32(sector + offset) & 0x0FFFFFFFU;
    if (*next == FAT32_BAD || *next == 0 || *next == 1) return -1;
    return 0;
}

int fat32_root_entry(const struct alpos_fat32_info *info, unsigned wanted, struct alpos_fat32_entry *out) {
    uint8_t sector[512]; uint32_t fat_start, data_start, cluster, visited = 0;
    uint32_t data_sectors, cluster_limit; unsigned found = 0, s, e;
    uint64_t volume_end;
    if (!info || !out || !info->sectors_per_cluster || !info->sectors_per_fat) return -1;
    fat_start = info->partition_lba + info->reserved_sectors;
    data_start = fat_start + (uint32_t)info->fat_count * info->sectors_per_fat;
    data_sectors = info->total_sectors - info->reserved_sectors - (uint32_t)info->fat_count * info->sectors_per_fat;
    cluster_limit = data_sectors / info->sectors_per_cluster + 2U;
    volume_end = (uint64_t)info->partition_lba + info->total_sectors;
    cluster = info->root_cluster;
    while (cluster >= 2U && cluster < cluster_limit && visited++ < FAT32_MAX_CHAIN) {
        uint32_t first_sector = data_start + (cluster - 2U) * info->sectors_per_cluster;
        if ((uint64_t)first_sector + info->sectors_per_cluster > volume_end) return -4;
        for (s = 0; s < info->sectors_per_cluster; ++s) {
            if (!storage_read_sector(first_sector + s, sector)) return -2;
            for (e = 0; e < 16; ++e) {
                const uint8_t *d = sector + e * 32U; unsigned i, n = 0;
                char name[ALPOS_FAT32_NAME_MAX];
                if (d[0] == 0x00) return -1;
                if (d[0] == 0xE5 || d[11] == FAT32_ATTR_LFN || (d[11] & FAT32_ATTR_VOLUME)) continue;
                for (i = 0; i < 8 && d[i] != ' '; ++i) name[n++] = (char)d[i];
                if (d[8] != ' ') { name[n++] = '.'; for (i = 8; i < 11 && d[i] != ' '; ++i) name[n++] = (char)d[i]; }
                name[n] = 0;
                if (found++ != wanted) continue;
                for (i = 0; i <= n; ++i) out->name[i] = name[i];
                out->first_cluster = ((uint32_t)le16(d + 20) << 16) | le16(d + 26);
                out->size = le32(d + 28); out->attributes = d[11];
                return 0;
            }
        }
        if (next_cluster(info, cluster, &cluster) < 0) return -2;
        if (cluster >= FAT32_EOC) return -1;
        if (cluster >= cluster_limit) return -4;
    }
    return -1;
}

int fat32_read_file(const struct alpos_fat32_info *info, const struct alpos_fat32_entry *entry, uint8_t *out, uint32_t capacity) {
    uint8_t sector[512]; uint32_t cluster, copied = 0, visited = 0;
    uint32_t data_start, data_sectors, cluster_limit;
    uint64_t volume_end;
    if (!info || !entry || !out || (entry->attributes & FAT32_ATTR_DIR) || entry->size > capacity || entry->size > ALPOS_FAT32_FILE_MAX) return -1;
    if (entry->size == 0) return 0;
    cluster = entry->first_cluster;
    data_start = info->partition_lba + info->reserved_sectors + (uint32_t)info->fat_count * info->sectors_per_fat;
    data_sectors = info->total_sectors - info->reserved_sectors - (uint32_t)info->fat_count * info->sectors_per_fat;
    cluster_limit = data_sectors / info->sectors_per_cluster + 2U;
    volume_end = (uint64_t)info->partition_lba + info->total_sectors;
    while (copied < entry->size && cluster >= 2U && cluster < cluster_limit && visited++ < FAT32_MAX_CHAIN) {
        uint32_t first_sector = data_start + (cluster - 2U) * info->sectors_per_cluster; unsigned s;
        if ((uint64_t)first_sector + info->sectors_per_cluster > volume_end) return -2;
        for (s = 0; s < info->sectors_per_cluster && copied < entry->size; ++s) {
            uint32_t amount = entry->size - copied; unsigned i;
            if (amount > 512U) amount = 512U;
            if (!storage_read_sector(first_sector + s, sector)) return -3;
            for (i = 0; i < amount; ++i) out[copied + i] = sector[i];
            copied += amount;
        }
        if (copied < entry->size) {
            if (next_cluster(info, cluster, &cluster) < 0 || cluster >= FAT32_EOC || cluster >= cluster_limit) return -4;
        }
    }
    return copied == entry->size ? (int)copied : -5;
}
