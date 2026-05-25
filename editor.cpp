#include "editor.h"
#include "config.h"

#include <stdio.h>

const string MODE_STRING[Mode_Count] = {
	S("normal"),
	S("insert"),
	S("command"),
};

typedef u32 Panel_Flags;
enum Panel_Flag : Panel_Flags {
	Panel_VSplit = 1 << 0, // HSplit otherwise
};

struct Panel {
	Panel_Flags flags;	

	Panel *parent;

	Panel *child1;
	Panel *chidl2;
};

global struct Editor
{
	Ed_Mode mode;

	Buffer *active_buffer;

	Buffer *buffers;

	u64     buffer_count;
	u8      command[128];
	u64     command_length;
	
	Panel *panel_tree;

	/////////////////////
	// ~geb: memory management

	Arena *persist_arena;
	Arena *frame_arena;

	Arena     *buffer_arena;
	Buffer    *free_buffers;
} editor;


funcdef Ed_Cmd
ed__parse_command(string cmd)
{
	string striped = string_strip(cmd);

	string function = S("");
	Slice<string> args = {};

	{
		Slice<string> split = string_split(cmd, editor.frame_arena);
		if (split.len == 0) return {};

		function = split[0];
		if (split.len > 1) {
			args = slice(split, 1, split.len);
		}
	}

	if (string_equal(function, S("open")))
	{
		string path = S("");
		if (args.len > 0) {
			path = args[0];
			Ed_Cmd cmd = open_buffer(path);

			return cmd;
		}
	}

	if (string_equal(function, S("close"))) 
	{
		string path = S("");
		if (args.len > 0) {
			path = args[0];
		}

		Ed_Cmd cmd = close_buffer(path);
		return cmd;
	}

	if (string_equal(function, S("save"))) 
	{
		string path = S("");
		if (args.len > 0) {
			path = args[0];
		}

		Ed_Cmd cmd = save_buffer(path);

		return cmd;
	}
	
	return {};
}

funcdef void
ed_init()
{
	MemZeroStruct(&editor);

	editor.persist_arena = arena_new(MB(8));
	editor.frame_arena = arena_new(MB(8));
	editor.buffer_arena = arena_new(GB(1));

	editor.mode = Mode_Normal;
}

funcdef bool
ed_update()
{
	Frame_Input input = {};
	bool window_close = graphics_update(&input);

	auto buf = editor.active_buffer;
	rune c = input.character;
	string encoded_char = utf8_encode(c, editor.frame_arena);

	switch (editor.mode) {
		case Mode_Normal: {
			switch(c) {
				case 'i': editor.mode = Mode_Insert; break;
				case ':': editor.mode = Mode_Command; break;
				case 'h': buffer_move_cursor(buf, 1, Direction_Left); break;
				case 'l': buffer_move_cursor(buf, 1, Direction_Right); break;
				case 'j': buffer_move_cursor(buf, 1, Direction_Down); break;
				case 'k': buffer_move_cursor(buf, 1, Direction_Up); break;
			}
		} break;

		case Mode_Command: 
		{
			u32 key_flags = input.key_flags;
			u8 input_char = (u8) c;

			if (key_flags & key_Escape) {
				editor.mode = Mode_Normal;
				editor.command_length = 0;
				break;
			} else if (key_flags & (key_Backspace | key_Delete)) {
				if (editor.command_length > 0) editor.command_length -= 1;
			} else if (c) {
				if (c == '\n') {

					string cmd_string = {
						editor.command, editor.command_length
					};

					Ed_Cmd cmd = ed__parse_command(cmd_string);
					ed_execute_cmd(cmd);

					editor.mode = Mode_Normal;
					editor.command_length = 0;
					// @TODO: parse cmd string and execute 
					break;
				} else if (c == '\t') {
					// nothing for now
				} else {
					if (editor.command_length < sizeof(editor.command)) {
						editor.command[editor.command_length] = input_char;
						editor.command_length += 1;
					}
				}
			}
		} break;

		case Mode_Insert: {
			if      (input.key_flags & key_Escape) editor.mode = Mode_Normal;
			else if (input.key_flags & key_Delete) buffer_delete(buf, 1, Direction_Right);
			else if (input.key_flags & key_Backspace) buffer_delete(buf, 1, Direction_Left);
			else if (c) {
				bool move_back = false;

				switch (c) {
				case '\n': {
					u64 line_index = buffer_line_at_index(buf, buf->cursor);
					Range_U64 line_range = buffer_line_range(buf, line_index);
					string data = string_from_bytes(slice_from_list(buf->data));
					string current_line = slice(data, line_range.begin, line_range.end);

					u64 i=0;
					while(i < current_line.len && (current_line[i] == ' ' || current_line[i] == '\t'))
						i += 1;

					string indents = slice(current_line, 0, i);

					encoded_char = string_concat(encoded_char, indents, editor.frame_arena);
				} break;
				}

				buffer_insert(editor.active_buffer, encoded_char);
				if (move_back) buffer_move_cursor(buf, 1, Direction_Left);
			}
		} break;

		default:
			break;
	}

	return window_close;
}

