#include <stdint.h>
#include <mem.h>
#include <vga.h>
#include <string.h>
#include <pcfs.h>

void vfs_init() {
    if(root) return;
    
    root = malloc(sizeof(VFSNode));
    if(!root) {
        kprintf("Memory allocation failed for root\n");
        return;
    }
    
    strncpy(root->name, "/", MAX_NAME_LEN);
    root->type = NODE_DIR;
    root->size = 0;
    root->content = NULL;
    root->parent = NULL;
    root->num_children = 0;
    kprintf("Root directory initialized\n");
}

VFSNode* find_node(const char *path) {
    if(!root) return NULL;
    if(strcmp(path, "/") == 0) return root;

    char tmp_path[MAX_PATH_LEN];
    strncpy(tmp_path, path, MAX_PATH_LEN);
    tmp_path[MAX_PATH_LEN-1] = '\0';

    VFSNode *current = root;
    char *token = strtok(tmp_path, "/");
    
    while(token != NULL) {
        int found = 0;
        for(int i = 0; i < current->num_children; i++) {
            if(strcmp(current->children[i]->name, token) == 0) {
                current = current->children[i];
                found = 1;
                break;
            }
        }
        if(!found) return NULL;
        token = strtok(NULL, "/");
    }
    return current;
}

int vfs_create_dir(const char *parent_path, const char *name) {
    if(!root) {
        kprintf("Filesystem not initialized\n");
        return -1;
    }
    
    VFSNode *parent = find_node(parent_path);
    if(!parent) {
        kprintf("Parent directory '%s' not found\n", parent_path);
        return -2;
    }
    
    if(parent->type != NODE_DIR) {
        kprintf("Path '%s' is not a directory\n", parent_path);
        return -3;
    }
    
    if(parent->num_children >= MAX_DIR_CHILDREN) {
        kprintf("Directory is full\n");
        return -4;
    }

    VFSNode *new_dir = malloc(sizeof(VFSNode));
    if(!new_dir) {
        kprintf("Memory allocation failed\n");
        return -5;
    }

    strncpy(new_dir->name, name, MAX_NAME_LEN);
    new_dir->type = NODE_DIR;
    new_dir->size = 0;
    new_dir->content = NULL;
    new_dir->parent = parent;
    new_dir->num_children = 0;

    parent->children[parent->num_children++] = new_dir;
    return 0;
}

int setup_system_t()
{
    vfs_create_dir("/", "Focus");
    vfs_create_dir("/", "Programs");
    vfs_create_dir("/Focus", "Libs");
    vfs_create_dir("/Focus", "Logs");
    vfs_create_dir("/Focus", "Utils");
}

void vfs_list_dir(const char *path) {
    VFSNode *dir = find_node(path);
    if(!dir || dir->type != NODE_DIR) {
        kprintf("Directory '%s' not found\n", path);
        return;
    }
    
    kprintf("\n%s:\n", path);

    // Сначала выводим директории
    for(int i = 0; i < dir->num_children; i++) {
        if(dir->children[i]->type == NODE_DIR) {
            kprintf("[DIR] %s\n", 
                  dir->children[i]->name);
        }
    }
    
    // Затем файлы
    for(int i = 0; i < dir->num_children; i++) {
        if(dir->children[i]->type == NODE_FILE) {
            kprintf("[FILE] %s %d bytes\n", 
                  dir->children[i]->name,
                  dir->children[i]->size);
        }
    }
    kprintf("\n");
}

// Запись в файл
int vfs_write_file(VFSNode *file, const char *data, uint32_t size) {
    if(!file || file->type != NODE_FILE) return -1;
    
    if(file->content) mfree(file->content);
    
    file->content = malloc(size);
    memcpy(file->content, data, size);
    file->size = size;
    return 0;
}

// Чтение файла
char* vfs_read_file(VFSNode *file) {
    if(!file || file->type != NODE_FILE) return NULL;
    return file->content;
}

// Удаление файла/директории
int vfs_delete(const char *path) {
    VFSNode *node = find_node(path);
    if(!node) return -1;
    
    VFSNode *parent = node->parent;
    for(int i = 0; i < parent->num_children; i++) {
        if(parent->children[i] == node) {
            mfree(node->content);
            mfree(node);
            memmove(&parent->children[i], &parent->children[i+1], 
                   (parent->num_children - i - 1) * sizeof(VFSNode*));
            parent->num_children--;
            return 0;
        }
    }
    return -2;
}

int vfs_create_file(const char *path, const char *name) {
    VFSNode *parent = find_node(path);
    if(!parent || parent->type != NODE_DIR) return -1;
    
    if(parent->num_children >= MAX_DIR_CHILDREN) return -2;
    
    VFSNode *new_node = malloc(sizeof(VFSNode));
    strcpy(new_node->name, name);
    new_node->type = NODE_FILE;
    new_node->size = 0;
    new_node->content = NULL;
    new_node->parent = parent;
    new_node->num_children = 0;
    
    parent->children[parent->num_children++] = new_node;
    return 0;
}