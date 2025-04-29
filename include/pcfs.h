#include <stdint.h>
#include <stddef.h>

#define MAX_NAME_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_DIR_CHILDREN 1024

typedef enum {
    NODE_FILE,
    NODE_DIR
} NodeType;

typedef struct VFSNode {
    char name[MAX_NAME_LEN];
    NodeType type;
    uint32_t size;
    char *content;
    
    struct VFSNode *parent;
    struct VFSNode *children[MAX_DIR_CHILDREN];
    int num_children;
} VFSNode;

VFSNode *root = NULL;

// Инициализация файловой системы
void vfs_init();
// Поиск узла по пути (внутренняя функция)
VFSNode* find_node(const char *path);
// Создание файла
int vfs_create_file(const char *path, const char *name);
// Создание директории
int vfs_create_dir(const char *path, const char *name);
// Запись в файл
int vfs_write_file(VFSNode *file, const char *data, uint32_t size);
// Чтение файла
char* vfs_read_file(VFSNode *file);
// Удаление файла/директории
int vfs_delete(const char *path);
// Листинг директории
void vfs_list_dir(const char *path);