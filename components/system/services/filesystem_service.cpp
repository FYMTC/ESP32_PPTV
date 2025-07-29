#include "filesystem_service.hpp"
#include "sd_init.hpp"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "esp_vfs_fat.h"

static const char *FS_TAG = "FileSystem";

esp_err_t fs_init(void)
{
    ESP_LOGI(FS_TAG, "File system service initialized");
    return ESP_OK;
}

bool filesystem_service_is_available(void)
{
    // 检查SD卡是否挂载
    return is_sd_card_mounted();
}

bool fs_is_path_exists(const char *path)
{
    if (!path) return false;
    
    struct stat st;
    return (stat(path, &st) == 0);
}

bool fs_is_directory(const char *path)
{
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool fs_is_file(const char *path)
{
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISREG(st.st_mode);
    }
    return false;
}

esp_err_t fs_create_directory(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (mkdir(path, 0755) == 0) {
        ESP_LOGI(FS_TAG, "Directory created: %s", path);
        return ESP_OK;
    }
    
    ESP_LOGE(FS_TAG, "Failed to create directory %s: %s", path, strerror(errno));
    return ESP_FAIL;
}

esp_err_t fs_remove_directory(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (rmdir(path) == 0) {
        ESP_LOGI(FS_TAG, "Directory removed: %s", path);
        return ESP_OK;
    }
    
    ESP_LOGE(FS_TAG, "Failed to remove directory %s: %s", path, strerror(errno));
    return ESP_FAIL;
}

