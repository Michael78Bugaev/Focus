// Реализация FAT32 с поддержкой длинных имён (LFN)
#include <fat32.h>
#include <ata.h>
#include <string.h>
#include <mem.h>

/* ---------------------------------------------------------------------
 *  Глобальные переменные, доступные другим модулям
 * -------------------------------------------------------------------*/
fat32_bpb_t fat32_bpb;
uint32_t    fat_start              = 0;
uint32_t    root_dir_first_cluster = 2;
uint32_t    current_dir_cluster    = 2;

/* private */
static uint32_t cluster_begin_lba   = 0;
static uint32_t sectors_per_fat     = 0;

/* Кеш одного сектора FAT для ускорения get_next_cluster */
static uint32_t cached_fat_sector   = 0xFFFFFFFF;
static uint8_t  fat_cache[512];

// ------------------------------------------------------------------
static char toupper_ascii(char c) {
    return (c>='a' && c<='z') ? (c - ('a'-'A')) : c;
}

/* Декодировать 13 UTF-16 символов из LFN-записи в ASCII.
 * Возвращает количество добавленных символов. */
static int lfn_copy_part(char *dst, const fat32_lfn_entry_t *lfn) {
    int pos = 0;
    const uint16_t *src16;
    // name1
    src16 = lfn->name1;
    for (int i=0;i<5;i++) {
        uint8_t ch = src16[i] & 0xFF;
        if (ch==0x00 || ch==0xFF) { dst[pos]=0; return pos; }
        dst[pos++] = ch;
    }
    // name2
    src16 = lfn->name2;
    for (int i=0;i<6;i++) {
        uint8_t ch = src16[i] & 0xFF;
        if (ch==0x00 || ch==0xFF) { dst[pos]=0; return pos; }
        dst[pos++] = ch;
    }
    // name3
    src16 = lfn->name3;
    for (int i=0;i<2;i++) {
        uint8_t ch = src16[i] & 0xFF;
        if (ch==0x00 || ch==0xFF) { dst[pos]=0; return pos; }
        dst[pos++] = ch;
    }
    dst[pos]=0;
    return pos;
}

/* Сравнение ASCII строк без учёта регистра. */
static int strcasecmp_ascii(const char *a, const char *b) {
    while (*a && *b) {
        char ca = toupper_ascii(*a);
        char cb = toupper_ascii(*b);
        if (ca!=cb) return ca - cb;
        ++a; ++b;
    }
    return (*a) - (*b);
}

/* Подсчитать checksum короткого имени (для LFN). */
static uint8_t shortname_checksum(const char name[11]) {
    uint8_t sum = 0;
    for (int i=0;i<11;i++) {
        sum = ((sum>>1) | (sum<<7)) + (uint8_t)name[i];
    }
    return sum;
}

/* Преобразовать короткое имя в человекочитаемую строку (без пробелов). */
static void shortname_to_string(const char in[11], char *out) {
    int pos=0;
    /* имя */
    for (int i=0;i<8;i++) {
        if (in[i]==' ') break;
        out[pos++] = in[i];
    }
    /* расширение */
    int has_ext=0;
    for (int i=8;i<11;i++) if (in[i]!=' ') { has_ext=1; break; }
    if (has_ext) {
        out[pos++]='.';
        for (int i=8;i<11;i++) {
            if (in[i]==' ') break;
            out[pos++] = in[i];
        }
    }
    out[pos]=0;
}

/* ---------------------------------------------------------------------
 *                          НИЗКОУРОВНЕВЫЕ ФУНКЦИИ
 * -------------------------------------------------------------------*/
int fat32_mount(uint8_t drive) {
    uint8_t *sector = malloc(512);
    if (!sector) return -1;
    if (ata_read_sector(drive, 0, sector)!=0) { mfree(sector); return -2; }
    memcpy(sector, &fat32_bpb, sizeof(fat32_bpb));

    if (fat32_bpb.table_size_32==0) { mfree(sector); kprintf("fat32_mount: table_size_32 is 0\n"); return -3; }
    sectors_per_fat     = fat32_bpb.table_size_32;
    fat_start           = fat32_bpb.reserved_sector_count;
    cluster_begin_lba   = fat_start + fat32_bpb.table_count * sectors_per_fat;
    root_dir_first_cluster = fat32_bpb.root_cluster ? fat32_bpb.root_cluster : 2;
    current_dir_cluster = root_dir_first_cluster;

    mfree(sector);
    cached_fat_sector = 0xFFFFFFFF; // сброс кеша
    return 0;
}

uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    return cluster_begin_lba + (cluster-2) * fat32_bpb.sectors_per_cluster;
}

uint32_t fat32_get_next_cluster(uint8_t drive, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;               // 4 байта на запись
    uint32_t fat_sector = fat_start + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (fat_sector != cached_fat_sector) {
        if (ata_read_sector(drive, fat_sector, fat_cache)!=0)
            return 0x0FFFFFFF; // ошибка
        cached_fat_sector = fat_sector;
    }
    uint32_t next = *(uint32_t*)(&fat_cache[ent_offset]) & 0x0FFFFFFF;
    return next;
}

/* ------------------------------------------------------------------
 *            ЧТЕНИЕ КАТАЛОГА (с построением LFN)
 * ----------------------------------------------------------------*/
int fat32_list_dir(uint8_t drive, uint32_t cluster,
                   fat32_entry_t* out, int max_entries) {
    uint8_t *sector = malloc(512);
    if (!sector) return -1;
    int count = 0;

    char lfn_parts[20][14]; // до 20 частей х 13 символов = 260
    int  lfn_present = 0;

    uint32_t cl = cluster;
    while (cl < 0x0FFFFFF8) {
        for (uint8_t s=0; s<fat32_bpb.sectors_per_cluster; s++) {
            uint32_t lba = fat32_cluster_to_lba(cl)+s;
            if (ata_read_sector(drive, lba, sector)!=0) { mfree(sector); return -2; }
            for (int off=0; off<512; off+=32) {
                fat32_dir_entry_t *ent = (fat32_dir_entry_t*)&sector[off];
                if (ent->name[0]==0x00) { mfree(sector); return count; }
                if (ent->attr==0x0F) {
                    fat32_lfn_entry_t *lfn = (fat32_lfn_entry_t*)ent;
                    int ord = lfn->order & 0x1F;    // 1..N
                    if (lfn->order & 0x40) {
                        /* это начало новой цепочки LFN – очищаем буфер */
                        memset(lfn_parts, 0, sizeof(lfn_parts));
                    }
                    if (ord>0 && ord<=20) {
                        lfn_copy_part(lfn_parts[ord-1], lfn);
                        if (lfn->order & 0x40) lfn_present = ord; // последний элемент
                    }
                    continue;
                }
                if (ent->name[0]==0xE5) { lfn_present=0; continue; } // удалённая
                if ((ent->attr & 0x08)==0x08) { lfn_present=0; continue; } // volume label
                if (count>=max_entries) { mfree(sector); return count; }

                // --- заполняем выходную структуру ---
                fat32_entry_t *dst = &out[count];
                memset(dst,0,sizeof(*dst));
                if (lfn_present) {
                    /* склеиваем части в прямом порядке */
                    int pos = 0;
                    for (int i = 0; i < lfn_present; i++) {
                        int len = strlen(lfn_parts[i]);
                        memcpy(lfn_parts[i], &dst->name[pos], len);
                        pos += len;
                    }
                    dst->name[pos]=0;
                } else {
                    shortname_to_string(ent->name, dst->name);
                }
                dst->attr = ent->attr;
                dst->first_cluster = ((uint32_t)ent->first_cluster_high<<16) | ent->first_cluster_low;
                dst->size = ent->file_size;

                count++;
                lfn_present = 0; // сброс для следующего файла
            }
        }
        cl = fat32_get_next_cluster(drive, cl);
    }
    mfree(sector);
    return count;
}

/* Упрощённая обёртка для совместимости: возвращаем только короткие записи
 * (LFN игнорируются). Старый код shell.c продолжит работать, хотя длинные
 * имена будут скрыты. */
