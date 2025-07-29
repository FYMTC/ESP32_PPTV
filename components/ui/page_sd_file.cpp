#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include "filesystem_service.hpp"
#include "sd_init.hpp"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SD_FileBrowser";

extern PageManager g_pageManager;
static void file_list_event_cb(lv_event_t *e);
// UI元素
static lv_obj_t *sd_page = nullptr;
static lv_obj_t *file_list = nullptr;
static lv_obj_t *path_label = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_obj_t *back_btn = nullptr;
static lv_obj_t *refresh_btn = nullptr;

// 当前路径和文件数据
#ifdef _WIN32
static char current_path[MAX_PATH_LEN] = "C:\\Users";
#else
static char current_path[MAX_PATH_LEN] = sdcard_mount_point;
#endif
static file_info_t *current_files = nullptr;
static int current_file_count = 0;
static bool is_loading = false;  // 加载状态标识

// 图标定义 (使用LVGL内置符号)
#define ICON_FOLDER     LV_SYMBOL_DIRECTORY
#define ICON_FILE       LV_SYMBOL_FILE
#define ICON_BACK       LV_SYMBOL_LEFT
#define ICON_REFRESH    LV_SYMBOL_REFRESH
#define ICON_UP         LV_SYMBOL_UP

// 格式化文件大小
static void format_file_size(size_t size, char *buffer, size_t buffer_size)
{
    if (size < 1024) {
        snprintf(buffer, buffer_size, "%zu B", size);
    } else if (size < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", size / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

// 获取文件类型图标
static const char* get_file_icon(file_type_t type, const char* filename)
{
    if (type == FILE_TYPE_DIRECTORY) {
        return ICON_FOLDER;
    }

    // 根据文件扩展名返回不同图标
    const char *ext = strrchr(filename, '.');
    if (ext) {
        ext++; // 跳过点号
        if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0) {
            return LV_SYMBOL_EDIT;
        } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "png") == 0 ||
                   strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "gif") == 0) {
            return LV_SYMBOL_IMAGE;
        } else if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0) {
            return LV_SYMBOL_AUDIO;
        } else if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0) {
            return LV_SYMBOL_VIDEO;
        }
    }

    return ICON_FILE;
}

// 更新状态标签
static void update_status_label()
{
    if (!status_label) return;

    if (!filesystem_service_is_available()) {
        lv_label_set_text(status_label, "Filesystem not available");
        return;
    }

    // 获取存储信息
    storage_info_t storage_info;
    if (fs_get_storage_info(current_path, &storage_info) == ESP_OK) {
        char size_str[64];
        char free_str[64];
        format_file_size(storage_info.total_bytes, size_str, sizeof(size_str));
        format_file_size(storage_info.free_bytes, free_str, sizeof(free_str));

        lv_label_set_text_fmt(status_label, "%s/%s | %d",
                              free_str, size_str,  current_file_count);
    } else {
        lv_label_set_text_fmt(status_label, "Files: %d", current_file_count);
    }
}

// 清理文件列表数据
static void cleanup_file_data()
{
    if (current_files) {
        fs_free_file_list(current_files, current_file_count);
        current_files = nullptr;
        current_file_count = 0;
    }
}

