#include "filesys/fat.h"
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include <string.h>

/* Should be less than DISK_SECTOR_SIZE */
struct fat_boot {
	unsigned int magic;
	unsigned int sectors_per_cluster; /* Fixed to 1 */
	unsigned int total_sectors;
	unsigned int fat_start;
	unsigned int fat_sectors; /* Size of FAT in sectors. */
	unsigned int root_dir_cluster;
};

/* FAT FS */
struct fat_fs {
	struct fat_boot bs;
	unsigned int *fat;
	unsigned int fat_length;
	disk_sector_t data_start;
	cluster_t last_clst;
	struct lock write_lock;
};

static struct fat_fs *fat_fs;

void fat_boot_create (void);
void fat_fs_init (void);

void
fat_init (void) {
	fat_fs = calloc (1, sizeof (struct fat_fs));
	if (fat_fs == NULL)
		PANIC ("FAT init failed");

	//##########for proj4################
	// Read boot sector from the disk
	unsigned int *bounce = malloc (DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT init failed");
	disk_read (filesys_disk, FAT_BOOT_SECTOR, bounce);
	memcpy (&fat_fs->bs, bounce, sizeof (fat_fs->bs));
	free (bounce);

	// Extract FAT info
	if (fat_fs->bs.magic != FAT_MAGIC)
		fat_boot_create ();
	fat_fs_init ();
}

void
fat_open (void) {
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT load failed");

	// Load FAT directly from the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_read = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_read (filesys_disk, fat_fs->bs.fat_start + i,
			           buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		} else {
			uint8_t *bounce = malloc (DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT load failed");
			disk_read (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			memcpy (buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free (bounce);
		}
	}
}

void
fat_close (void) {
	// Write FAT boot sector
	// uint8_t *bounce;
	//####################for proj4#################
	uint8_t *bounce = calloc (1, DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT close failed");
	memcpy (bounce, &fat_fs->bs, sizeof (fat_fs->bs));
	disk_write (filesys_disk, FAT_BOOT_SECTOR, bounce);
	free (bounce);

	// Write FAT directly to the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_wrote = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_write (filesys_disk, fat_fs->bs.fat_start + i,
			            buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		} else {
			bounce = calloc (1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT close failed");
			memcpy (bounce, buffer + bytes_wrote, bytes_left);
			disk_write (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			bytes_wrote += bytes_left;
			free (bounce);
		}
	}
}

void
fat_create (void) {
	// Create FAT boot
	fat_boot_create ();
	fat_fs_init ();

	// Create FAT table
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT creation failed");

	// Set up ROOT_DIR_CLST
	fat_put (ROOT_DIR_CLUSTER, EOChain);

	// Fill up ROOT_DIR_CLUSTER region with 0
	uint8_t *buf = calloc (1, DISK_SECTOR_SIZE);
	if (buf == NULL)
		PANIC ("FAT create failed due to OOM");
	disk_write (filesys_disk, cluster_to_sector (ROOT_DIR_CLUSTER), buf);
	// page_cache_write(cluster_to_sector (ROOT_DIR_CLUSTER), buf);
	free (buf);
}

void
fat_boot_create (void) {
	unsigned int fat_sectors =
	    (disk_size (filesys_disk) - 1)
	    / (DISK_SECTOR_SIZE / sizeof (cluster_t) * SECTORS_PER_CLUSTER + 1) + 1; //20180109 Q: what is fat_sectors?
	// printf("fat sectors: %d \n", fat_sectors); //fat sectors: 157 
	fat_fs->bs = (struct fat_boot){
	    .magic = FAT_MAGIC,
	    .sectors_per_cluster = SECTORS_PER_CLUSTER,
	    .total_sectors = disk_size (filesys_disk),
	    .fat_start = 1,
	    .fat_sectors = fat_sectors,
	    .root_dir_cluster = ROOT_DIR_CLUSTER,
	};
	// printf("total sectors: %d \n", fat_fs->bs.total_sectors); //total sectors: 20160 
}

void
fat_fs_init (void) {
	/* TODO: Your code goes here. */
    fat_fs->fat_length = (&fat_fs->bs)->total_sectors / (&fat_fs->bs)->sectors_per_cluster - fat_fs->bs.fat_sectors;//20180109 every sectors in disk are changed into clusters
    fat_fs->data_start = (&fat_fs->bs)->fat_start;//20180109 Q: why +1 --> so that 0th index is free, and not confused between NULL. + because root directory goes into 1
	// fat_fs->fat_length = fat_fs->bs.fat_sectors * DISK_SECTOR_SIZE / sizeof(cluster_t) / SECTORS_PER_CLUSTER;
	// fat_fs->data_start = fat_fs->bs.fat_start + fat_fs->bs.fat_sectors;
}

/*----------------------------------------------------------------------------*/
/* FAT handling                                                               */
/*----------------------------------------------------------------------------*/

/* Add a cluster to the chain.
 * If CLST is 0, start a new chain.
 * Returns 0 if fails to allocate a new cluster. */
cluster_t
fat_create_chain (cluster_t clst) {
	static char zeros[DISK_SECTOR_SIZE];
	/* TODO: Your code goes here. */
	// if (clst == 0) return 0;
	cluster_t idx = 0;
	// for (cluster_t i=fat_fs->data_start+1; i< fat_fs->fat_length; i++) //20180109 unint overflow carefull!
	cluster_t i;
	for (i=1; i< fat_fs->fat_length; i++)
	{
		if(fat_fs->fat[i] == 0){
			idx = i;
			break;
		}
	}
	// printf("i: %d in fat_create_chain \n", i);
	if(idx == 0){
		// printf("fat create chain here? \n");
		return 0;
	}
	if(cluster_to_sector(idx) >= (filesys_disk)->capacity){
		// printf("fat create chain here? %d %d\n", (filesys_disk)->capacity, idx, cluster_to_sector(idx));
		return 0;
	}
	if(clst == 0){
		fat_fs->fat[idx] = EOChain;
		return idx;
	}
	ASSERT(fat_fs->fat[clst] == EOChain); //clst 가 chain 의 마지막 요소가 아니면 error를 내준다
	fat_fs->fat[clst] = idx;
	fat_fs->fat[idx] = EOChain;
	// printf("clst: %d, idx: %d\n", clst, idx);
	return idx;
}

/* Remove the chain of clusters starting from CLST.
 * If PCLST is 0, assume CLST as the start of the chain. */
/*
pclst should be the direct previous cluster in the chain. 
This means, after the execution of this function,
pclst should be the last element of the updated chain. 
If clst is the first element in the chain, pclst should be 0
*/
void
fat_remove_chain (cluster_t clst, cluster_t pclst) {
	/* TODO: Your code goes here. */
	if (clst == 0)
	{
		fat_fs->fat[pclst] = 0;
		return;
	}
	while(fat_fs->fat[clst] != EOChain)
	{
		cluster_t temp = fat_fs->fat[clst];
		fat_fs->fat[clst] = 0;
		clst = temp;
	}
	fat_fs->fat[clst] = 0;
	if(pclst == 0)
		fat_fs->fat[pclst] = 0;
	else
		fat_fs->fat[pclst] = EOChain;
}

// /* Update a value in the FAT table. */
void
fat_put (cluster_t clst, cluster_t val) {
	/* TODO: Your code goes here. */
	// if(val == EOChain){
	// 	fat_fs->fat[clst] = val;
	// 	return;
	// }
	// cluster_t next = fat_fs->fat[clst];
	// fat_fs->fat[clst] = val;
	// fat_fs->fat[val] = next;

	fat_fs->fat[clst] = val;
}

/* Fetch a value in the FAT table. */
cluster_t
fat_get (cluster_t clst) {
	/* TODO: Your code goes here. */
	return fat_fs->fat[clst];
}

/* Covert a cluster # to a sector number. */
disk_sector_t
cluster_to_sector (cluster_t clst) {
	/* TODO: Your code goes here. */
	ASSERT(clst >= 0);
	// return clst;
	// if (clst == 1)
	// 	return clst;
	// return (disk_sector_t) clst + fat_fs->data_start;
	return (disk_sector_t) clst + fat_fs->bs.fat_sectors;
}

/*20180109 implemented - returns the end of the linked list*/
cluster_t
fat_get_end(cluster_t clst){
	if(clst == 0){
		return 0;
	}
	cluster_t next = clst;
	cluster_t i = 0;
	while(next != EOChain){
		i = next;
		next = fat_fs->fat[i];
	}
	return i;
}