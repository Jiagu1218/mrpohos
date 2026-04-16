#include "./header/fileLib.h"
#include "./header/utils.h"

#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>

static char g_sandboxPath[1024] = {0};

void fileLib_setSandboxPath(const char *path) {
    if (path) {
        strncpy(g_sandboxPath, path, sizeof(g_sandboxPath) - 1);
        g_sandboxPath[sizeof(g_sandboxPath) - 1] = '\0';
        size_t len = strlen(g_sandboxPath);
        while (len > 1 && g_sandboxPath[len - 1] == '/') {
            g_sandboxPath[--len] = '\0';
        }
    }
}

static const char *mapPath(const char *filename) {
    static char mapped[2048];
    if (g_sandboxPath[0] == '\0') {
        return filename;
    }
    if (filename[0] == '/') {
        snprintf(mapped, sizeof(mapped), "%s%s", g_sandboxPath, filename);
    } else {
        snprintf(mapped, sizeof(mapped), "%s/%s", g_sandboxPath, filename);
    }
    return mapped;
}

// mrc_open需要返回0表示失败
static struct rb_root filef_map = RB_ROOT;
static uint32_t filef_count = 0;

// mrc_findStart需要返回-1表示失败
static struct rb_root dirf_map = RB_ROOT;
static uint32_t dirf_count = 0;

int32_t my_open(const char *filename, uint32_t mode) {
    int f;
    int new_mode = 0;

    if (mode & MR_FILE_RDONLY) new_mode = O_RDONLY;
    if (mode & MR_FILE_WRONLY) new_mode = O_WRONLY;
    if (mode & MR_FILE_RDWR) new_mode = O_RDWR;
    if (mode & MR_FILE_CREATE) new_mode |= O_CREAT;

#ifdef _WIN32
    new_mode |= O_RAW;
#endif

    f = open(mapPath((char *)filename), new_mode, S_IRWXU | S_IRWXG | S_IRWXO);
    if (f == -1) {
        return 0;
    }

    filef_count++;
    uIntMap *obj = malloc(sizeof(uIntMap));
    obj->key = filef_count;
    obj->data = (void *)f;
    uIntMap_insert(&filef_map, obj);
    return filef_count;
}

int32_t my_close(int32_t f) {
    uIntMap *obj = uIntMap_delete(&filef_map, f);
    if (obj == NULL) {
        return MR_FAILED;
    }
    if (f == filef_count) {
        filef_count--;
    }
    int fh = (int)obj->data;
    free(obj);
    if (close(fh) != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_seek(int32_t f, int32_t pos, int method) {
    uIntMap *obj = uIntMap_search(&filef_map, f);
    if (obj == NULL) {
        return MR_FAILED;
    }
    off_t ret = lseek((int)obj->data, (off_t)pos, method);
    if (ret == -1) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_read(int32_t f, void *p, uint32_t l) {
    uIntMap *obj = uIntMap_search(&filef_map, f);
    if (obj == NULL) {
        return MR_FAILED;
    }
    int32_t readnum = read((int)obj->data, p, (size_t)l);
    if (readnum == -1) {
        return MR_FAILED;
    }
    return readnum;
}

int32_t my_write(int32_t f, void *p, uint32_t l) {
    uIntMap *obj = uIntMap_search(&filef_map, f);
    if (obj == NULL) {
        return MR_FAILED;
    }
    int32_t writenum = write((int)obj->data, p, (size_t)l);
    if (writenum == -1) {
        return MR_FAILED;
    }
    return writenum;
}

int32_t my_rename(const char *oldname, const char *newname) {
    int ret = rename(mapPath(oldname), mapPath(newname));
    if (ret != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_remove(const char *filename) {
    int ret = remove(mapPath(filename));
    if (ret != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_getLen(const char *filename) {
    struct stat s1;
    int ret = stat(mapPath(filename), &s1);
    if (ret != 0)
        return -1;
    return s1.st_size;
}

int32_t my_mkDir(const char *name) {
    int ret;
    if (access(mapPath(name), F_OK) == 0) {
        goto ok;
    }
#ifndef _WIN32
    ret = mkdir(mapPath(name), S_IRWXU | S_IRWXG | S_IRWXO);
#else
        ret = mkdir(mapPath(name));
#endif
    if (ret != 0) {
        return MR_FAILED;
    }
ok:
    return MR_SUCCESS;
}

int32_t my_rmDir(const char *name) {
    int ret = rmdir(mapPath(name));
    if (ret != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_info(const char *filename) {
    struct stat s1;
    int ret = stat(mapPath(filename), &s1);

    if (ret != 0) {
        return MR_IS_INVALID;
    }
    if (s1.st_mode & S_IFDIR) {
        return MR_IS_DIR;
    } else if (s1.st_mode & S_IFREG) {
        return MR_IS_FILE;
    }
    return MR_IS_INVALID;
}

int32_t my_opendir(const char *name) {
    DIR *pDir = opendir(mapPath(name));
    if (pDir != NULL) {
        dirf_count++;
        uIntMap *obj = malloc(sizeof(uIntMap));
        obj->key = dirf_count;
        obj->data = (void *)pDir;
        uIntMap_insert(&dirf_map, obj);
        return dirf_count;
    }
    return MR_FAILED;
}

char *my_readdir(int32_t f) {
    uIntMap *obj = uIntMap_search(&dirf_map, f);
    if (obj == NULL) {
        return NULL;
    }
    struct dirent *pDt = readdir((DIR *)obj->data); // 手册说返回的内存可能是静态分配的，不要尝试free()
    if (pDt != NULL) {
        return pDt->d_name;
    }
    return NULL;
}

int32_t my_closedir(int32_t f) {
    uIntMap *obj = uIntMap_delete(&dirf_map, f);
    if (obj == NULL) {
        return MR_FAILED;
    }
    if (f == dirf_count) {
        dirf_count--;
    }
    DIR *pDir = (DIR *)obj->data;
    free(obj);
    if (closedir(pDir) != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

void writeFile(const char *filename, void *data, uint32 length) {
    int fh = my_open(filename, MR_FILE_CREATE | MR_FILE_RDWR);
    int32_t wLen = 0;
    char *ptr = (char *)data;
    do {
        ptr += wLen;
        wLen = my_write(fh, ptr, length < 1000 ? length : 1000);
        if (wLen == MR_FAILED) {
            break;
        }
        length -= wLen;
    } while (length > 0);
    my_close(fh);
}

char *readFile(const char *filename) {
    int32_t len = my_getLen(filename);
    char *p = malloc(len);
    if (p == NULL) {
        return NULL;
    }
    int32_t fh = my_open(filename, MR_FILE_RDONLY);
    if (fh) {
        int32_t rLen = 0;
        char *ptr = p;
        do {
            ptr += rLen;
            rLen = my_read(fh, ptr, len);
            if (rLen == MR_FAILED) {
                free(p);
                return NULL;
            }
            len -= rLen;
        } while (len > 0);
        my_close(fh);
        return p;
    }
    return NULL;
}
