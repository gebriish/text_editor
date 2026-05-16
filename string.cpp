#include "editor.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>


funcdef int
utf8_character_width(u8 first_byte)
{
	local_persist int widths[256] = {
		// ascii control characters
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

		// ascii characters
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

		// continuation bytes
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,

		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	return widths[first_byte];
}


funcdef string
string_concat(string a, string b, Arena *allocator)
{
	bytes data = alloc_slice(allocator, u8, a.len + b.len);
	string result = string_from_bytes(data);
	
	memcpy((void *)result.raw, (void *) a.raw, a.len);
	memcpy((void *)(result.raw + a.len), (void *) b.raw, b.len);
	
	return result;
}


funcdef rune
utf8_decode(string slice, int *width)
{
	if (!slice.len) {
		if (width) *width = 0;
		return 0;
	}

	const u8 *s = slice.raw;
	u8  b0 = s[0];

	int w = utf8_character_width(b0);

	if (width) *width = w;

	if (w == 0 || slice.len < (u64)w) {
		if (width) *width = 1;
		return 0xFFFD;
	}

	switch (w) {
		case 1: {
			return b0;
		} break;

		case 2: {
			u8 b1 = s[1];

			if ((b1 & 0xC0) != 0x80)
				goto invalid;

			rune cp =
				((b0 & 0x1F) << 6) |
				((b1 & 0x3F) << 0);

			if (cp < 0x80)
				goto invalid;

			return cp;
		} break;

		case 3: {
			u8 b1 = s[1];
			u8 b2 = s[2];

			if ((b1 & 0xC0) != 0x80 ||
				(b2 & 0xC0) != 0x80)
				goto invalid;

			rune cp =
				((b0 & 0x0F) << 12) |
				((b1 & 0x3F) << 6 ) |
				((b2 & 0x3F) << 0 );

			if (cp < 0x800)
				goto invalid;

			if (cp >= 0xD800 && cp <= 0xDFFF)
				goto invalid;

			return cp;
		} break;

		case 4: {
			u8 b1 = s[1];
			u8 b2 = s[2];
			u8 b3 = s[3];

			if ((b1 & 0xC0) != 0x80 ||
				(b2 & 0xC0) != 0x80 ||
				(b3 & 0xC0) != 0x80)
				goto invalid;

			rune cp =
				((b0 & 0x07) << 18) |
				((b1 & 0x3F) << 12) |
				((b2 & 0x3F) << 6 ) |
				((b3 & 0x3F) << 0 );

			if (cp < 0x10000)
				goto invalid;

			if (cp > 0x10FFFF)
				goto invalid;

			return cp;
		} break;
	}

invalid:
	if (width) *width = 1;
	return 0xFFFD;
}


funcdef string
string_format(Arena *arena, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (len < 0) return {0};

	bytes buf = alloc_slice(arena, u8, len + 1);

	va_start(args, fmt);
	vsnprintf((char *)buf.raw, len + 1, fmt, args);
	va_end(args);

	return { .raw = buf.raw, .len = (u64) len };
}

funcdef Slice<string>
string_as_lines(string parent, Arena *allocator)
{
    if (parent.len == 0) {
		return {};
	}

    u64 line_count = 0;
    for (u64 i = 0; i < parent.len; ++i) {
        if (parent[i] == '\n') line_count += 1;
    }

    if (parent[parent.len - 1] != '\n') line_count += 1;

    Slice<string> lines = alloc_slice(allocator, string, line_count);
    u64 out = 0;
    u64 i0  = 0;

    for (u64 i = 0; i < parent.len; ++i) {
        if (parent[i] != '\n') continue;
        lines[out++] = slice(parent, i0, i);
        i0 = i + 1;
    }
    if (parent[parent.len - 1] != '\n') {
        lines[out++] = slice(parent, i0, parent.len);
    }

    return lines;
}