int fat32_read_dir(uint8_t drive, uint32_t cluster,
                   fat32_dir_entry_t* entries, int max_entries) {
    uint8_t *sector = malloc(512);
    if (!sector) return -1;
    int count=0;
    uint32_t cl = cluster;
    while (cl < 0x0FFFFFF8) {
        for (uint8_t s=0; s<fat32_bpb.sectors_per_cluster; s++) {
            uint32_t lba = fat32_cluster_to_lba(cl)+s;
            if (ata_read_sector(drive, lba, sector)!=0) { mfree(sector); return -2; }
            for (int off=0; off<512; off+=32) {
                fat32_dir_entry_t *ent = (fat32_dir_entry_t*)&sector[off];
                if (ent->name[0]==0x00) { mfree(sector); return count; }
                if (ent->attr==0x0F || ent->name[0]==0xE5) continue; // пропускаем LFN и удалённые
                if (count>=max_entries) { mfree(sector); return count; }
                memcpy(ent, &entries[count], sizeof(fat32_dir_entry_t));
                count++;
            }
        }
        cl = fat32_get_next_cluster(drive, cl);
    }
    mfree(sector);
    return count;
}

/* --------------------- Прочитать файл целиком -------------------------*/
int fat32_read_file(uint8_t drive, uint32_t first_cluster,
                    uint8_t* buf, uint32_t size) {
    if (first_cluster<2) return -1;
    uint32_t cluster = first_cluster;
    uint32_t total   = 0;
    uint8_t *sector  = malloc(512);
    if (!sector) return -2;

    while (cluster < 0x0FFFFFF8 && total < size) {
        for (uint8_t s=0; s<fat32_bpb.sectors_per_cluster; s++) {
            uint32_t lba = fat32_cluster_to_lba(cluster)+s;
            if (ata_read_sector(drive, lba, sector)!=0) { mfree(sector); return -3; }
            uint32_t copy = (size-total>512)?512:(size-total);
            memcpy(sector, buf+total, copy);
            total += copy;
            if (total>=size) break;
        }
        cluster = fat32_get_next_cluster(drive, cluster);
    }
    mfree(sector);
    return total;
}

/* --------------- Заглушки для записи (пока не реализованы) ------------*/
int fat32_write_file(uint8_t drive, const char* path, const uint8_t* buf, uint32_t size){ (void)drive; (void)path; (void)buf; (void)size; return -1; }
/* старый stub fat32_create_file удалён */
int fat32_write_file_data(uint8_t d,const char*p,const uint8_t*b,uint32_t s,uint32_t o){(void)d;(void)p;(void)b;(void)s;(void)o;return -1;}
int fat32_read_file_data(uint8_t d,const char*p,uint8_t*b,uint32_t s,uint32_t o){(void)d;(void)p;(void)b;(void)s;(void)o;return -1;}

/* -------------------------------------------------------------
 *          Простейшая реализация разрешения пути
 * -----------------------------------------------------------*/
int fat32_resolve_path(uint8_t drive, const char* path, uint32_t* target_cluster) {
    if (!path || !target_cluster) return -1;

    /* Абсолютный корень */
    if ((path[0]=='/' || path[0]=='\\') && path[1]=='\0') {
        *target_cluster = root_dir_first_cluster;
        return 0;
    }

    /* Текущий каталог */
    if (path[0]=='\0' || (path[0]=='.' && path[1]=='\0')) {
        *target_cluster = current_dir_cluster;
        return 0;
    }

    /* Родитель */
    if (path[0]=='.' && path[1]=='.' && path[2]=='\0') {
        fat32_entry_t list[2];
        int n = fat32_list_dir(drive, current_dir_cluster, list, 2);
        if (n<2) return -1;
        *target_cluster = list[1].first_cluster;
        return 0;
    }

    /* Ищем директорию/файл в текущем каталоге */
    fat32_entry_t list[64];
    int n = fat32_list_dir(drive, current_dir_cluster, list, 64);
    if (n<0) return -1;
    for (int i=0;i<n;i++) {
        if ((list[i].attr & 0x10)==0) continue; /* нужна только DIR */
        if (strcasecmp_ascii(list[i].name, path)==0) {
            *target_cluster = list[i].first_cluster;
            return 0;
        }
    }
    return -1; /* не найдено */
}