static int compare_files(const void *a, const void *b, sort_type_t sort_type)
{
    file_info_t *file_a = (file_info_t *)a;
    file_info_t *file_b = (file_info_t *)b;
    
    // 目录总是排在前面
    if (file_a->type == FILE_TYPE_DIRECTORY && file_b->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    if (file_b->type == FILE_TYPE_DIRECTORY && file_a->type != FILE_TYPE_DIRECTORY) {
        return 1;
    }
    
    switch (sort_type) {
        case SORT_BY_NAME_ASC:
            return strcasecmp(file_a->name, file_b->name);
        case SORT_BY_NAME_DESC:
            return strcasecmp(file_b->name, file_a->name);
        case SORT_BY_SIZE_ASC:
            return (file_a->size < file_b->size) ? -1 : (file_a->size > file_b->size) ? 1 : 0;
        case SORT_BY_SIZE_DESC:
            return (file_b->size < file_a->size) ? -1 : (file_b->size > file_a->size) ? 1 : 0;
        case SORT_BY_TIME_ASC:
            return (file_a->modified_time < file_b->modified_time) ? -1 : (file_a->modified_time > file_b->modified_time) ? 1 : 0;
        case SORT_BY_TIME_DESC:
            return (file_b->modified_time < file_a->modified_time) ? -1 : (file_b->modified_time > file_a->modified_time) ? 1 : 0;
        case SORT_BY_TYPE:
        default:
            return strcasecmp(file_a->name, file_b->name);
    }
}

static int file_compare_wrapper(const void *a, const void *b)
{
    // 这里需要一个全局变量来传递排序类型，或者使用其他方法
    return compare_files(a, b, SORT_BY_NAME_ASC);
}

esp_err_t fs_list_directory(const char *path, file_info_t **files, int *count, sort_type_t sort)
{
    if (!path || !files || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *files = NULL;
    *count = 0;
    
    // 检查SD卡是否挂载
    if (!is_sd_card_mounted()) {
        ESP_LOGW(FS_TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(FS_TAG, "Failed to open directory %s: %s", path, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    
    // 首先计算文件数量
    struct dirent *entry;
    int file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过. 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        file_count++;
    }
    
    if (file_count == 0) {
        closedir(dir);
        return ESP_OK;
    }
    
    // 分配内存
    *files = (file_info_t *)malloc(file_count * sizeof(file_info_t));
    if (!*files) {
        closedir(dir);
        return ESP_ERR_NO_MEM;
    }
    
    // 重新开始读取
    rewinddir(dir);
    int index = 0;
    
    while ((entry = readdir(dir)) != NULL && index < file_count) {
        // 跳过. 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        file_info_t *file_info = &(*files)[index];
        
        // 设置文件名
        strncpy(file_info->name, entry->d_name, sizeof(file_info->name) - 1);
        file_info->name[sizeof(file_info->name) - 1] = '\0';
        
        // 安全构建完整路径，避免缓冲区溢出
        size_t path_len = strlen(path);
        size_t name_len = strlen(entry->d_name);
        if (path_len + name_len + 2 <= sizeof(file_info->full_path)) {
            // 手动构建路径以避免格式字符串警告
            strcpy(file_info->full_path, path);
            strcat(file_info->full_path, "/");
            strcat(file_info->full_path, entry->d_name);
        } else {
            // 路径太长，截断处理
            strncpy(file_info->full_path, path, sizeof(file_info->full_path) - 1);
            file_info->full_path[sizeof(file_info->full_path) - 1] = '\0';
            ESP_LOGW(FS_TAG, "Path too long, truncated");
        }
        
        // 获取文件信息
        struct stat st;
        if (stat(file_info->full_path, &st) == 0) {
            file_info->size = st.st_size;
            file_info->modified_time = st.st_mtime;
            
            if (S_ISDIR(st.st_mode)) {
                file_info->type = FILE_TYPE_DIRECTORY;
            } else if (S_ISREG(st.st_mode)) {
                file_info->type = FILE_TYPE_REGULAR;
            } else {
                file_info->type = FILE_TYPE_UNKNOWN;
            }
        } else {
            file_info->size = 0;
            file_info->modified_time = 0;
            file_info->type = FILE_TYPE_UNKNOWN;
        }
        
        // 检查是否为隐藏文件
        file_info->is_hidden = (entry->d_name[0] == '.');
        
        index++;
    }
    
    closedir(dir);
    *count = index;
    
    // 排序文件列表
    if (*count > 1) {
        qsort(*files, *count, sizeof(file_info_t), file_compare_wrapper);
    }
    
    ESP_LOGI(FS_TAG, "Listed %d files in directory: %s", *count, path);
    return ESP_OK;
}

void fs_free_file_list(file_info_t *files, int count)
{
    if (files) {
        free(files);
    }
}

esp_err_t fs_open_directory(const char *path, dir_iterator_t *iterator)
{
    if (!path || !iterator) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(iterator, 0, sizeof(dir_iterator_t));
    
    iterator->dir_handle = opendir(path);
    if (!iterator->dir_handle) {
        ESP_LOGE(FS_TAG, "Failed to open directory %s: %s", path, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    
    strncpy(iterator->current_path, path, sizeof(iterator->current_path) - 1);
    iterator->current_path[sizeof(iterator->current_path) - 1] = '\0';
    
    return ESP_OK;
}

esp_err_t fs_read_next_file(dir_iterator_t *iterator, file_info_t *file_info)
{
    if (!iterator || !file_info || !iterator->dir_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct dirent *entry;
    do {
        entry = readdir(iterator->dir_handle);
        if (!entry) {
            return ESP_ERR_NOT_FOUND;  // 没有更多文件
        }
    } while (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0);
    
    // 填充文件信息
    strncpy(file_info->name, entry->d_name, sizeof(file_info->name) - 1);
    file_info->name[sizeof(file_info->name) - 1] = '\0';
    
    // 安全构建完整路径，避免缓冲区溢出
    size_t path_len = strlen(iterator->current_path);
    size_t name_len = strlen(entry->d_name);
    if (path_len + name_len + 2 <= sizeof(file_info->full_path)) {
        // 手动构建路径以避免格式字符串警告
        strcpy(file_info->full_path, iterator->current_path);
        strcat(file_info->full_path, "/");
        strcat(file_info->full_path, entry->d_name);
    } else {
        // 路径太长，截断处理
        strncpy(file_info->full_path, iterator->current_path, sizeof(file_info->full_path) - 1);
        file_info->full_path[sizeof(file_info->full_path) - 1] = '\0';
        ESP_LOGW(FS_TAG, "Path too long, truncated");
    }
    
    struct stat st;
    if (stat(file_info->full_path, &st) == 0) {
        file_info->size = st.st_size;
        file_info->modified_time = st.st_mtime;
        
        if (S_ISDIR(st.st_mode)) {
            file_info->type = FILE_TYPE_DIRECTORY;
            iterator->dir_count++;
        } else if (S_ISREG(st.st_mode)) {
            file_info->type = FILE_TYPE_REGULAR;
            iterator->file_count++;
        } else {
            file_info->type = FILE_TYPE_UNKNOWN;
        }
    } else {
        file_info->size = 0;
        file_info->modified_time = 0;
        file_info->type = FILE_TYPE_UNKNOWN;
    }
    
    file_info->is_hidden = (entry->d_name[0] == '.');
    
    return ESP_OK;
}

void fs_close_directory(dir_iterator_t *iterator)
{
    if (iterator && iterator->dir_handle) {
        closedir(iterator->dir_handle);
        iterator->dir_handle = NULL;
    }
}

esp_err_t fs_get_file_info(const char *path, file_info_t *info)
{
    if (!path || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(FS_TAG, "Failed to get file info for %s: %s", path, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    
    // 提取文件名
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++; // 跳过 '/'
    } else {
        filename = path;
    }
    
    strncpy(info->name, filename, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    
    strncpy(info->full_path, path, sizeof(info->full_path) - 1);
    info->full_path[sizeof(info->full_path) - 1] = '\0';
    
    info->size = st.st_size;
    info->modified_time = st.st_mtime;
    info->is_hidden = (filename[0] == '.');
    
    if (S_ISDIR(st.st_mode)) {
        info->type = FILE_TYPE_DIRECTORY;
    } else if (S_ISREG(st.st_mode)) {
        info->type = FILE_TYPE_REGULAR;
    } else {
        info->type = FILE_TYPE_UNKNOWN;
    }
    
    return ESP_OK;
}

esp_err_t fs_delete_file(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (unlink(path) == 0) {
        ESP_LOGI(FS_TAG, "File deleted: %s", path);
        return ESP_OK;
    }
    
    ESP_LOGE(FS_TAG, "Failed to delete file %s: %s", path, strerror(errno));
    return ESP_FAIL;
}

esp_err_t fs_rename_file(const char *old_name, const char *new_name)
{
    if (!old_name || !new_name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (rename(old_name, new_name) == 0) {
        ESP_LOGI(FS_TAG, "File renamed from %s to %s", old_name, new_name);
        return ESP_OK;
    }
    
    ESP_LOGE(FS_TAG, "Failed to rename file from %s to %s: %s", old_name, new_name, strerror(errno));
    return ESP_FAIL;
}

esp_err_t fs_get_parent_path(const char *path, char *parent_path, size_t parent_path_size)
{
    if (!path || !parent_path || parent_path_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(parent_path, path, parent_path_size - 1);
    parent_path[parent_path_size - 1] = '\0';
    
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash) {
        *last_slash = '\0';
    }
    
    // 如果结果为空字符串，返回根目录
    if (strlen(parent_path) == 0) {
        strncpy(parent_path, "/:", parent_path_size - 1);
        parent_path[parent_path_size - 1] = '\0';
    }
    
    return ESP_OK;
}

esp_err_t fs_join_path(const char *base_path, const char *sub_path, char *result_path, size_t result_size)
{
    if (!base_path || !sub_path || !result_path || result_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 安全构建路径以避免格式截断警告
    size_t base_len = strlen(base_path);
    size_t sub_len = strlen(sub_path);
    if (base_len + sub_len + 2 <= result_size) {
        strcpy(result_path, base_path);
        strcat(result_path, "/");
        strcat(result_path, sub_path);
    } else {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t fs_get_filename(const char *path, char *filename, size_t filename_size)
{
    if (!path || !filename || filename_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *last_slash = strrchr(path, '/');
    const char *name = last_slash ? (last_slash + 1) : path;
    
    strncpy(filename, name, filename_size - 1);
    filename[filename_size - 1] = '\0';
    
    return ESP_OK;
}

esp_err_t fs_get_file_extension(const char *path, char *extension, size_t extension_size)
{
    if (!path || !extension || extension_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *last_dot = strrchr(path, '.');
    if (last_dot && last_dot != path) {
        strncpy(extension, last_dot + 1, extension_size - 1);
        extension[extension_size - 1] = '\0';
    } else {
        extension[0] = '\0';  // 没有扩展名
    }
    
    return ESP_OK;
}

esp_err_t fs_get_storage_info(const char *path, storage_info_t *info)
{
    if (!path || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 对于ESP-IDF，我们可以通过SD卡信息来获取存储容量
    sd_card_info_t sd_info;
    if (get_sd_card_info(&sd_info) == ESP_OK && sd_info.is_mounted) {
        info->total_bytes = (uint64_t)sd_info.capacity_mb * 1024 * 1024;
        
        // 简单的空闲空间估算 - 在实际应用中可能需要更精确的方法
        // 这里我们假设有80%的可用空间（这只是一个估算）
        info->free_bytes = info->total_bytes * 8 / 10;
        info->used_bytes = info->total_bytes - info->free_bytes;
        
        ESP_LOGD(FS_TAG, "Storage info - Total: %llu MB, Free: %llu MB", 
                info->total_bytes / (1024*1024), info->free_bytes / (1024*1024));
        return ESP_OK;
    }
    
    // 如果无法获取SD卡信息，设置默认值
    info->total_bytes = 0;
    info->free_bytes = 0;
    info->used_bytes = 0;
    
    ESP_LOGW(FS_TAG, "Unable to get accurate storage info for %s", path);
    return ESP_ERR_NOT_SUPPORTED;
}
