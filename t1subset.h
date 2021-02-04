#pragma once
#include <cstdio>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include "t1-encoding.h"

using namespace std;

#define MAX_NAME_LEN    127
#define MAX_LINE_BUFFER 512
#define MAX_GLYPH_COUNT 256

typedef unsigned char byte_t;

const unsigned short key_eexec = 55665, key_charstring = 4330;
const uint16_t C1 = 52845, C2 = 22719;

class t1subset
{
	char m_line_buffer[MAX_LINE_BUFFER + 1]{ 0 };
	FILE* m_input_file{ nullptr };
	FILE* m_output_file{ nullptr };
	bool m_char_subset[MAX_GLYPH_COUNT]{ false };
	int32_t m_bin_data_offset{ 0 };
	vector<char> m_bin_data;
	byte_t m_discard_bytes[4]{ 0 };
	int32_t m_lenIV{ 0 };
	vector<string> m_glyph_list;

	void clear()
	{
		if (m_input_file)
		{
			fclose(m_input_file);
		}
		if (m_output_file && m_output_file != stdout)
		{
			fclose(m_output_file);
		}
		m_input_file = m_output_file = nullptr;
	}
	void file_size_check()
	{
		long curpos, file_size, binary_data_offset = 0;

		fread(&binary_data_offset, sizeof(binary_data_offset), 1, m_input_file);

		curpos = ftell(m_input_file);

		fseek(m_input_file, 0, SEEK_END);

		file_size = ftell(m_input_file);

		if (file_size <= 512) // trailer size is at least 512 bytes
		{
			throw runtime_error("File is not a valid '.pfb' file. File size can't be less than 512 bytes");
		}
		else if (binary_data_offset >= file_size)
		{
			throw runtime_error("File is not a valid '.pfb' file. Binary data missing");
		}
		fseek(m_input_file, curpos, SEEK_SET);
	}
	void check_file_type()
	{
		byte_t signature[6] = { 0 };
		char buf[20]{ 0 };

		fread(signature, 2, 1, m_input_file);

		if (128 == signature[0] && 1 == signature[1])
		{
			// check the file size to ensure we have enough data and to avoid checking for EOF all over the place

			file_size_check();

			// write a temporary signature

			fwrite(signature, sizeof(signature), 1, m_output_file);
		}
		else
		{
			throw runtime_error("File is not a '.pfb' file");
		}

		fgets(m_line_buffer, MAX_LINE_BUFFER, m_input_file);

		if ('%' == *m_line_buffer)
		{
			const char sgn1[] = "%!PS-AdobeFont";
			const char sgn2[] = "%!FontType1";

			if ((strncmp(m_line_buffer, sgn1, sizeof(sgn1) - 1) == 0) || (strncmp(m_line_buffer, sgn2, sizeof(sgn2) - 1) == 0))
			{
				fputs(m_line_buffer, m_output_file);
			}
			else
			{
				throw runtime_error("Unknown file type");
			}
		}
		else
		{
			throw runtime_error("File is not a '.pfb' file");
		}
	}
	void load_file(const char* font_name)
	{
		fopen_s(&m_input_file, font_name, "rb");

		if (!m_input_file)
		{
			throw runtime_error("Unable to open input font file");
		}
	}
	void create_output_file(const char* output_filename)
	{
		if (!output_filename)
		{
			m_output_file = stdout;
		}
		else
		{
			fopen_s(&m_output_file, output_filename, "wb");

			if (!m_output_file)
			{
				throw runtime_error("Unable to create the output file");
			}
		}
	}
	void precondition(const char* font_name, const byte_t* char_subset, byte_t char_subset_count, const char* output_filename)
	{
		if (!font_name)
		{
			throw runtime_error("Please indicate the filename of the input font");
		}
		else if (!char_subset)
		{
			throw runtime_error("Please indicate the characters/glyphs to subset");
		}

		load_file(font_name);

		create_output_file(output_filename);

		check_file_type();

		for (short i = 0; i < char_subset_count; ++i)
		{
			byte_t ch = char_subset[i];

			m_char_subset[ch] = true;
		}
	}
	bool read_char(byte_t& ch)
	{
		int c = fgetc(m_input_file);

		ch = (byte_t)c;

		return c != EOF;
	}
	void write_comment()
	{
		fputc('%', m_output_file);

		if (fgets(m_line_buffer, MAX_LINE_BUFFER, m_input_file))
		{
			fputs(m_line_buffer, m_output_file);
		}
	}
	bool read_name(char* name, short len)
	{
		return fscanf_s(m_input_file, "%s", name, len) == 1;
	}
	void find_encoding()
	{
		byte_t ch;

		while (read_char(ch))
		{
			if ('%' == ch)
			{
				write_comment();
			}
			else if ('/' == ch)
			{
				m_line_buffer[0] = ch;

				read_name(&m_line_buffer[1], MAX_NAME_LEN);

				fputs(m_line_buffer, m_output_file);

				if (strcmp(m_line_buffer, "/Encoding") == 0)
				{
					fputc(' ', m_output_file);

					return;
				}
			}
			else
			{
				fputc(ch, m_output_file);
			}
		}

		throw runtime_error("Unable to find the /Encoding part");
	}
	void read_encoding_name(const char* name)
	{
		const char** enc = nullptr;

		if (strcmp(name, "StandardEncoding") == 0)
		{
			enc = StandardEncoding;
		}
		else if (strcmp(name, "WinAnsiEncoding") == 0)
		{
			enc = WinAnsiEncoding;
		}
		else if (strcmp(name, "MacRomanEncoding") == 0)
		{
			enc = MacRomanEncoding;
		}
		else
		{
			// m_line_buffer == name; don't use it

			char msg[MAX_LINE_BUFFER];

			sprintf_s(msg, MAX_LINE_BUFFER, "Unsupported encoding: %s", name);

			throw runtime_error(msg);
		}

		// skip the 'def' after the encoding name

		read_name(m_line_buffer, MAX_NAME_LEN);

		fputs("256 array\n 0 1 255 { 1 index exch / .notdef put} for\n", m_output_file);

		if (enc)
		{
			string name(128, ' ');

			name[0] = '/';

			for (uint16_t i = 0; i < MAX_GLYPH_COUNT; ++i)
			{
				if (m_char_subset[i] && enc[i])
				{
					sprintf_s(m_line_buffer, sizeof(m_line_buffer), "dup %u /%s put\n", i, enc[i]);

					fputs(m_line_buffer, m_output_file);

					name.replace(1, string::npos, enc[i]);

					m_glyph_list.push_back(name);
				}
			}
		}

		fputs("readonly def\n", m_output_file);
	}
	void find_for_operator()
	{
		byte_t ch;

		while (read_char(ch))
		{
			// find 'for'
			//" 256 array\n 0 1 255 { 1 index exch / .notdef put} for"
			if ('}' == ch)
			{
				fputc(ch, m_output_file);

				read_name(m_line_buffer, MAX_NAME_LEN);

				if (strcmp(m_line_buffer, "for") == 0)
				{
					fputs(" for\n", m_output_file);

					return;
				}
			}
			// unlikely
			else if ('%' == ch)
			{
				write_comment();
			}
			else
			{
				fputc(ch, m_output_file);
			}
		}
		throw runtime_error("Unable to find the 'for' operator");
	}
	void read_encoding_table()
	{
		find_for_operator();

		while (read_name(m_line_buffer, MAX_NAME_LEN))
		{
			if (strcmp(m_line_buffer, "dup") == 0)
			{
				char glyph_name[MAX_NAME_LEN + 1]{ 0 };
				char opr_name[MAX_NAME_LEN + 1]{ 0 };
				int index;

				if (fscanf_s(m_input_file, " %d %s %s", &index, glyph_name, MAX_NAME_LEN, opr_name, MAX_NAME_LEN) == 3)
				{
					if (index < 0 || index > 255)
					{
						sprintf_s(m_line_buffer, MAX_LINE_BUFFER, "Index out of range: %d", index);

						throw runtime_error(m_line_buffer);
					}
					else if (glyph_name[0] != '/')
					{
						sprintf_s(m_line_buffer, MAX_LINE_BUFFER, "Glyph name must begin with '/': %s", glyph_name);

						throw runtime_error(m_line_buffer);
					}
					else if (strcmp(opr_name, "put") != 0)
					{
						sprintf_s(m_line_buffer, MAX_LINE_BUFFER, "Operator 'put' expected after the glyph name '%s'; operator found: '%s'", glyph_name, opr_name);

						throw runtime_error(m_line_buffer);
					}

					if (m_char_subset[index])
					{
						fprintf(m_output_file, "dup %d %s put\n", index, glyph_name);

						m_glyph_list.push_back(string(glyph_name));
					}
				}
				else
				{
					throw runtime_error("Expected to read a glyph index and its name here");
				}
			}
			else if (strcmp(m_line_buffer, "readonly") == 0)
			{
				fputs(m_line_buffer, m_output_file);

				return;
			}
			else
			{
				throw runtime_error("Expected to read either the 'dup' or the 'readonly' operator here");
			}
		}
		throw runtime_error("Unexpected end of file");
	}
	void get_encoding_type()
	{
		read_name(m_line_buffer, MAX_NAME_LEN);

		if (isdigit((byte_t)*m_line_buffer))
		{
			fputs(m_line_buffer, m_output_file);

			read_encoding_table();
		}
		else if (isalpha((byte_t)*m_line_buffer))
		{
			read_encoding_name(m_line_buffer);
		}
		else
		{
			throw runtime_error("A number or the encoding name is expected after /Encoding");
		}
	}
	void update_offset()
	{
		int32_t new_offset;

		// update the offset at the header

		m_bin_data_offset = (int32_t)ftell(m_output_file);

		fseek(m_output_file, 2, SEEK_SET);

		// offset is relative to the header, not to the start of the file

		new_offset = m_bin_data_offset - 6;

		fwrite(&new_offset, sizeof(new_offset), 1, m_output_file);

		fseek(m_output_file, m_bin_data_offset, SEEK_SET);
	}
	void goto_binary_data()
	{
		byte_t ch;

		while (read_char(ch))
		{
			if (128 == ch)
			{
				ungetc(ch, m_input_file);

				update_offset();

				return;
			}
			else
			{
				fputc(ch, m_output_file);
			}
		}

		throw runtime_error("Unexpected end of file");
	}