int fat32_change_dir(uint8_t drive, const char* path) {
    uint32_t newcl;
    if (fat32_resolve_path(drive, path, &newcl)!=0) return -1;
    if (newcl<2) return -1;
    current_dir_cluster = newcl;
    return 0;
}

/* Найти свободный кластер (значение 0 в FAT) */
static uint32_t find_free_cluster(uint8_t drive){
    for(uint32_t cl=3; cl<0x0FFFFFEF; cl++){
        if(fat32_get_next_cluster(drive, cl)==0x00000000) return cl;
    }
    return 0;
}
/* Записать значение в FAT для указанного кластера */
static int fat_write_fat_entry(uint8_t drive, uint32_t cluster, uint32_t value){
    uint32_t fat_offset = cluster*4;
    uint32_t fat_sector = fat_start + fat_offset/512;
    uint32_t ent_off    = fat_offset%512;
    uint8_t sector[512];
    if(ata_read_sector(drive, fat_sector, sector)!=0) return -1;
    *(uint32_t*)&sector[ent_off] = value & 0x0FFFFFFF;
    if(ata_write_sector(drive, fat_sector, sector)!=0) return -1;
    cached_fat_sector = 0xFFFFFFFF; /* сброс кеша */
    return 0;
}
/* Подготовить SFN из long_name (упрощённо) */
static void make_sfn(const char *longname, char sfn[11]){
    memset(sfn,' ',11);
    int len=strlen(longname); int dot=-1;
    for(int i=0;i<len;i++) if(longname[i]=='.'){dot=i;break;}
    if(dot==-1){
        for(int i=0;i<len && i<8;i++) sfn[i]=toupper_ascii(longname[i]);
    } else {
        for(int i=0;i<dot && i<8;i++) sfn[i]=toupper_ascii(longname[i]);
        for(int i=dot+1,j=8;i<len && j<11;i++,j++) sfn[j]=toupper_ascii(longname[i]);
    }
}
/* Записать последовательность LFN+SFN в каталог (один сектор, без расширения) */
static int dir_write_entries(uint8_t drive, uint32_t lba, int offset, const uint8_t *entries, int count){
    uint8_t sector[512]; if(ata_read_sector(drive,lba,sector)!=0) return -1;
    memcpy(entries, &sector[offset], count*32);
    if(ata_write_sector(drive,lba,sector)!=0) return -1;
    return 0;
}