// 刷新文件列表
static void refresh_file_list()
{
    if (!file_list || is_loading) return;  // 如果正在加载则返回

    is_loading = true;  // 设置加载状态

    // 显示加载提示
    if (status_label) {
        lv_label_set_text(status_label, "Loading...");
    }

    // 清除现有项目
    lv_obj_clean(file_list);
    cleanup_file_data();

    // 检查文件系统是否可用
    if (!filesystem_service_is_available()) {
        lv_obj_t *no_fs_label = lv_label_create(file_list);
        lv_label_set_text(no_fs_label, "文件系统不可用");
		lv_obj_set_style_text_font(no_fs_label, &NotoSansSC_Medium_3500, 0);
        lv_obj_set_style_text_align(no_fs_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(no_fs_label);
        update_status_label();
        is_loading = false;  // 清除加载状态
        return;
    }

    // 检查当前路径是否存在
    if (!fs_is_path_exists(current_path)) {
        lv_obj_t *no_path_label = lv_label_create(file_list);
        lv_label_set_text_fmt(no_path_label, "路径不存在: %s", current_path);
		lv_obj_set_style_text_font(no_path_label, &NotoSansSC_Medium_3500, 0);
        lv_obj_set_style_text_align(no_path_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(no_path_label);
        update_status_label();
        is_loading = false;  // 清除加载状态
        return;
    }

    // 添加返回上级目录项（如果不在根目录）
#ifdef _WIN32
    bool is_root = (strlen(current_path) <= 3 && current_path[1] == ':');
#else
    bool is_root = (strcmp(current_path, "/") == 0);
#endif
    if (!is_root) {
        lv_obj_t *back_item = lv_list_add_btn(file_list, ICON_UP, "返回");
        lv_obj_set_style_bg_color(back_item, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_text_color(back_item, lv_color_white(), 0);
		lv_obj_set_style_text_font(back_item, &NotoSansSC_Medium_3500, 0);
        // 设置特殊用户数据标识返回按钮
        lv_obj_set_user_data(back_item, (void*)(-1));
        // 为返回按钮添加点击事件处理
        lv_obj_add_event_cb(back_item, file_list_event_cb, LV_EVENT_CLICKED, NULL);
    }

    // 获取文件列表
    esp_err_t ret = fs_list_directory(current_path, &current_files, &current_file_count, SORT_BY_NAME_ASC);
    if (ret != ESP_OK) {
        lv_obj_t *error_label = lv_label_create(file_list);
        lv_label_set_text_fmt(error_label, "无法读取目录: %s", esp_err_to_name(ret));
		lv_obj_set_style_text_font(error_label, &NotoSansSC_Medium_3500, 0);
        lv_obj_set_style_text_align(error_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(error_label);
        update_status_label();
        is_loading = false;  // 清除加载状态
        return;
    }

    // 添加文件和目录到列表
    for (int i = 0; i < current_file_count; i++) {
        file_info_t *file = &current_files[i];

        // 跳过隐藏文件（可选）
        if (file->is_hidden) {
            continue;
        }

        const char *icon = get_file_icon(file->type, file->name);

        // 创建文件项
        char item_text[512];
        if (file->type == FILE_TYPE_DIRECTORY) {
            snprintf(item_text, sizeof(item_text), "%s", file->name);
        } else {
            char size_str[32];
            format_file_size(file->size, size_str, sizeof(size_str));
            snprintf(item_text, sizeof(item_text), "%s (%s)", file->name, size_str);
        }

        lv_obj_t *item = lv_list_add_btn(file_list, icon, item_text);

        // 为目录和文件设置不同的颜色
        if (file->type == FILE_TYPE_DIRECTORY) {
            lv_obj_set_style_text_color(item, lv_color_hex(0x4CAF50), 0);
        }

        // 存储文件索引到用户数据
        lv_obj_set_user_data(item, (void*)(intptr_t)i);

        // 为每个列表项添加点击事件处理
        lv_obj_add_event_cb(item, file_list_event_cb, LV_EVENT_CLICKED, NULL);
    }

    // 更新路径标签
    if (path_label) {
        lv_label_set_text(path_label, current_path);
    }

    // 更新状态标签
    update_status_label();

    is_loading = false;  // 清除加载状态

    ESP_LOGI(TAG, "File list refreshed: %d items in %s", current_file_count, current_path);
}

// 文件列表点击事件处理
static void file_list_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        // 防止重复点击
        if (is_loading) {
            ESP_LOGW(TAG, "Already loading, ignoring click");
            return;
        }

        // 获取用户数据
        intptr_t file_index = (intptr_t)lv_obj_get_user_data(btn);
        ESP_LOGI(TAG, "Button clicked, file_index: %d", (int)file_index);

        // 检查是否是返回上级目录按钮
        if (file_index == -1) {
            ESP_LOGI(TAG, "Back button clicked, current path: %s", current_path);

            // 检查是否在根目录
#ifdef _WIN32
            bool is_root = (strlen(current_path) <= 3 && current_path[1] == ':');
#else
            bool is_root = (strcmp(current_path, sdcard_mount_point) == 0);
#endif
            if (is_root) {
                // 在根目录时，退出页面
                ESP_LOGI(TAG, "At root directory, exiting page");
                cleanup_file_data();
                g_pageManager.back();
                return;
            }

            // 返回上级目录
            char parent_path[MAX_PATH_LEN];
            if (fs_get_parent_path(current_path, parent_path, sizeof(parent_path)) == ESP_OK) {
                strncpy(current_path, parent_path, sizeof(current_path) - 1);
                current_path[sizeof(current_path) - 1] = '\0';
                ESP_LOGI(TAG, "Going back to parent directory: %s", current_path);
                refresh_file_list();
            } else {
                ESP_LOGE(TAG, "Failed to get parent path");
            }
            return;
        }

        // 处理文件/目录点击
        if (file_index >= 0 && file_index < current_file_count) {
            file_info_t *selected_file = &current_files[file_index];

            ESP_LOGI(TAG, "File clicked: %s, type: %d", selected_file->name, selected_file->type);

            if (selected_file->type == FILE_TYPE_DIRECTORY) {
                // 进入目录
                strncpy(current_path, selected_file->full_path, sizeof(current_path) - 1);
                current_path[sizeof(current_path) - 1] = '\0';
                ESP_LOGI(TAG, "Entering directory: %s", current_path);
                refresh_file_list();
            } else {
                // 选中文件，显示信息（可以扩展为打开文件）
                ESP_LOGI(TAG, "Selected file: %s (%zu bytes)",
                        selected_file->name, selected_file->size);

                // 创建文件信息对话框
                lv_obj_t *mbox = lv_msgbox_create(NULL);
				lv_obj_set_style_text_font(mbox, &NotoSansSC_Medium_3500, 0);
                lv_obj_set_size(mbox, LV_PCT(100), LV_SIZE_CONTENT);
                lv_msgbox_add_title(mbox, "File Info");

                // 格式化文件信息
                char file_info[1024];
                char size_str[32];
                format_file_size(selected_file->size, size_str, sizeof(size_str));

                int written = snprintf(file_info, sizeof(file_info),
                    "Name: %s\nPath: %s\nSize: %s",
                    selected_file->name,
                    selected_file->full_path,
                    size_str);
                if (written < 0 || written >= (int)sizeof(file_info)) {
                    // Truncated, show warning
                    strncpy(file_info + sizeof(file_info) - 32, "\n[Info truncated]", 31);
                    file_info[sizeof(file_info) - 1] = '\0';
                }

                lv_msgbox_add_text(mbox, file_info);

                // 添加关闭按钮
                lv_msgbox_add_close_button(mbox);

                lv_obj_center(mbox);
            }
        } else {
            ESP_LOGW(TAG, "Invalid file index: %d", (int)file_index);
        }
    }
}

// 刷新按钮事件处理
static void refresh_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (!is_loading) {  // 防止重复点击
            ESP_LOGI(TAG, "Refresh button clicked");
            refresh_file_list();
        }
    }
}

