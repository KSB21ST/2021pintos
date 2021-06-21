#ifndef FILESYS_FAT_H
#define FILESYS_FAT_H

#include "devices/disk.h"
#include "filesys/file.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t cluster_t;  /* Index of a cluster within FAT. */

#define FAT_MAGIC 0xEB3C9000 /* MAGIC string to identify FAT disk */
#define EOChain 0x0FFFFFFF   /* End of cluster chain */

/* Sectors of FAT information. */
#define SECTORS_PER_CLUSTER 1 /* Number of sectors per cluster */
#define FAT_BOOT_SECTOR 0     /* FAT boot sector. */
#define ROOT_DIR_CLUSTER 1    /* Cluster for the root directory */

//start 20180109
// size_t CLUSTER_SIZE = DISK_SECTOR_SIZE*SECTORS_PER_CLUSTER;
#define CLUSTER_SIZE DISK_SECTOR_SIZE*SECTORS_PER_CLUSTER
//end 20180109

int mount_cnt[2];

void fat_init (void);
void fat_open (void);
void fat_close (void);
void fat_create (void);
void fat_close (void);

cluster_t fat_create_chain (
    cluster_t clst /* Cluster # to stretch, 0: Create a new chain */
);
void fat_remove_chain (
    cluster_t clst, /* Cluster # to be removed */
    cluster_t pclst /* Previous cluster of clst, 0: clst is the start of chain */
);
cluster_t fat_get (cluster_t clst);
void fat_put (cluster_t clst, cluster_t val);
disk_sector_t cluster_to_sector (cluster_t clst);

//start 20180109
cluster_t fat_get_end(cluster_t clst);
//end 20180109


// edit for mount
void fat_init_scratch (void);
void fat_open_scratch (void);
void fat_close_scratch (void);
void fat_create_scratch (void);
void fat_close_scratch (void);

cluster_t fat_create_chain_scratch (
    cluster_t clst /* Cluster # to stretch, 0: Create a new chain */
);
void fat_remove_chain_scratch (
    cluster_t clst, /* Cluster # to be removed */
    cluster_t pclst /* Previous cluster of clst, 0: clst is the start of chain */
);
cluster_t fat_get_scratch (cluster_t clst);
void fat_put_scratch (cluster_t clst, cluster_t val);
disk_sector_t cluster_to_sector_scratch (cluster_t clst);

cluster_t fat_get_end_scratch (cluster_t clst);


#endif /* filesys/fat.h */
