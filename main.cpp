#include "alloc.cpp"
#include "string.cpp"
#include "buffer.cpp"
#include "graphics.cpp"
#include "platform.cpp"

#include "config.h"

enum Editor_Mode {
	Mode_Normal,
	Mode_Insert,
	Mode_Command,
	Mode_Count,
};

const string MODE_STRING[Mode_Count] = {
	S("normal"),
	S("insert"),
	S("command"),
};

global struct {
	Editor_Mode mode;
	List<Buffer> buffers;
    
	List<u8> cmd_buffer;
} editor;

int main()
{
	Arena persist = {};
	arena_make(&persist, malloc_bytes(MB(32)));
    
	Arena transient = {};
	arena_make(&transient, malloc_bytes(MB(32)));
    
	graphics_init("</>", 1280, 800, &persist);
    
	{
		auto buf = alloc_slice(&persist, Buffer, 16);
		editor.buffers = list_from_backing_buffer(buf);
		append(&editor.buffers, {});
        
		auto cmd_buf = alloc_slice(&persist, u8, 128);
		editor.cmd_buffer = list_from_backing_buffer(cmd_buf);
	}
    
	auto buf = &editor.buffers[0];
    
	buffer_make(
		buf,
		alloc_slice(&persist, u8, KB(512)),
		alloc_slice(&persist, u64, 2048)
	);
    
	Frame_Input input = {};
	while(graphics_update(Color::bg, &input))
	{
		rune c = input.character;

		graphics_push_clip({ {0, 0}, {(f32) gfx.win->w , (f32) gfx.win->h }}, &transient);
        
		switch (editor.mode) {
			case Mode_Normal: {
				if (c) {
					switch(c) {
						case 'i': editor.mode = Mode_Insert; break;
						case ':': editor.mode = Mode_Command; break;
						case 'h': buffer_move_cursor(buf, 1, Direction_Left); break;
						case 'l': buffer_move_cursor(buf, 1, Direction_Right); break;
						case 'j': buffer_move_cursor(buf, 1, Direction_Down); break;
						case 'k': buffer_move_cursor(buf, 1, Direction_Up); break;
					}
				}
			} break;
            
			case Mode_Insert: {
				if (input.key_flags & key_Escape) {
					editor.mode = Mode_Normal;
				}
				else if (input.key_flags & key_Delete) {
					buffer_delete(buf, 1);
				}
				else if (input.key_flags & key_Backspace) {
					if (buf->cursor > 0) {
						buffer_move_cursor(buf, 1, Direction_Left);
						buffer_delete(buf, 1);
					}
				}
				else if (c) {
					string input = {
						.raw = (const u8 *)&c,
						.len = 1
					};
					buffer_insert(buf, input);
				}
			} break;
            
			case Mode_Command: {
				if (input.key_flags & key_Escape) {
					editor.mode = Mode_Normal;
					clear(&editor.cmd_buffer);
				}
				else if(input.key_flags & (key_Backspace | key_Delete)) {
					editor.cmd_buffer.len -= 1;
				}
				else if(c) {
					if (c == '\n') {
						string cmd_string = {
							.raw = editor.cmd_buffer.raw,
							.len = editor.cmd_buffer.len
						};

						printf("%.*s\n", s_fmt(cmd_string));

						editor.mode = Mode_Normal;
						clear(&editor.cmd_buffer);
						break;
					}
                    
					append(&editor.cmd_buffer, (u8) c);
				}
			} break;
            
			case Mode_Count: {
			} break;
		}
        
		Slice<string> lines = buffer_as_lines(buf, &transient);
		f32 y = 4;
        
		for (u64 i = 0; i < lines.len; ++i) {
			string line = lines[i];
            
			u64 begin = (u64) (line.raw - buf->data.raw);
			u64 end = begin + line.len;
            
			bool on_cursor = Range_Check(begin, buf->cursor, end);
            
			u32 color = Color::fg;
			u32 num_color = on_cursor ? Color::accent : Color::dim;
            
			if (on_cursor) {
				draw_quad({0.0f, y}, {(f32) gfx.win->w, gfx.line_height}, Color::bg_alt);
			}
            
			string number = string_format(&transient, "%zu", i + 1);
			draw_text(number, {4.0f, y}, num_color);
			f32 text_x = 4.0f + graphics_char_width(' ') * (1 + Max(digit_count_u64(lines.len), 2));
            
			if (on_cursor) {
				u64 cursor_col = buf->cursor - begin;
				string before_cursor = slice(line, 0, cursor_col);
				f32 cursor_x = text_x + graphics_measure_text(before_cursor).x;
                
				draw_quad({cursor_x, y}, {graphics_char_width(' '), gfx.line_height}, Color::cursor);
				if (editor.mode == Mode_Insert)
					draw_quad({cursor_x + 1, y + 1}, {graphics_char_width(' ') - 2, gfx.line_height - 2}, Color::bg_alt);
                
				draw_text(before_cursor, {text_x, y}, color);
                
				int rune_width = 0;
				utf8_decode(slice(line, cursor_col, line.len), &rune_width);
				string cursor_rune  = slice(line, cursor_col, cursor_col + rune_width);
				string after_cursor = slice(line, cursor_col + rune_width, line.len);
                
				draw_text(cursor_rune,  {cursor_x, y}, editor.mode == Mode_Insert ? Color::fg : Color::bg_alt);
				draw_text(after_cursor, {cursor_x + graphics_measure_text(cursor_rune).x, y}, color);
			} else {
				draw_text(line, {text_x, y}, color);
			}
            
			y += gfx.line_height;
			if (y > (f32)gfx.win->h)
				break;
		}
        
		draw_quad({0, gfx.win->h - gfx.line_height}, {(f32) gfx.win->w, gfx.line_height}, Color::bg_alt);
        
		if (editor.mode != Mode_Command) {
			string mode_string = string_format(&transient, "-- %.*s --", s_fmt(MODE_STRING[editor.mode]));
			draw_text(mode_string, {0, gfx.win->h - gfx.line_height}, Color::accent);
		} else {
			string cmd_string = {
				.raw = editor.cmd_buffer.raw,
				.len = editor.cmd_buffer.len
			};
            
			cmd_string = string_format(&transient, ":%.*s", s_fmt(cmd_string));
            
			vec2 size = draw_text(cmd_string, {0, gfx.win->h - gfx.line_height}, Color::accent);
			draw_quad({size.x, gfx.win->h - gfx.line_height}, {(f32) graphics_char_width(' '), gfx.line_height}, Color::accent);
		}
        
		arena_free(&transient);
	}
}
