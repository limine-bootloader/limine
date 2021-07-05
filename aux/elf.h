// This file is taken straight from the mlibc project.
// <https://github.com/managarm/mlibc>

#ifndef _ELF_H
#define _ELF_H

#include <stdint.h>

// TODO: Convert the enums to #defines so that they work with #ifdef.

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFOSABI_SYSV 0
#define EM_X86_64 62

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOBITS 8
#define SHT_DYNSYM 11

#define SHF_WRITE 1
#define SHF_ALLOC 2
#define SHF_EXECINSTR 4

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;
typedef uint64_t Elf32_Xword;
typedef int64_t Elf32_Sxword;

#define EI_NIDENT (16)

typedef struct {
	unsigned char e_ident[EI_NIDENT]; /* ELF identification */
	Elf32_Half e_type; /* Object file type */
	Elf32_Half e_machine; /* Machine type */
	Elf32_Word e_version; /* Object file version */
	Elf32_Addr e_entry; /* Entry point address */
	Elf32_Off e_phoff; /* Program header offset */
	Elf32_Off e_shoff; /* Section header offset */
	Elf32_Word e_flags; /* Processor-specific flags */
	Elf32_Half e_ehsize; /* ELF header size */
	Elf32_Half e_phentsize; /* Size of program header entry */
	Elf32_Half e_phnum; /* Number of program header entries */
	Elf32_Half e_shentsize; /* Size of section header entry */
	Elf32_Half e_shnum; /* Number of section header entries */
	Elf32_Half e_shstrndx; /* Section name string table index */
} Elf32_Ehdr;

typedef struct {
	unsigned char e_ident[EI_NIDENT]; /* ELF identification */
	Elf64_Half e_type; /* Object file type */
	Elf64_Half e_machine; /* Machine type */
	Elf64_Word e_version; /* Object file version */
	Elf64_Addr e_entry; /* Entry point address */
	Elf64_Off e_phoff; /* Program header offset */
	Elf64_Off e_shoff; /* Section header offset */
	Elf64_Word e_flags; /* Processor-specific flags */
	Elf64_Half e_ehsize; /* ELF header size */
	Elf64_Half e_phentsize; /* Size of program header entry */
	Elf64_Half e_phnum; /* Number of program header entries */
	Elf64_Half e_shentsize; /* Size of section header entry */
	Elf64_Half e_shnum; /* Number of section header entries */
	Elf64_Half e_shstrndx; /* Section name string table index */
} Elf64_Ehdr;

enum {
	ET_NONE = 0,
	ET_REL = 1,
	ET_EXEC = 2,
	ET_DYN = 3,
	ET_CORE = 4,
};

enum {
	SHN_UNDEF = 0,
	SHN_ABS = 0xFFF1
};

typedef struct {
	Elf64_Word st_name;
	unsigned char st_info;
	unsigned char st_other;
	Elf64_Half st_shndx;
	Elf64_Addr st_value;
	Elf64_Xword st_size;
} Elf64_Sym ;

extern inline unsigned char ELF64_ST_BIND(unsigned char info) {
	return info >> 4;
}
extern inline unsigned char ELF64_ST_TYPE(unsigned char info) {
	return info & 0x0F;
}
extern inline unsigned char ELF64_ST_INFO(unsigned char bind, unsigned char type) {
	return (bind << 4) | type;
}

enum {
	STB_GLOBAL = 1,
	STB_WEAK = 2,
	STB_GNU_UNIQUE = 10
};

enum {
	STT_OBJECT = 1,
	STT_FUNC = 2,
	STT_TLS = 6
};

enum {
	R_X86_64_NONE = 0,
	R_X86_64_64 = 1,
	R_X86_64_COPY = 5,
	R_X86_64_GLOB_DAT = 6,
	R_X86_64_JUMP_SLOT = 7,
	R_X86_64_RELATIVE = 8,
	R_X86_64_DTPMOD64 = 16,
	R_X86_64_DTPOFF64 = 17,
	R_X86_64_TPOFF64 = 18,
};

enum {
	R_AARCH64_ABS64 = 257,
	R_AARCH64_COPY = 1024,
	R_AARCH64_GLOB_DAT = 1025,
	R_AARCH64_JUMP_SLOT = 1026,
	R_AARCH64_RELATIVE = 1027,
	R_AARCH64_TLSDESC = 1031
};

typedef struct {
	Elf64_Addr r_offset;
	uint64_t   r_info;
} Elf64_Rel;

typedef struct {
	Elf64_Addr r_offset;
	Elf64_Xword r_info;
	Elf64_Sxword r_addend;
} Elf64_Rela;

static inline Elf64_Xword ELF64_R_SYM(Elf64_Xword info) {
	return info >> 32;
}
static inline Elf64_Xword ELF64_R_TYPE(Elf64_Xword info) {
	return info & 0xFFFFFFFF;
}

enum {
	PT_LOAD = 1,
	PT_DYNAMIC = 2,
	PT_INTERP = 3,
	PT_NOTE = 4,
	PT_PHDR = 6,
	PT_TLS = 7,
	PT_GNU_EH_FRAME = 0x6474E550,
	PT_GNU_STACK = 0x6474E551,
	PT_GNU_RELRO = 0x6474E552
};

