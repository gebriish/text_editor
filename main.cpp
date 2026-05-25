#include "alloc.cpp"
#include "string.cpp"
#include "buffer.cpp"
#include "graphics.cpp"
#include "platform.cpp"
#include "editor.cpp"

#include "config.h"

/*
Konnichiwa (こんにちは) is the most common and versatile way to say "hello" or "good day" in Japanese.
Pronounced koh-nee-chee-wah, it is an appropriate greeting for both formal and informal situations,
primarily used during the daytime and early evening.
*/

funcdef void
ed__draw_buffer_view(Buffer *buffer, Rect region)
{
	draw_quad(region.from, region.size, Color::bg);

	if (!buffer) {
		string error_message = S("[ no buffer open ]");
		vec2 size = graphics_measure_text(error_message);

		vec2 p = {
			region.from.x + (region.size.x - size.x) * 0.5f,
			region.from.y + (region.size.y - size.y) * 0.5f
		};

		draw_text(error_message, p, Color::error);
		return;
	}

	f32 line_height = graphics_line_height();

	graphics_push_clip(region, editor.frame_arena);
	defer(graphics_pop_clip());

	local_persist f32 space_width = graphics_char_width(' ');

	f32 y = region.from.y;

	f32 region_width = region.size.x;

	Slice<string> lines = buffer_as_lines(buffer, editor.frame_arena);
	
	u64 line_at_cursor = buffer_line_at_index(buffer, buffer->cursor);
	Range_U64 cursor_line_range = buffer_line_range(buffer, line_at_cursor);

	for (u64 i=0; i<lines.len; ++i) {
		string line = lines[i];

		bool on_cursor = (i == line_at_cursor);

		if (on_cursor) { // draw cursor line
			draw_quad({region.from.x, y}, { region_width, line_height}, Color::bg_alt);
			// draw_quad({region.from.x, y + 2}, { region_width, line_height - 4}, Color::bg);
		}

		string number_string = string_format(editor.frame_arena, "%zu", i + 1);
		draw_text(number_string, { region.from.x, y }, on_cursor ? Color::accent : Color::dim);

		f32 x = region.from.x + graphics_char_width('0') * (1 + Max(digit_count_u64(lines.len), 2));

		if (on_cursor && editor.mode != Mode_Command) {
			u64 cursor_offset = buffer->cursor - cursor_line_range.begin;
			string before_cursor = slice(line, 0, cursor_offset);
			f32 cursor_x = x + graphics_measure_text(before_cursor).x;

			vec2 cursor_pos = { cursor_x, y };

			if (editor.mode != Mode_Insert) {
				draw_capsule(cursor_pos, {space_width, line_height}, Color::cursor);
				draw_capsule({cursor_pos.x + 2, cursor_pos.y + 2}, {space_width - 4, line_height - 4}, Color::bg);

			} else {
				draw_quad(cursor_pos, {2, line_height}, Color::cursor);
			}
		}
		draw_text(line, {x, y}, Color::fg);


		y += line_height;
		if (y > region.from.y + region.size.y) break;
	}

	vec2 bottom_line_o = {region.from.x, region.from.y + region.size.y - graphics_line_height()};
	draw_quad_rounded(
		bottom_line_o,
		{region.size.x, graphics_line_height()},
		5, Color::bg_alt
	);

	string mode_string = string_format(editor.frame_arena, "-- %.*s --", s_fmt(MODE_STRING[editor.mode]));
	vec2 path_size = graphics_measure_text(buffer->path);


	draw_text(buffer->path, {region.from.x + region.size.x - path_size.x - 5, bottom_line_o.y}, Color::accent);
	draw_text(mode_string, {bottom_line_o.x + 5, bottom_line_o.y}, Color::accent);
}

int main()
{
	ed_init();

	graphics_init("text editor", 1280, 800, editor.persist_arena);

	u64 last_frame_time = platform_time_now();
	f32 delta_time = 0;

	for (bool quit = false; !quit;)
	{
		u64 curr_time = platform_time_now();
		delta_time = (f32) platform_time_diff(last_frame_time, curr_time).seconds;
		last_frame_time = curr_time;

		((void)delta_time);

		quit = ed_update();

		Rect window_rect = {};
		window_rect.size = graphics_resolution();

		graphics_push_clip(window_rect, editor.frame_arena);
		defer(graphics_pop_clip());

		ed__draw_buffer_view(editor.active_buffer, window_rect);

		if (editor.mode == Mode_Command) {
			push_draw_layer_scoped(Draw_Layer_Popup) {
				const f32 panel_width = window_rect.size.x * 0.3f;
				const f32 panel_height = graphics_line_height();

				f32 x0 = (f32) (window_rect.size.x - panel_width) * 0.5f;
				f32 y0 = 32.0f;

				draw_quad_rounded({x0 - 2, y0 - 2}, {panel_width + 4, panel_height + 4}, 5, Color::cursor);
				draw_quad_rounded({x0 - 1, y0 - 1}, {panel_width + 2, panel_height + 2}, 5, Color::bg);

				Rect clip = {
					{x0, y0}, {panel_width, panel_height}
				};

				graphics_push_clip(clip, editor.frame_arena);
				defer(graphics_pop_clip());

				string str = {
					editor.command,
					editor.command_length
				};

				vec2 text_pos = {x0 + 5, y0};
				vec2 size = draw_text(str, text_pos, Color::cursor);
				text_pos.x += size.x;

				draw_quad(text_pos,  {2, graphics_line_height()}, Color::cursor);
			}
		}

		graphics_submit_draw();

		arena_free(editor.frame_arena);
	}
}