// 返回按钮事件处理
static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        cleanup_file_data();
        g_pageManager.back();
    }
}

lv_obj_t* createPage_sd_files()
{
    // 创建主页面
    sd_page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sd_page, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(sd_page, 0, 0);  // 去除边距
    lv_obj_set_flex_flow(sd_page, LV_FLEX_FLOW_COLUMN);  // 弹性布局
    lv_obj_set_flex_align(sd_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_border_width(sd_page, 0, 0);  // 去除边框
	lv_obj_set_size(sd_page, LV_HOR_RES, LV_VER_RES);
	lv_obj_align(sd_page, LV_ALIGN_CENTER, 0, 0);


    // 创建顶部工具栏
    lv_obj_t *toolbar = lv_obj_create(sd_page);
    lv_obj_set_size(toolbar, 240, LV_SIZE_CONTENT);  // 固定高度
    lv_obj_set_style_bg_color(toolbar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(toolbar, 0, 0);
    lv_obj_set_style_radius(toolbar, 0, 0);
    lv_obj_set_style_pad_all(toolbar, 0, 0);  // 设置内边距
    lv_obj_clear_flag(toolbar, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动

    // 返回按钮
    back_btn = lv_btn_create(toolbar);
    lv_obj_set_size(back_btn, 30, 30);  // 调整按钮大小
    lv_obj_set_pos(back_btn, 5, 7);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, ICON_BACK);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // 刷新按钮
    refresh_btn = lv_btn_create(toolbar);
    lv_obj_set_size(refresh_btn, 30, 30);  // 调整按钮大小
    lv_obj_set_pos(refresh_btn, 205, 7);
    lv_obj_t *refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, ICON_REFRESH);
    lv_obj_center(refresh_label);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // 标题
    lv_obj_t *title_label = lv_label_create(toolbar);
    lv_label_set_text(title_label, MY_SYMBOL_FILES "文件浏览器 ");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &NotoSansSC_Medium_3500, 0);  // 使用中文字体
    lv_obj_set_pos(title_label, 45, 12);  // 调整位置

    // 创建路径标签
    path_label = lv_label_create(sd_page);
    lv_obj_set_size(path_label, 240, LV_SIZE_CONTENT);  // 设置固定高度
    lv_label_set_text(path_label, current_path);
    lv_obj_set_style_text_color(path_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(path_label, &NotoSansSC_Medium_3500, 0);  // 使用中文字体
    lv_obj_set_style_pad_all(path_label, 0, 0);  // 设置内边距
    lv_obj_set_style_border_width(path_label, 0, 0);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // 创建文件列表
    file_list = lv_list_create(sd_page);
    lv_obj_set_size(file_list, 240, LV_SIZE_CONTENT);  // 自适应高度
    lv_obj_set_flex_grow(file_list, 1);  // 占用剩余空间
    lv_obj_set_style_bg_color(file_list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_all(file_list, 0, 0);  // 去除边距

    // 创建状态栏
    status_label = lv_label_create(sd_page);
    lv_obj_set_size(status_label, 240, LV_SIZE_CONTENT);  // 固定高度
    lv_label_set_text(status_label, "Loading...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(status_label, &NotoSansSC_Medium_3500, 0);  // 使用中文字体
    lv_obj_set_style_pad_all(status_label, 0, 0);  // 设置内边距
    lv_obj_set_style_border_width(status_label, 0, 0);

    // 初始化文件系统服务
    fs_init();

    // 检查文件系统状态
    if (filesystem_service_is_available()) {
        ESP_LOGI(TAG, "Filesystem service initialized successfully");
    } else {
        ESP_LOGW(TAG, "Filesystem service initialization failed");
    }

    // 刷新文件列表
    refresh_file_list();

    ESP_LOGI(TAG, "File browser page created");
    return sd_page;
}
