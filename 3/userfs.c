#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
    int pos;  // todo refactor remove pos, return in filedesc new and delete use of pos
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

static struct block *
block_new(struct block *next, struct block *prev)
{
    struct block *b = (struct block *) malloc(sizeof(*b));
    b->memory = calloc(BLOCK_SIZE, sizeof(char));
    b->occupied = 0;
    b->next = next;
    b->prev = prev;
    return b;
}

static void
block_delete(struct block *b)
{
    free(b->memory);
    struct block *prev = b->prev, *next = b->next;
    if (prev != NULL)
        prev->next = next;
    if (next != NULL)
        next->prev = prev;
    free(b);
}

static struct file *
file_new(const char *name)
{
    struct file *f = (struct file *) malloc(sizeof(*f));
    f->block_list = NULL;
    f->last_block = NULL;
    f->refs = 0;
    f->name = strdup(name);
    f->next = NULL;
    f->prev = NULL;
    if (file_list != NULL) {
        file_list->next = f;
        f->prev = file_list;
    }
    file_list = f;
    return f;
}

static void
file_delete(struct file *f)
{
    while (f->block_list != NULL) {
        struct block *b = f->block_list;
        f->block_list = b->next;
        block_delete(b);
    }
    free(f->name);
    struct file *prev = f->prev, *next = f->next;
    if (prev != NULL) {
        prev->next = next;
        if (file_list == f)
            file_list = prev;
    } else if (file_list == f)
        file_list = NULL;
    if (next != NULL)
        next->prev = prev;
    free(f);
}

static struct filedesc *
filedesc_new(struct file *file)
{
    struct filedesc *_fd = (struct filedesc *) malloc(sizeof(*_fd));
    _fd->file = file;
    ++file->refs;
    if (file_descriptor_count == file_descriptor_capacity) {
        file_descriptor_capacity = (file_descriptor_capacity + 1) * 2;
        file_descriptors = realloc(file_descriptors, sizeof(*file_descriptors) * file_descriptor_capacity);
    } else {
        assert(file_descriptor_count < file_descriptor_capacity);
    }
    _fd->pos = file_descriptor_count;
    file_descriptors[file_descriptor_count++] = _fd;
    return _fd;
}

static void
filedesc_delete(struct filedesc *_fd)
{
    --_fd->file->refs;
    file_descriptors[_fd->pos] = file_descriptors[file_descriptor_count--];
    free(_fd);
}

int
ufs_open(const char *filename, int flags)
{
    (void)block_new;
    (void)file_delete;
    (void)filedesc_delete; // todo remove

    // todo UFS_ERR_NO_MEM
    struct file *file = file_list;
    while (file != NULL) {
        if (strcmp(file->name, filename) == 0) {
            return filedesc_new(file)->pos;
        }
        file = file->prev;
    }
    if ((flags & UFS_CREATE) == 0) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    file = file_new(filename);
    return filedesc_new(file)->pos;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;  // todo remove

    // TODO according to tests
	return -1;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)fd;
	(void)buf;
	(void)size;
	ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;  // todo remove

	// todo support reading after file deletion: "content will live until the last descriptor is closed"
    // TODO according to tests
	return -1;
}

int
ufs_close(int fd)
{
    if (fd < 0 || fd >= file_descriptor_count) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    // todo should double close close the same filedescriptor? Now it might close two different
    filedesc_delete(file_descriptors[fd]);
	return 0;
}

int
ufs_delete(const char *filename)
{
    struct file *file = file_list;
    while (file != NULL) {
        if (strcmp(file->name, filename) == 0) {
            file_delete(file);
            return 0;
        }
        file = file->prev;
    }
    ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

void
ufs_destroy(void)
{

    // TODO according to tests
}

// todo recheck requirements
