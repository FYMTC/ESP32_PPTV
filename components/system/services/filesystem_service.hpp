#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILENAME_LEN 256
#define MAX_PATH_LEN 512

// 文件类型枚举
typedef enum {
    FILE_TYPE_DIRECTORY = 0,
    FILE_TYPE_REGULAR,
    FILE_TYPE_UNKNOWN
} file_type_t;

// 文件信息结构体
typedef struct {
    char name[MAX_FILENAME_LEN];
    char full_path[MAX_PATH_LEN];
    file_type_t type;
    size_t size;
    time_t modified_time;
    bool is_hidden;
} file_info_t;

// 目录遍历结构体
typedef struct {
    DIR *dir_handle;
    char current_path[MAX_PATH_LEN];
    int file_count;
    int dir_count;
} dir_iterator_t;

// 文件排序类型
typedef enum {
    SORT_BY_NAME_ASC = 0,
    SORT_BY_NAME_DESC,
    SORT_BY_SIZE_ASC,
    SORT_BY_SIZE_DESC,
    SORT_BY_TIME_ASC,
    SORT_BY_TIME_DESC,
    SORT_BY_TYPE
} sort_type_t;

// 文件系统API函数
esp_err_t fs_init(void);
bool filesystem_service_is_available(void);
bool fs_is_path_exists(const char *path);
bool fs_is_directory(const char *path);
bool fs_is_file(const char *path);

// 目录操作
esp_err_t fs_create_directory(const char *path);
esp_err_t fs_remove_directory(const char *path);
esp_err_t fs_list_directory(const char *path, file_info_t **files, int *count, sort_type_t sort);
void fs_free_file_list(file_info_t *files, int count);

// 目录遍历器
esp_err_t fs_open_directory(const char *path, dir_iterator_t *iterator);
esp_err_t fs_read_next_file(dir_iterator_t *iterator, file_info_t *file_info);
void fs_close_directory(dir_iterator_t *iterator);

// 文件操作
esp_err_t fs_get_file_info(const char *path, file_info_t *info);
esp_err_t fs_copy_file(const char *src, const char *dst);
esp_err_t fs_move_file(const char *src, const char *dst);
esp_err_t fs_delete_file(const char *path);
esp_err_t fs_rename_file(const char *old_name, const char *new_name);

// 路径操作
esp_err_t fs_get_parent_path(const char *path, char *parent_path, size_t parent_path_size);
esp_err_t fs_join_path(const char *base_path, const char *sub_path, char *result_path, size_t result_size);
esp_err_t fs_get_filename(const char *path, char *filename, size_t filename_size);
esp_err_t fs_get_file_extension(const char *path, char *extension, size_t extension_size);

// 存储空间信息
typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint64_t used_bytes;
} storage_info_t;

esp_err_t fs_get_storage_info(const char *path, storage_info_t *info);

#ifdef __cplusplus
}
#endif
