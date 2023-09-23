#pragma once

#include "types.h"
#include "stdio.h"

typedef struct TrueTypeTable
{
	char tag[4];
	u32 checksum;
	u32 offset;
	u32 length;
} TrueTypeTable;

typedef struct TrueTypeFontDirectory
{
	//Begin Offset Subtable
	u32 scaler;
	u16 num_tables;
	u16 search_range;
	u16 entry_selector;
	u16 range_shift;
	//End Offset Subtable

	//Begin table Directory
	TrueTypeTable* table_directory;
	//End Table Directory
	
} TrueTypeFontDirectory;

typedef struct TrueTypeFont
{
	
} TrueTypeFont;

bool truetype_read_file(const char* filename, size_t* out_file_size, u8** out_data) {
	
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

u8 read_u8(void* src, size_t offset)
{
	u8 num = *(u8*)(src + offset);
	return num;
}

//FCS TODO: Big-Endian systems shouldn't byte swap
u16 read_u16(void* src, size_t offset)
{	
	u16 num = *(u16*)(src + offset);
	return (num>>8) | (num<<8);
}

i16 read_i16(void* src, size_t offset)
{	
	i16 num = *(i16*)(src + offset);
	return (num>>8) | (num<<8);
}

u32 read_u32(void* src, size_t offset)
{
	u32 num = *(u32*)(src + offset);	
	return  ((num>>24)&0xff) 	| 	// move byte 3 to byte 0
			((num<<8)&0xff0000) | 	// move byte 1 to byte 2
			((num>>8)&0xff00) 	| 	// move byte 2 to byte 1
			((num<<24)&0xff000000); // byte 0 to byte 3
}


i32 read_i32(void* src, size_t offset)
{
	i32 num = *(i32*)(src + offset);	
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
	u8* file_data = NULL;
	if (truetype_read_file(filename, &file_size, &file_data))
	{
		printf("Font File Size: %zu\n", file_size);

		u8* font_directory_start = file_data;

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

		u8* table_directory_start = font_directory_start + 12;
		
		font_directory.table_directory = calloc(font_directory.num_tables, sizeof(TrueTypeTable));
		for (i32 table_index = 0; table_index < font_directory.num_tables; ++table_index)
		{
			u8* current_table_start = table_directory_start + (16 * table_index); 
			TrueTypeTable* current_table = &font_directory.table_directory[table_index];
			
			for (i32 i = 3; i >=0; --i)
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