	void write_trailer()
	{
		byte_t hdr2[6] = { 128, 2, 0, 0, 0, 0 };
		long curpos = ftell(m_output_file);
		size_t bin_data_size = curpos - m_bin_data_offset - sizeof(hdr2); // size of the encrypted data
		size_t* size = (size_t*)&hdr2[2];

		*size = bin_data_size;

		// write the rest of the non-encrypted data in the input file

		while (!feof(m_input_file))
		{
			size_t size = fread(m_line_buffer, 1, sizeof(m_line_buffer), m_input_file);

			if (size > 0)
			{
				fwrite(m_line_buffer, size, 1, m_output_file);
			}
		}
		// go to the start of the binary data 

		fseek(m_output_file, m_bin_data_offset, SEEK_SET);

		//update the size of header 2

		fwrite(hdr2, sizeof(hdr2), 1, m_output_file);
	}
	char* find_name(const string& name, size_t offset)
	{
		auto it = std::search(m_bin_data.begin() + offset, m_bin_data.end(), std::boyer_moore_searcher(name.begin(), name.end()));

		if (it != m_bin_data.end())
		{
			char* data = (char*)m_bin_data.data();
			int64_t offset = it - m_bin_data.begin();

			return (data + offset);
		}
		else
		{
			return nullptr;
		}
	}
	void read_lenIV()
	{
		char* p = find_name(string("/lenIV"), 0);

		if (p)
		{
			m_lenIV = atoi(p + 6); // 6 = length of '/lenIV'
		}
		else
		{
			m_lenIV = 4;
		}
	}
	byte_t decrypt(byte_t cipher, uint16_t& key)
	{
		if (m_lenIV < 0)
		{
			return cipher;
		}
		else
		{
			byte_t plain = cipher ^ (key >> 8);
			key = ((cipher + key) * C1 + C2);
			return plain;
		}
	}
	byte_t encrypt(byte_t plain, uint16_t& key)
	{
		byte_t cipher = plain ^ (key >> 8);
		key = (cipher + key) * C1 + C2;
		return cipher;
	}
	void decrypt_binary_data()
	{
		byte_t hdr2[6] = { 0, 0, 0, 0, 0, 0 };

		fread(hdr2, sizeof(hdr2), 1, m_input_file);

		if (hdr2[1] != 2)
		{
			sprintf_s(m_line_buffer, MAX_LINE_BUFFER, "Invalid byte in the secondary header: %d. Expected is '2'", hdr2[1]);

			throw runtime_error(m_line_buffer);
		}
		else
		{
			uint16_t key = key_eexec;
			size_t data_size;
			byte_t* data;
			byte_t ch;

			data_size = *((size_t*)&hdr2[2]);

			if (0 == data_size)
			{
				throw runtime_error("Size of encrypted data is 0");
			}

			fwrite(hdr2, sizeof(hdr2), 1, m_output_file);

			for (int i = 0; i < 4; ++i)
			{
				read_char(ch);

				m_discard_bytes[i] = ch;

				// discard the result of these 4 bytes

				decrypt(ch, key);
			}

			// subtract the 4 bytes discarded

			data_size -= 4;

			m_bin_data.resize(data_size);

			data = (byte_t*)m_bin_data.data();

			fread(data, data_size, 1, m_input_file);

			for (size_t i = 0; i < data_size; ++i)
			{
				data[i] = decrypt(data[i], key);
			}

			read_lenIV();
		}
	}
	void encrypt_data(char* start, size_t len, uint16_t& key)
	{
		for (size_t i = 0; i < len; ++i)
		{
			byte_t ch = (char)encrypt((byte_t)start[i], key);

			start[i] = (char)ch;
		}

		fwrite(start, len, 1, m_output_file);
	}
	char* skip_glyph(const char* start, const char* end)
	{
		char* p = (char*)start;
		int len;
		char* endp;

		if (*p != '/')
			return nullptr;

		while (p < end)
		{
			if (isspace((byte_t)*p))
				break;
			++p;
		}

		len = strtol(p, &endp, 10);

		p = endp;

		while (*p && isspace((byte_t)*p))
			++p;

		// skip 'RD' or -|

		p += 2;

		while (*p && isspace((byte_t)*p))
			++p;

		p += len;

		return strchr(p, '\n') + 1;
	}
	char* write_glyph_data(const string& name, const char* start_data, const char* data_end, const char* curpos, uint16_t& key)
	{
		size_t len = name.size();
		char* endp, * p = (char*)curpos;

		while (p)
		{
			p = find_name(name, p - start_data);

			if (p)
			{
				endp = skip_glyph(p, data_end);

				if (isspace((byte_t)p[len])) // ensure this is an exact match
				{
					encrypt_data(p, endp - p, key);

					return endp;
				}
				// skip this glyph and repeat the search
				p = endp;
			}
			else
			{
				break;
			}
		}
		return nullptr;
	}
	void find_end_of_charstring(const char* curpos, const char* data_start, const char* data_end, uint16_t& key)
	{
		size_t offset = curpos - data_start;
		char* p;// = (char*)curpos;

		do
		{
			p = find_name(string("end"), offset);

			if (p)
			{
				char* start = p;

				p += 3;

				while (*p && isspace((byte_t)*p))
					++p;

				if (strncmp(p, "end", 3) == 0)
				{
					encrypt_data(start, data_end - start, key);

					m_bin_data.clear();

					return;
				}
				offset = p - data_start;
			}
			else
			{
				throw runtime_error("Unable to find the text 'end end' in the /CharString data");
			}
		} while (p < data_end);

		throw runtime_error("Unable to find the text 'end end' in the /CharString data");
	}
	void remove_glyphs()
	{
		char* p = find_name(string("/.notdef"), 0);

		if (p)
		{
			char* data = m_bin_data.data();
			size_t size = m_bin_data.size();
			char* data_end = data + size;
			char* endp, * end_of_notdef;
			uint16_t key = 55665;

			// encrypt the 4 random bytes

			encrypt_data((char*)m_discard_bytes, sizeof(m_discard_bytes), key);

			// skip .notdef

			end_of_notdef = endp = skip_glyph(p, data_end);

			// encrypt from the start of the encrypted data to the end of .notdef

			encrypt_data(data, end_of_notdef - data, key);

			p = end_of_notdef;

			for (const string& name : m_glyph_list)
			{
				// start from the current position

				p = write_glyph_data(name, data, data_end, p, key);

				if (!p)
				{
					// start from the beginning
					p = write_glyph_data(name, data, data_end, end_of_notdef, key);

					// if still null

					if (!p)
					{
						// restart from the beginning, for the next glyph
						p = end_of_notdef;
					}
				}
			}

			m_glyph_list.clear();

			find_end_of_charstring(endp, data, data_end, key);
		}
		else
		{
			throw runtime_error("Unable to locate /CharString");
		}
	}
	void do_subsetting()
	{
		find_encoding();

		get_encoding_type();

		goto_binary_data();

		decrypt_binary_data();

		remove_glyphs();

		write_trailer();
	}
public:
	t1subset() : m_bin_data(), m_glyph_list()
	{
	}
	~t1subset() {}
	bool subset_font(const char* font_name, const byte_t* char_subset, byte_t char_subset_count,
		const char* output_filename, string& error)
	{
		bool result = true;
		try
		{
			precondition(font_name, char_subset, char_subset_count, output_filename);

			do_subsetting();
		}
		catch (const exception& ex)
		{
			error = ex.what();

			result = false;
		}

		clear();

		return result;
	}
};
