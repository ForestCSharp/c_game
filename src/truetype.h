#pragma once

#include "stdint.h"
#include "stdio.h"

typedef struct TrueTypeTable
{
	char tag[4];
	uint32_t checksum;
	uint32_t offset;
	uint32_t length;
} TrueTypeTable;

typedef struct TrueTypeFontDirectory
{
	//Begin Offset Subtable
	uint32_t scaler;
	uint16_t num_tables;
	uint16_t search_range;
	uint16_t entry_selector;
	uint16_t range_shift;
	//End Offset Subtable

	//Begin table Directory
	TrueTypeTable* table_directory;
	//End Table Directory
	
} TrueTypeFontDirectory;

typedef struct TrueTypeFont
{
	
} TrueTypeFont;

bool truetype_read_file(const char* filename, size_t* out_file_size, uint8_t** out_data) {
	
	FILE *file = fopen (filename, "rb");
	if(!file) return false;

	fseek(file, 0L, SEEK_END);
	const size_t file_size = *out_file_size = ftell(file);
	rewind(file);

	*out_data = calloc(1, file_size+1);
	if (!*out_data) {
		fclose(file);
		return false;
	}

	if (fread(*out_data, 1, file_size, file) != file_size) {
		fclose(file);
		free(*out_data);
		return false;
	}

	fclose(file);
	return true;
}

uint8_t read_u8(void* src, size_t offset)
{
	uint8_t num = *(uint8_t*)(src + offset);
	return num;
}

//FCS TODO: Big-Endian systems shouldn't byte swap
uint16_t read_u16(void* src, size_t offset)
{	
	uint16_t num = *(uint16_t*)(src + offset);
	return (num>>8) | (num<<8);
}

int16_t read_i16(void* src, size_t offset)
{	
	int16_t num = *(int16_t*)(src + offset);
	return (num>>8) | (num<<8);
}

uint32_t read_u32(void* src, size_t offset)
{
	uint32_t num = *(uint32_t*)(src + offset);	
	return  ((num>>24)&0xff) 	| 	// move byte 3 to byte 0
			((num<<8)&0xff0000) | 	// move byte 1 to byte 2
			((num>>8)&0xff00) 	| 	// move byte 2 to byte 1
			((num<<24)&0xff000000); // byte 0 to byte 3
}


int32_t read_i32(void* src, size_t offset)
{
	int32_t num = *(int32_t*)(src + offset);	
	return  ((num>>24)&0xff) 	| 	// move byte 3 to byte 0
			((num<<8)&0xff0000) | 	// move byte 1 to byte 2
			((num>>8)&0xff00) 	| 	// move byte 2 to byte 1
			((num<<24)&0xff000000); // byte 0 to byte 3
}

void read_bytes(void* dst, void* src, size_t offset, size_t num_bytes)
{
	memcpy(dst, src + offset, num_bytes);
}

bool truetype_load_file(const char* filename, TrueTypeFont* out_font)
{
	size_t file_size = 0;
	uint8_t* file_data = NULL;
	if (truetype_read_file(filename, &file_size, &file_data))
	{
		printf("Font File Size: %zu\n", file_size);

		uint8_t* font_directory_start = file_data;

		TrueTypeFontDirectory font_directory = {};
		font_directory.scaler = read_u32(font_directory_start, 0); 
		font_directory.num_tables = read_u16(font_directory_start, 4);
		font_directory.search_range = read_u16(font_directory_start, 6);
		font_directory.entry_selector = read_u16(font_directory_start, 8);
		font_directory.range_shift = read_u16(font_directory_start, 10);

		printf("Offset Subtable\n");
		printf("\tScaler: %u\n", font_directory.scaler);
		printf("\tNumTables: %u\n", font_directory.num_tables);
		printf("\tSearch Range: %u\n", font_directory.search_range);
		printf("\tEntry Selector: %u\n", font_directory.entry_selector);
		printf("\tRange Shift: %u\n", font_directory.range_shift);

		uint8_t* table_directory_start = font_directory_start + 12;
		
		font_directory.table_directory = calloc(font_directory.num_tables, sizeof(TrueTypeTable));
		for (int32_t table_index = 0; table_index < font_directory.num_tables; ++table_index)
		{
			uint8_t* current_table_start = table_directory_start + (16 * table_index); 
			TrueTypeTable* current_table = &font_directory.table_directory[table_index];
			
			for (int32_t i = 3; i >=0; --i)
			{
				current_table->tag[i] = read_u8(current_table_start, i);
			}
			current_table->checksum = read_u32(current_table_start,  4);
			current_table->offset = read_u32(current_table_start, 8);
			current_table->length = read_u32(current_table_start, 12);
	
			printf("Table %i:", table_index);
			printf("\ttag: %.4s", (char*)&current_table->tag);
			printf("\toffset: %u", current_table->offset);
			printf("\tlength: %u\n", current_table->length);
		}

		return true;
	}
	
	return false;
}

void truetype_free_file(TrueTypeFont* in_font)
{

}