/* Создать файл (пустой) с длинным именем в текущем каталоге */
int fat32_create_file(uint8_t drive, const char* name){
    /* Подготовка SFN */
    char sfn[11]; make_sfn(name,sfn);
    uint8_t checksum = shortname_checksum(sfn);
    int namelen=strlen(name);
    int lfn_entries = (namelen+12)/13;

    /* Собираем массив будущих записей */
    int total_entries = lfn_entries+1;
    uint8_t *buf = malloc(total_entries*32); if(!buf) return -1;
    memset(buf,0,total_entries*32);
    /* LFN – номера 1..N; запись с 0x40 (последняя) содержит начало имени */
    for(int seq=lfn_entries; seq>=1; seq--){
        int buf_idx = lfn_entries - seq;              /* положение в буфере */
        fat32_lfn_entry_t *lfn=(fat32_lfn_entry_t*)&buf[buf_idx*32];
        lfn->order = seq;
        if(seq==lfn_entries) lfn->order |= 0x40;      /* последняя часть */
        lfn->attr  = 0x0F; lfn->type=0; lfn->checksum=checksum; lfn->first_cluster_low=0;

        int start = (seq-1)*13;                       /* смещение в имени */
        for(int j=0;j<13;j++){
            uint16_t ch = 0xFFFF;
            if(start+j < namelen) ch = (uint8_t)name[start+j];
            uint16_t *dst = (j<5)? &lfn->name1[j] : (j<11)? &lfn->name2[j-5] : &lfn->name3[j-11];
            *dst = ch;
        }
    }
    /* SFN entry */
    fat32_dir_entry_t *s = (fat32_dir_entry_t*)&buf[lfn_entries*32];
    memcpy(sfn, s->name, 11);
    s->attr = 0x20; /* file */
    s->file_size=0; s->first_cluster_high=0; s->first_cluster_low=0;

    /* Найти место в текущем каталоге */
    uint32_t cl=current_dir_cluster;
    uint8_t sector[512];
    while(1){
        for(uint8_t sec=0;sec<fat32_bpb.sectors_per_cluster;sec++){
            uint32_t lba=fat32_cluster_to_lba(cl)+sec;
            if(ata_read_sector(drive,lba,sector)!=0){mfree(buf);return -1;}
            for(int off=0;off<=512-32*total_entries;off+=32){
                int free_ok=1;
                for(int e=0;e<total_entries;e++) if(sector[off+e*32]!=0x00 && sector[off+e*32]!=0xE5){free_ok=0;break;}
                if(free_ok){
                    /* пишем наши записи */
                    memcpy(buf, &sector[off], total_entries*32);
                    int end=off+total_entries*32;
                    if(end<512) sector[end]=0x00;
                    if(ata_write_sector(drive,lba,sector)!=0){mfree(buf);return -1;}
                    mfree(buf); return 0;
                }
                /* конец каталога метка 0x00 */
                if(sector[off]==0x00){
                    /* достаточно ли места? если нет, сдвигаем конец */
                    memset(&sector[off],0x00,512-off); /* clear to end*/
                    memcpy(buf, &sector[off], total_entries*32);
                    int end=off+total_entries*32;
                    if(end<512) sector[end]=0x00;
                    if(ata_write_sector(drive,lba,sector)!=0){mfree(buf);return -1;}
                    mfree(buf); return 0;
                }
            }
        }
        /* нет места, нужно расширить каталог */
        uint32_t next = fat32_get_next_cluster(drive, cl);
        if(next>=0x0FFFFFF8){ /* allocate new */
            uint32_t newcl = find_free_cluster(drive); if(!newcl){mfree(buf);return -1;}
            fat_write_fat_entry(drive, cl, newcl);
            fat_write_fat_entry(drive, newcl, 0x0FFFFFFF);
            /* zero new cluster */
            uint8_t zero[512]; memset(zero,0,512);
            for(uint8_t sct=0;sct<fat32_bpb.sectors_per_cluster;sct++) ata_write_sector(drive,fat32_cluster_to_lba(newcl)+sct,zero);
            cl=newcl;
        } else cl=next;
    }
}

