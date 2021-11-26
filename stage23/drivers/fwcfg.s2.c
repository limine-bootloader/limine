#include <stdint.h>
#include <lib/libc.h>
#include <lib/print.h>
#include <mm/pmm.h>
#include <lib/blib.h>
#include <sys/cpu.h>

struct dma_descr {
    uint32_t control;
    uint32_t length;
    uint64_t address;
};

struct fw_cfg_file {
	uint32_t size;
	uint16_t select;
	uint16_t reserved;
	char name[56];
};
struct fw_cfg_files {
    uint32_t count;
    struct fw_cfg_file f[];
};

static uint16_t bswap16(uint16_t value) {
	uint8_t* value_ptr = (uint8_t*)&value;
	return value_ptr[0]<<8|value_ptr[1];
}

static uint32_t bswap32(uint32_t value) {
	uint8_t* value_ptr = (uint8_t*)&value;
	return value_ptr[0]<<24|value_ptr[1]<<16|value_ptr[2]<<8|value_ptr[3];
}

static void fwcfg_disp_read(uint16_t sel, uint32_t outsz, uint8_t* outbuf) {
	outw(0x510, sel);
	for (uint32_t i = 0;i < outsz;i++) outbuf[i] = inb(0x511);
}

static struct fw_cfg_files* filebuf = NULL;
static const char* simple_mode_config = 
	"TIMEOUT=0\n"
	":simple mode config\n"
	"KERNEL_PATH=fwcfg:///opt/org.limine-bootloader.kernel";

static const char* simple_mode_bg_config = 
	"TIMEOUT=0\n"
	"GRAPHICS=yes\n"
	"THEME_BACKGROUND=50000000\n"
	"BACKGROUND_PATH=fwcfg:///opt/org.limine-bootloader.background\n"
	"BACKGROUND_STYLE=stretched\n"
	":simple mode config\n"
	"KERNEL_PATH=fwcfg:///opt/org.limine-bootloader.kernel";

static bool simple_mode = false;

bool fwcfg_open(struct file_handle *handle, const char *name) {	
	char sig[5] = { 0 };
	fwcfg_disp_read(/* signature */ 0x0000, 4, (uint8_t*)sig);
	if (strcmp(sig, "QEMU")) return false;
	
	uint32_t count;
	fwcfg_disp_read(0x0019, 4, (uint8_t*)&count);
	count = bswap32(count);
	
	if (!filebuf) {
		filebuf = (struct fw_cfg_files*)ext_mem_alloc(count * 64 + 4);
		fwcfg_disp_read(0x0019, count * 64 + 4, (uint8_t*)filebuf);
	}

	bool has_kernel = false, has_background = false;
	for (uint32_t i = 0;i < count;i++) {
		if (!strncmp(filebuf->f[i].name, name, 56)) {
			uint16_t sel = bswap16(filebuf->f[i].select);
    		handle->size = bswap32(filebuf->f[i].size);
   	 		handle->is_memfile = true;
			uint8_t* buf = (uint8_t*)(handle->fd = ext_mem_alloc(handle->size));
			fwcfg_disp_read(sel, handle->size, buf);
			return true;
		}
		if (!strncmp(filebuf->f[i].name, "opt/org.limine-bootloader.background", 56)) {
			has_background = true;
		}
		if (!strncmp(filebuf->f[i].name, "opt/org.limine-bootloader.kernel", 56)) {
			has_kernel = true;
		}
	}
	
	if (has_kernel && !strcmp(name, "opt/org.limine-bootloader.config")) {
    	const char* conf = has_background ? simple_mode_bg_config : simple_mode_config;
		handle->size = strlen(conf);
   	 	handle->is_memfile = true;
		char* buf = (char*)(handle->fd = ext_mem_alloc(handle->size + 1));
		strcpy(buf, conf);
		simple_mode = true;
		return true;
	}

	return false;
}