enum {
	PF_X = 1,
	PF_W = 2,
	PF_R = 4
};

typedef struct {
	Elf64_Word p_type; /* Type of segment */
	Elf64_Word p_flags; /* Segment attributes */
	Elf64_Off p_offset; /* Offset in file */
	Elf64_Addr p_vaddr; /* Virtual address in memory */
	Elf64_Addr p_paddr; /* Reserved */
	Elf64_Xword p_filesz; /* Size of segment in file */
	Elf64_Xword p_memsz; /* Size of segment in memory */
	Elf64_Xword p_align; /* Alignment of segment */
} Elf64_Phdr;

enum {
	DT_NULL = 0,
	DT_NEEDED = 1,
	DT_PLTRELSZ = 2,
	DT_PLTGOT = 3,
	DT_HASH = 4,
	DT_STRTAB = 5,
	DT_SYMTAB = 6,
	DT_RELA = 7,
	DT_RELASZ = 8,
	DT_RELAENT = 9,
	DT_STRSZ = 10,
	DT_SYMENT = 11,
	DT_INIT = 12,
	DT_FINI = 13,
	DT_SONAME = 14,
	DT_RPATH = 15,
	DT_SYMBOLIC = 16,
	DT_REL = 17,
	DT_BIND_NOW = 24,
	DT_INIT_ARRAY = 25,
	DT_FINI_ARRAY = 26,
	DT_INIT_ARRAYSZ = 27,
	DT_FINI_ARRAYSZ = 28,
	DT_RUNPATH = 29,
	DT_PLTREL = 20,
	DT_DEBUG = 21,
	DT_JMPREL = 23,
	DT_FLAGS = 30,
	DT_GNU_HASH = 0x6ffffef5,
	DT_TLSDESC_PLT = 0x6ffffef6,
	DT_TLSDESC_GOT = 0x6ffffef7,
	DT_VERSYM = 0x6ffffff0,
	DT_RELACOUNT = 0x6ffffff9,
	DT_FLAGS_1 = 0x6ffffffb,
	DT_VERDEF = 0x6ffffffc,
	DT_VERDEFNUM = 0x6ffffffd,
	DT_VERNEED = 0x6ffffffe,
	DT_VERNEEDNUM = 0x6fffffff
};

enum {
	// For DT_FLAGS.
	DF_SYMBOLIC = 0x02,
	DF_STATIC_TLS = 0x10,

	// For DT_FLAGS_1.
	DF_1_NOW = 0x00000001
};

typedef struct {
	Elf32_Sword d_tag;
	union {
		Elf32_Word d_val;
		Elf32_Addr d_ptr;
	} d_un;
} Elf32_Dyn;

typedef struct {
	Elf64_Sxword d_tag;
	union {
		Elf64_Xword d_val;
		Elf64_Addr d_ptr;
	} d_un;
} Elf64_Dyn;

typedef struct {
	Elf32_Word sh_name;
	Elf32_Word sh_type;
	Elf32_Word sh_flags;
	Elf32_Addr sh_addr;
	Elf32_Off sh_offset;
	Elf32_Word sh_size;
	Elf32_Word sh_link;
	Elf32_Word sh_info;
	Elf32_Word sh_addralign;
	Elf32_Word sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf64_Word sh_name;
	Elf64_Word sh_type;
	Elf64_Xword sh_flags;
	Elf64_Addr sh_addr;
	Elf64_Off sh_offset;
	Elf64_Xword sh_size;
	Elf64_Word sh_link;
	Elf64_Word sh_info;
	Elf64_Xword sh_addralign;
	Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
  Elf32_Word n_namesz;
  Elf32_Word n_descsz;
  Elf32_Word n_type;
} Elf32_Nhdr;

typedef struct {
  Elf64_Word n_namesz;
  Elf64_Word n_descsz;
  Elf64_Word n_type;
} Elf64_Nhdr;

/* ST_TYPE (subfield of st_info) values (symbol type) */
#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION	3
#define STT_FILE	4

/* ST_BIND (subfield of st_info) values (symbol binding) */
#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2

/* sh_type (section type) values */
#define SHT_NULL		0
#define SHT_PROGBITS		1
#define SHT_SYMTAB		2
#define SHT_STRTAB		3
#define SHT_RELA		4
#define SHT_NOBITS		8
#define SHT_REL			9
#define SHT_INIT_ARRAY		14
#define SHT_FINI_ARRAY		15
#define SHT_SYMTAB_SHNDX	18

/* special section indices */
#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_COMMON	0xfff2
#define SHN_XINDEX	0xffff
#define SHN_HIRESERVE	0xff00

/* values for e_machine */
#define EM_NONE		0
#define EM_SPARC	2
#define EM_386		3
#define EM_PPC		20
#define EM_PPC64	21
#define EM_X86_64	62

/* e_indent constants */
#define EI_MAG0		0
#define ELFMAG0		0x7f

#define EI_MAG1		1
#define ELFMAG1		'E'

#define EI_MAG2		2
#define ELFMAG2		'L'

#define EI_MAG3		3
#define ELFMAG3		'F'

#define EI_CLASS	4
#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2
#define ELFCLASSNUM	3

#define EI_DATA		5
#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2
#define ELFDATANUM	3

#endif // _ELF_H
