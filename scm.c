/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */

/* research the above Needed API and design accordingly */

#define VIRT_ADDR 0x600000000000

//struct scm {
//    int fd;
//    size_t size; // when truncate is called, size is 0
//    void *addr; // this value is VIRT_ADDR
//};

// function file_size
// input is pathname
// determine how big the file is
// return the size of the file
// mode for open is O_RDWR
// open(pathname, mode)
// returns the file descriptor or -1 on error
// for close, close(fd)
// fstat(fd, &st), st is a struct stat
// we need to check if the file is a regular file or not
// S_ISREG(st.st_mode) is a macro that returns true if the file is a regular file
// if it is not a regular file, return NULL
// use st.st_size to get the size of the file

//static off_t
//file_size(const char *pathname) {
//    struct stat st;
//    int fd;
//
//    if ((fd = open(pathname, O_RDWR)) == -1) {
//        return -1;
//    }
//    if (fstat(fd, &st) == -1) {
//        close(fd);
//        return -1;
//    }
//    if (!S_ISREG(st.st_mode)) {
//        close(fd);
//        return -1;
//    }
//    close(fd);
//
//    return st.st_size;
//}

// for mmap, we need to do mmap(VIRT_ADDR, length, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0)
// if the return value is MAP_FAILED, return NULL
// otherwise, return the pointer to the memory region
// length is the size of the file
// the returned address should match VIRT_ADDR

// print mapped region
// print unmapped region
// print storage class memory region
// do this before program starts
// MAPPED REGION is 0 - current break
// UNMAPPED REGION is current break - (VIRT_ADDR - 1)
// STORAGE CLASS MEMORY REGION is VIRT_ADDR - (VIRT_ADDR + length - 1)
// the heap will have some metadata and the base address for allocating memory will start after the metadata
// the metadata will start at VIRT_ADDR

// how scm_malloc works
// scm_malloc(scm, n)
// first time scm_malloc is called, scm->size is 0
// have a temp variable for size
// size = scm->size + n
// if size is greater than the length of the file, we need to throw an error
// if size is less than the length of the file, we need to increase the size of the file
// increase size by n
// return the address of the memory region + scm->size

// in scm_open
// we see the very first memory location to get the size
// but everytime we call scm_malloc, we need to update the size
// after that call msync so that it's not lost
// instead of a single size, put a T before and a CRC after so that it's encoded and we can check for corruption
// the base address is different from original addr because of the T and CRC
// the scm_mbase is used for the above reason

// if we use a bit value in each block of assigned memory, we can check if it's alloted memory or freed memory
// if it's alloted memory, the bit value is 1
// if it's freed memory, the bit value is 0

// in scm_malloc, we need to check if the memory is alloted or freed
// we need to allocate the memory in the first free block after the last alloted block
// if we have reached the end of the file, we need to see if there are any freed blocks
// if there are freed blocks (bit value is 0), we need to allocate the memory there

// in scm_free, we need to check if the memory is alloted or freed
// if it's alloted, we need to free it
// if it's freed, we need to throw an error

// in scm_strdup, we need to use scm_malloc to allocate the memory
// we need to use memcpy to copy the string into the allocated memory

// in scm_close, we need to call msync to save the changes
// we need to call munmap to unmap the memory region
// we need to call close to close the file descriptor

struct scm {
    int fd; // file descriptor of the scm file
    struct {
        size_t utilized; // number of bytes utilized thus far
        size_t capacity; // number of bytes available in total
    } size;
    void *addr; // address of the scm in the heap
};

struct scm *file_size(const char *pathname) {
    struct stat st;
    int fd;
    struct scm *scm;

    assert(pathname);

    if (!(scm = malloc(sizeof(struct scm)))) {
        return NULL;
    }
    memset(scm, 0, sizeof(struct scm));

    if ((fd = open(pathname, O_RDWR)) == -1) {
        free(scm);
        return NULL;
    }

    if (fstat(fd, &st) == -1) {
        free(scm);
        close(fd);
        return NULL;
    }

    if (!S_ISREG(st.st_mode)) {
        free(scm);
        close(fd);
        return NULL;
    }

