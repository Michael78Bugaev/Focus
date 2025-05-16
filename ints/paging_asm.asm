global loadPageDirectory
global enablePaging

; void loadPageDirectory(uint32_t* dir)
loadPageDirectory:
    mov eax, [esp + 4]
    mov cr3, eax
    ret

; void enablePaging()
enablePaging:
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret