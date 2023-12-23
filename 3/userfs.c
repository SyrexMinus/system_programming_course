#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

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
	/** Double-linked list of file blocks. First block. */
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
	/** number of blocks occupied to control file size. */
	size_t blocks_num;
};

/** Double-linked list of all files. Last file. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
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
block_new(struct block *prev)
{
    struct block *b = (struct block *) malloc(sizeof(*b));
    b->memory = calloc(BLOCK_SIZE, sizeof(char));
    b->occupied = 0;
    if (prev != NULL)
        prev->next = b;
    b->prev = prev;
    b->next = NULL;
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
    if (file_list != NULL)
        file_list->next = f;
    f->prev = file_list;
    file_list = f;
    f->blocks_num = 0;
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
    if (prev != NULL)
        prev->next = next;
    if (next != NULL)
        next->prev = prev;
    else
        file_list = prev;
    free(f);
}

static struct file *
file_get(const char *name)
{
    struct file *file = file_list;
    while (file != NULL) {  // TODO replace with name-file map
        if (strcmp(file->name, name) == 0)
            break;
        file = file->prev;
    }
    return file;
}

static int
file_read(struct file *file, char *buf, size_t size)  // TODO accept pointer in memory
{
    struct block *cur_b = file->block_list;
    size_t size_left = size;
    while (cur_b != NULL && size_left > 0) {
        size_t to_read = min(size_left, (size_t) cur_b->occupied);
        memcpy(buf, cur_b->memory, to_read);
        size_left -= to_read;
        cur_b = cur_b->next;
    }
    return size - size_left;
}

static int
file_write(struct file *file, const char *buf, size_t size)  // TODO accept pointer in memory
{
    size_t size_left = size;
    while (size_left > 0) {
        if (file->last_block == NULL || file->last_block->occupied == BLOCK_SIZE) {
            if (BLOCK_SIZE * (file->blocks_num+1) > MAX_FILE_SIZE) {
                if (size_left == size) {
                    ufs_error_code = UFS_ERR_NO_MEM;
                    return -1;
                }
                break;
            }
            file->last_block = block_new(file->last_block);
            if (file->block_list == NULL)
                file->block_list = file->last_block;
        }
        struct block *cur_b = file->last_block;
        size_t to_write = min(BLOCK_SIZE - (size_t) cur_b->occupied, size_left);
        memcpy(cur_b->memory, buf, to_write);
        cur_b->occupied = max((size_t) cur_b->occupied, to_write);
        size_left -= to_write;
    }
    return size - size_left;
}

static int
filedesc_new(struct file *file)
{
    struct filedesc *fd_ = (struct filedesc *) malloc(sizeof(*fd_));
    fd_->file = file;
    ++file->refs;
    if (file_descriptor_count == file_descriptor_capacity) {
        file_descriptor_capacity = (file_descriptor_capacity + 1) * 2;
        file_descriptors = realloc(file_descriptors, sizeof(struct filedesc *) * file_descriptor_capacity);
    } else {
        assert(file_descriptor_count < file_descriptor_capacity);
    }
    file_descriptors[file_descriptor_count++] = fd_;
    return file_descriptor_count - 1;
}

static struct filedesc *
filedesc_get(int fd)
{
    if (fd < 0 || fd >= file_descriptor_count) {
        return NULL;
    }
    return file_descriptors[fd];
}

static void
filedesc_delete(struct filedesc * fd_)
{
    assert(fd_ != NULL);
    --(fd_->file->refs);
    --file_descriptor_count;
    free(fd_);
}

int
ufs_open(const char *filename, int flags)
{
    struct file *file = file_get(filename);
    if (file)
        return filedesc_new(file);
    if ((flags & UFS_CREATE) == 0) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    file = file_new(filename);
    return filedesc_new(file);
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
    struct filedesc *fd_ = filedesc_get(fd);
    if (fd_ == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    return file_read(fd_->file, buf, size);
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
    struct filedesc *fd_ = filedesc_get(fd);
    if (fd_ == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    return file_write(fd_->file, buf, size);
}

/**
 * Close a file.
 * @param fd File descriptor from ufs_open().
 * @retval 0 Success.
 * @retval -1 Error occurred. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - invalid file descriptor.
 */
int
ufs_close(int fd)
{
    struct filedesc *fd_ = filedesc_get(fd);
    if (fd_ == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    // todo should double close close the same filedescriptor? Now it might close two different
    filedesc_delete(fd_);
	return 0;
}

int
ufs_delete(const char *filename)
{
    struct file *file = file_get(filename);
    if (file) {
        file_delete(file);
        return 0;
    }
    ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

void
ufs_destroy(void)
{
    for (int i = 0; i < file_descriptor_count; ++i) {
        struct filedesc *fd_ = file_descriptors[i];
        filedesc_delete(fd_);
    }
    while (file_list != NULL) {
        struct file *f = file_list;
        file_list = f->prev;
        file_delete(f);
    }
}

// TODO according to tests
// todo recheck requirements