    scm->fd = fd;
    scm->size.utilized = 0;
    scm->size.capacity = st.st_size;

    return scm;
}

struct scm *scm_open(const char *pathname, int truncate) {
    struct scm *scm = file_size(pathname);
    if (!scm) {
        return NULL;
    }

    if (sbrk(scm->size.capacity) == (void *) -1) {
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if ((scm->addr = mmap((void *) VIRT_ADDR, scm->size.capacity, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED,
                          scm->fd, 0)) == MAP_FAILED) {
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if (truncate) {
        if (ftruncate(scm->fd, scm->size.capacity) == -1) {
            close(scm->fd);
            free(scm);
            return NULL;
        }
        scm->size.utilized = 0;
    } else {
        // get the value of the utilized memory from the first sizeof(size_t) bytes
        scm->size.utilized = (size_t) *(size_t *) scm->addr;
        printf("scm->size.utilized: %lu\n", scm->size.utilized);
    }
    scm->addr = (char *) scm->addr + sizeof(size_t);
    printf("scm->addr after: %p\n", scm->addr);

    return scm;
}

void scm_close(struct scm *scm) {
    if (scm) {
        printf("scm->addr: %p\n", scm->addr);
        printf("scm->size.utilized: %lu\n", scm->size.utilized);
        msync((char *) VIRT_ADDR, scm->size.capacity, MS_SYNC);
        munmap((char *) VIRT_ADDR, scm->size.capacity);
        close(scm->fd);
        memset(scm, 0, sizeof(struct scm));
    }
    free(scm);
}

void *scm_malloc(struct scm *scm, size_t n) {
    // each block of memory has three things: size, bit value and data
    // size is the size of the block of memory
    // bit value is 1 if the block of memory is alloted and 0 if the block of memory is freed
    // data is the actual data
    // the size of the block of memory is sizeof(size_t) + sizeof(short) + n
    // the bit value is stored in the first sizeof(short) bytes
    // the size is stored in the next sizeof(size_t) bytes
    // the data is stored in the next n bytes
    // the base address of the block of memory is the address of the data
    // if we reach the end of the file, we need to check if there are any freed blocks
    // if there are freed blocks, we need to allocate the memory there
    // if there are no freed blocks, we need to throw errors

    // base address of the block of memory
    void *p = (char *) scm->addr + scm->size.utilized;
    // size of the block of memory
    size_t size = sizeof(short) + sizeof(size_t) + n;
    // if the size of the block of memory exceeds the capacity of the file, throw an error
    if (scm->size.utilized + size > scm->size.capacity) {
        // TODO: fix me
        return NULL;
    }

    // set the bit value to 1
    *(short *) p = 1;
    // set size to size
    *(size_t *) ((char *) p + sizeof(short)) = size;
    // increase utilized by size
    scm->size.utilized += size;
    // save utilized to the first sizeof(size_t) bytes
    *(size_t *) ((char *) scm->addr - sizeof(size_t)) = scm->size.utilized;
    // return the base address of the block of memory
    return (char *) p + sizeof(short) + sizeof(size_t);
}

char *scm_strdup(struct scm *scm, const char *s) {
    size_t n = strlen(s) + 1;
    char *p = scm_malloc(scm, n);
    if (!p) {
        return NULL;
    }
    memcpy(p, s, n);
    return p;
}

void scm_free(struct scm *scm, void *p) {
    size_t size = *(size_t *) ((char *) p - sizeof(size_t));
    *(short *) ((char *) p - sizeof(short) - sizeof(size_t)) = 0;
    scm->size.utilized -= size;
    *(size_t *) ((char *) scm->addr - sizeof(size_t)) = scm->size.utilized;
}

size_t scm_utilized(const struct scm *scm) {
    return scm->size.utilized;
}

size_t scm_capacity(const struct scm *scm) {
    return scm->size.capacity;
}

void *scm_mbase(struct scm *scm) {
    // the base address of the scm is VIRT_ADDR
    // the base address of the scm is different from the original address because of the T and CRC
    // the base address of the scm is the address of the metadata
    return (char *) scm->addr + sizeof(short) + sizeof(size_t);
}
