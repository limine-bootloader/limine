#include <stdint.h>

// Some defines.
#define NULL 0
#define VGA_ADDRESS 0xb8000

// This indicates where the char should be placed.
uint16_t vga_cursor_index;

// This is a pointer to the VGA Screen buffer memory location.
uint16_t *vga_buffer;

// This function will make the kernel ready to print things.
// Basically, this sets some variables and the memory location
// of VGA's screen buffer, and clear the screen as well.
void initialize_vga()
{
	vga_cursor_index = 0;
	vga_buffer = (uint16_t *) VGA_ADDRESS;
	
	// Clear the screen.
	for (int i = 0; i < 2200; i++)
	{
		vga_buffer[i] = 0x0000;			// NULL char and black foreground & background
	}
}

// This returns a valid value that can be placed on the vga screen buffer to
// indicate the color of a char.
inline uint8_t vga_entry_color(uint8_t foreground, uint8_t background)
{
	return foreground | background << 4;
}

// This returns a valid value that can be placed on the vga screen buffer to
// display a char.
inline uint16_t vga_entry(const char ch, uint8_t color)
{
	return (uint16_t) ch | (uint16_t) color << 8;
}

// Prints a string to the screen.
void print_string(const char *string)
{
	uint8_t color;
	for (int i = 0; string[i] != '\0'; i++)
	{
		color = vga_entry_color(7, 0); // 7 = light gray, 0 = black
		vga_buffer[vga_cursor_index] = vga_entry(string[i], color);
		vga_cursor_index++;
	}
}

// Kernel main
// The assembly code will redirect us here.
void kmain()
{
	initialize_vga();
	print_string("Hello kernel!");
	asm volatile("cli\nhlt");
}