int fat32_create_dir(uint8_t drive, const char* name){
    /* create directory entry first (similar to file) */
    char sfn[11]; make_sfn(name,sfn); uint8_t checksum=shortname_checksum(sfn);
    int namelen=strlen(name); int lcnt=(namelen+12)/13; int total=lcnt+1;
    uint8_t *buf = malloc(total*32); if(!buf) return -1; memset(buf,0,total*32);
    for(int seq=lcnt; seq>=1; seq--){
        int buf_idx = lcnt-seq;
        fat32_lfn_entry_t *lfn=(fat32_lfn_entry_t*)&buf[buf_idx*32];
        lfn->order = seq;
        if(seq==lcnt) lfn->order |= 0x40;
        lfn->attr=0x0F; lfn->type=0; lfn->checksum=checksum; lfn->first_cluster_low=0;
        int start=(seq-1)*13;
        for(int j=0;j<13;j++){
            uint16_t ch=0xFFFF; if(start+j<namelen) ch=(uint8_t)name[start+j];
            uint16_t *dst=(j<5)?&lfn->name1[j]:(j<11)?&lfn->name2[j-5]:&lfn->name3[j-11];
            *dst=ch;
        }
    }
    fat32_dir_entry_t *d=(fat32_dir_entry_t*)&buf[lcnt*32];
    memcpy(sfn, d->name, 11); d->attr=0x10; /* dir */
    uint32_t newcl=find_free_cluster(drive); if(!newcl){mfree(buf);return -1;}
    d->first_cluster_high=newcl>>16; d->first_cluster_low=newcl&0xFFFF; d->file_size=0;
    /* mark cluster as end */
    fat_write_fat_entry(drive, newcl, 0x0FFFFFFF);

    /* write dir entry into current directory */
    uint32_t cl=current_dir_cluster; uint8_t sector[512];
    while(1){
        for(uint8_t sct=0;sct<fat32_bpb.sectors_per_cluster;sct++){
            uint32_t lba=fat32_cluster_to_lba(cl)+sct;
            if(ata_read_sector(drive,lba,sector)!=0){mfree(buf);return -1;}
            for(int off=0;off<=512-32*total;off+=32){
                int free_ok=1; for(int e=0;e<total;e++) if(sector[off+32*e]!=0x00 && sector[off+32*e]!=0xE5){free_ok=0;break;}
                if(free_ok){
                    memcpy(buf, &sector[off], total*32);
                    int end=off+total*32;
                    if(end<512) sector[end]=0x00;
                    if(ata_write_sector(drive,lba,sector)!=0){mfree(buf);return -1;}
                    mfree(buf);
                    /* init new directory cluster with '.' and '..' */
                    uint8_t dirsec[512]; memset(dirsec,0,512);
                    /* . */
                    memset(dirsec,' ',11); dirsec[0]='.'; dirsec[11]=0x10;
                    *(uint16_t*)(&dirsec[20])=(newcl>>16)&0xFFFF; *(uint16_t*)(&dirsec[26])=newcl&0xFFFF;
                    /* .. */
                    memset(&dirsec[32],' ',11); dirsec[32]='.'; dirsec[33]='.'; dirsec[43]=0x10;
                    *(uint16_t*)(&dirsec[52])=(current_dir_cluster>>16)&0xFFFF; *(uint16_t*)(&dirsec[58])=current_dir_cluster&0xFFFF;
                    for(uint8_t sc=0;sc<fat32_bpb.sectors_per_cluster;sc++) ata_write_sector(drive,fat32_cluster_to_lba(newcl)+sc,dirsec);
                return 0;
                }
                if(sector[off]==0x00){
                    memset(&sector[off],0,512-off);
                    memcpy(buf, &sector[off], total*32);
                    int end=off+total*32;
                    if(end<512) sector[end]=0x00;
                    ata_write_sector(drive,lba,sector);
                    mfree(buf);
                    /* init new dir cluster same as above */
                    uint8_t dirsec[512]; memset(dirsec,0,512);
                    memset(dirsec,' ',11); dirsec[0]='.'; dirsec[11]=0x10;
                    *(uint16_t*)(&dirsec[20])=(newcl>>16)&0xFFFF; *(uint16_t*)(&dirsec[26])=newcl&0xFFFF;
                    memset(&dirsec[32],' ',11); dirsec[32]='.'; dirsec[33]='.'; dirsec[43]=0x10;
                    *(uint16_t*)(&dirsec[52])=(current_dir_cluster>>16)&0xFFFF; *(uint16_t*)(&dirsec[58])=current_dir_cluster&0xFFFF;
                    for(uint8_t sc=0;sc<fat32_bpb.sectors_per_cluster;sc++) ata_write_sector(drive,fat32_cluster_to_lba(newcl)+sc,dirsec);
                    return 0;
                }
            }
        }
        uint32_t next=fat32_get_next_cluster(drive,cl);
        if(next>=0x0FFFFFF8){uint32_t newc=find_free_cluster(drive); if(!newc){mfree(buf);return -1;} fat_write_fat_entry(drive,cl,newc); fat_write_fat_entry(drive,newc,0x0FFFFFFF); uint8_t zero[512]; memset(zero,0,512); for(uint8_t s=0;s<fat32_bpb.sectors_per_cluster;s++) ata_write_sector(drive,fat32_cluster_to_lba(newc)+s,zero); cl=newc;}
        else cl=next;
    }
}