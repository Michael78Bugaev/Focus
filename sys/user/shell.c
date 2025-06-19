#include <vga.h>
#include <string.h>
#include <fat32.h>
#include <mem.h>
#include <iso9660.h>
#include <elf_loader.h>

extern uint32_t current_dir_cluster;
extern void enter_user(void (*entry)(void));

#define USER_STACK_TOP 0x010FF000u

/* Простейшая командная оболочка уровня ядра. Использует kprintf/kgetch. */

static void print_prompt(void) {
    kprintf("FocusOS &");
}

static void cmd_help(void) {
    kprintf("Available commands:\n");
    kprintf("  help   - help\n");
    kprintf("  ls     - list files (FAT32)\n");
    kprintf("  cd DIR - change directory (FAT32)\n");
    kprintf("  cat F  - print file content\n");
    kprintf("  clear  - clear screen\n");
    kprintf("  exec P - run user program (cdrom:/path)\n");
}

static void cmd_clear(void) { kclear(); }

static void cmd_ls(void) {
    fat32_entry_t list[64];
    int n = fat32_list_dir(0, current_dir_cluster, list, 64);
    if (n<0) { kprintf("ls: error %d\n", n); return; }
    for(int i=0;i<n;i++) {
        kprintf("%s%s\n", list[i].name, (list[i].attr & 0x10)?"/":"");
    }
}

static void cmd_cd(const char *arg) {
    if (!arg||!arg[0]) return;
    int r = fat32_change_dir(0, arg);
    if (r!=0) kprintf("cd: error %d\n", r);
}

static void cmd_cat(const char *arg) {
    if (!arg||!arg[0]) return;
    uint32_t cluster=0;
    int res = fat32_resolve_path(0, arg, &cluster);
    if(res!=0){ kprintf("cat: path error %d\n", res); return; }
    uint32_t size = 4096;
    uint8_t *buf = malloc(size);
    if (!buf) { kprintf("cat: no mem\n"); return; }
    int r = fat32_read_file(0, cluster, buf, size-1);
    if (r<0) { kprintf("cat: read error %d\n", r); mfree(buf); return; }
    buf[r] = 0;
    kprintf("%s\n", buf);
    mfree(buf);
}

static void cmd_exec(const char *arg) {
    if (!arg || !arg[0]) { kprintf("exec: no path\n"); return; }
    /* пока поддерживаем только CD-ROM путь */
    if (strncmp(arg, "cdrom:", 6) != 0 && strncmp(arg, "CDROM:", 6) != 0) {
        kprintf("exec: only cdrom:/ paths supported for now\n");
        return;
    }

    /* определяем размер файла */
    uint32_t lba=0, fsize=0;
    if (iso9660_find(arg, &lba, &fsize) != 0) { kprintf("exec: not found\n"); return; }
    uint8_t *buf = malloc(fsize);
    if (!buf) { kprintf("exec: no mem (%u bytes)\n", fsize); return; }
    int rb = iso9660_read(arg, buf, fsize);
    if (rb <= 0) { kprintf("exec: read error %d\n", rb); mfree(buf); return; }

    void (*entry)(void) = 0;
    if (elf_load_image(buf, fsize, USER_STACK_TOP, &entry) != 0) {
        kprintf("exec: elf load failed\n"); mfree(buf); return; }

    /* гарантируем доступность стека для пользователя */
    extern void ensure_region_mapped(uint32_t, uint32_t);
    ensure_region_mapped(USER_STACK_TOP - 0x40000, 0x40000);

    /* --- сбрасываем TLB, чтобы новые USER-биты вступили в силу --- */
    asm volatile ("mov %%cr3, %%eax\n\t"
                  "mov %%eax, %%cr3" :: : "eax", "memory");

    kprintf("[exec] switching to user mode...\n");
    uint32_t esp0; asm("mov %%esp,%0":"=r"(esp0));
    exec_update_tss(esp0);
    enter_user(entry);
    kprintf("[exec] returned from user program\n");
    mfree(buf);
}

static void execute_command(const char *line) {
    char cmd[64]; char arg[192];
    /* разделяем строку на cmd и arg */
    int len = strlen(line);
    int i=0; while(i<len && line[i]==' ') i++;
    int j=0; while(i<len && line[i]!=' ' && j<63) cmd[j++] = line[i++];
    cmd[j] = 0;
    while(i<len && line[i]==' ') i++;
    j=0; while(i<len && j<191) arg[j++] = line[i++];
    arg[j]=0;

    if(cmd[0]==0) return;
    if (strcmp(cmd,"help")==0) cmd_help();
    else if (strcmp(cmd,"ls")==0) cmd_ls();
    else if (strcmp(cmd,"clear")==0) cmd_clear();
    else if (strcmp(cmd,"cd")==0) cmd_cd(arg[0]?arg:NULL);
    else if (strcmp(cmd,"cat")==0) cmd_cat(arg[0]?arg:NULL);
    else if (strcmp(cmd,"exec")==0) cmd_exec(arg[0]?arg:NULL);
    else kprintf("Unknown command: %s\n", cmd);
}

void shell_main(void) {
    char line[256]; int pos=0;
    print_prompt();
    while (1) {
        int c = kgetch();
        if (c=='\r' || c=='\n') {
            line[pos] = 0; kprintf("\n");
            execute_command(line);
            pos=0;
            print_prompt();
            continue;
        }
        if (c==8) { // backspace
            if (pos>0) { pos--; kprintf("\b \b"); }
            continue;
        }
        if (pos<255) {
            line[pos++] = (char)c;
            kputchar((char)c, 0x0f);
        }
    }
}

/* точка возврата из enter_user() (CPL0) */
void shell_return_from_user(void) {
    kprintf("[exec] returned from user program\n");
    /* ничего: продолжим цикл shell_main(), тк стек правильный */
} 