funcdef Ed_Error
ed_execute_cmd(Ed_Cmd cmd)
{
	Ed_Error error = {};
	
	switch (cmd.kind)
	{
		case Cmd_None: break; // ignore

		case Cmd_Buffer_Open: {
			Buffer *buffer = nullptr;
			if (editor.free_buffers) {
				buffer = (Buffer *) editor.free_buffers;
				editor.free_buffers = editor.free_buffers->next;
			} else {
				buffer = alloc_struct(editor.buffer_arena, Buffer);
			}

			string path = cmd.buffer_path;
			string input_str = S("");
			if (path.len) {
				bytes input_data = platform_load_entire_file(path, editor.frame_arena);
				input_str = string_from_bytes(input_data);
			}
			u64 lines = string_count_lines(input_str);

			buffer_make(
				buffer, 
				Max(input_str.len * 2, KB(512)),
				Max(lines * 2, 2048),
				path
			);
			buffer->next = editor.buffers;
			editor.buffers = buffer;
			editor.buffer_count += 1;
			editor.active_buffer = buffer;

			buffer_insert(buffer, input_str);
			buffer->cursor = 0;
		} break;

		case Cmd_Buffer_Close: {
			Buffer *last = nullptr;

			string close_path = cmd.buffer_path;
			if (!close_path.len) {
				close_path = editor.active_buffer->path;
				if (!close_path.len) {
					error.kind = Ed_Error_Invalid_Argument;
					break;
				}
			}

			for(Buffer *buf = editor.buffers; buf != nullptr; last = buf, buf = buf->next) {
				if (string_equal(buf->path, close_path)) {

					if (last) {
						last->next = buf->next;
					} else {
						editor.buffers = buf->next;
					}

					if (editor.active_buffer == buf) {
						editor.active_buffer = editor.buffers;
					}

					editor.buffer_count -= 1;

					buffer_deinit(buf);

					buf->next = editor.free_buffers;
					editor.free_buffers = buf;

					break;
				}
			}
		} break;

		case Cmd_Buffer_Save:
		{
			Buffer *buf = editor.active_buffer;
			if (!buf) break;

			string save_path = cmd.buffer_path;
			if (!save_path.len) {
				save_path = buf->path;
				if (!save_path.len) {
					error.kind = Ed_Error_Invalid_Argument;
					break;
				}
			}

			bytes data = slice_from_list(buf->data);
			bool ok = platform_save_entire_file(save_path, data, editor.frame_arena);
			if (!ok) {
				error.kind = Ed_Error_Cmd_Failed;
				break;
			}
		} break;

		default:
			error.kind = Ed_Error_Invalid_Command;
		break;
	}

	return error;
}

funcdef void
ed_handle_error(Ed_Error error)
{
	if (error.kind == Ed_Error_None) return;

	switch (error.kind)
	{
	default: break;
	}
}

/////////////////////////////////////////////
// ~geb: commands

funcdef Ed_Cmd
open_buffer(string path)
{
	Ed_Cmd cmd = {};

	cmd.kind = Cmd_Buffer_Open;
	cmd.buffer_path = path;

	return cmd;
}


funcdef Ed_Cmd
close_buffer(string path)
{
	Ed_Cmd cmd = {};

	cmd.kind = Cmd_Buffer_Close;
	cmd.buffer_path = path;

	return cmd;
}

funcdef Ed_Cmd
save_buffer(string to_path)
{
	Ed_Cmd cmd = {};

	cmd.kind = Cmd_Buffer_Save;
	cmd.buffer_path = to_path;

	return cmd;
}
