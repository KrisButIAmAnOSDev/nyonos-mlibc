ENTRY(_start)

PHDRS
{
    text   PT_LOAD FILEHDR PHDRS FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x4fff0000;
    __ehdr_start = . - 0x1000;

    .text : ALIGN(4096) {
        *(.text.start)
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
        *(.eh_frame)
    } :rodata

    .data : ALIGN(4096) {
        *(.data .data.*)
        *(.got .got.plt)
    } :data

    .tdata : ALIGN(4096) {
        *(.tdata .tdata.*)
    } :data

    .tbss : ALIGN(4096) {
        *(.tbss .tbss.*)
    } :data

    .bss : ALIGN(4096) {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    .preinit_array : ALIGN(8) {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } :data

    .init_array : ALIGN(8) {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data

    .fini_array : ALIGN(8) {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } :data

    /DISCARD/ : {
        *(.note .note.*)
        *(.comment)
        *(.eh_frame_hdr)
    }
}
