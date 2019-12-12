#include <windows.h> // Needed for XInput architecture detection
#include <Xinput.h>
#include <obs-module.h>
#include <stdlib.h>
#include <stdbool.h>

const SHORT THUMBLX_MAX = 32767;

bool isPressed(const WORD button, WORD buttons)
{
	return (buttons & button) != 0;
}

WORD getButton(int id)
{
	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));

	int timer = 0;
	WORD button = -1;
	const int sleeptime = 100;
	const int waittime = 10000; // 10 sec
	while (state.Gamepad.wButtons == 0 || timer < waittime)
	{
		XInputGetState(id, &state);
		if (state.Gamepad.wButtons != 0)
		{
			button = state.Gamepad.wButtons;
			return button;
		}
		Sleep(sleeptime);
		timer += sleeptime;
	}
	return button;
}

struct pg_data {
	uint32_t throttle_color;
	uint32_t brake_color;
	uint32_t steer_color;
	uint32_t background_color;
	uint32_t background_transperancy;

	uint32_t width;
	uint32_t height;

	int player_id;
	WORD throttle_button;
	WORD brake_button;
	SHORT deadzone;

	bool throttle_pressed;
	bool brake_pressed;
	SHORT steer_input;

	gs_vertbuffer_t *vbuf;
	obs_source_t *src;
};

gs_vertbuffer_t *create_vbuffer() {
	obs_enter_graphics();

	gs_vertbuffer_t *vbuf = NULL;
	struct gs_vb_data *vrect = NULL;

	// 2 triangle, 2 steer trapez, 2 button = 24
	const int num_verts = 24;
	vrect = gs_vbdata_create();
	vrect->num = num_verts;
	vrect->points = (struct vec3 *)bmalloc(sizeof(struct vec3) * num_verts);
	vrect->num_tex = 1;
	vrect->tvarray = (struct gs_tvertarray *)bmalloc(sizeof(struct gs_tvertarray));
	vrect->tvarray[0].width = 2;
	vrect->tvarray[0].array = bmalloc(sizeof(struct vec2) * num_verts);

	memset(vrect->points, 0, sizeof(struct vec3) * num_verts);
	memset(vrect->tvarray[0].array, 1, sizeof(struct vec2) * num_verts);

	vbuf = gs_vertexbuffer_create(vrect, GS_DYNAMIC);
	if (vbuf == NULL) {
		blog(LOG_INFO, "Couldn't create UV vertex buffer.");
	}

	obs_leave_graphics();
	return vbuf;
}

static const char *pg_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("PrecisionGamepadSource");
}

static void pg_update(void *data, obs_data_t *settings)
{
	struct pg_data *context = data;
	uint32_t throttle_color = (uint32_t)obs_data_get_int(settings, "throttle_color");
	uint32_t brake_color = (uint32_t)obs_data_get_int(settings, "brake_color");
	uint32_t steer_color = (uint32_t)obs_data_get_int(settings, "steer_color");
	uint32_t background_color = (uint32_t)obs_data_get_int(settings, "background_color");
	uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t height = (uint32_t)obs_data_get_int(settings, "height");
	int player_id = obs_data_get_int(settings, "player_id");
	WORD throttle_button = (WORD)obs_data_get_int(settings, "throttle_button");
	WORD brake_button = (WORD)obs_data_get_int(settings, "brake_button");
	SHORT deadzone = (SHORT)obs_data_get_int(settings, "deadzone");

	context->throttle_color = throttle_color;
	context->brake_color = brake_color;
	context->steer_color = steer_color;
	context->background_color = background_color;
	context->width = width;
	context->height = height;
	context->player_id = player_id;
	context->throttle_button = throttle_button;
	context->brake_button = brake_button;
	context->deadzone = deadzone;
}

static void *pg_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct pg_data *context = bzalloc(sizeof(struct pg_data));
	context->throttle_pressed = false;
	context->brake_pressed = false;
	context->steer_input = THUMBLX_MAX / 2;
	context->src = source;
	context->vbuf = create_vbuffer();



	pg_update(context, settings);

	return context;
}

static void pg_destroy(void *data)
{
	struct pg_data *context = data;
	obs_enter_graphics();
	if (context->vbuf != NULL) {
		gs_vertbuffer_t *tmpvbuf = context->vbuf;
		context->vbuf = NULL;
		gs_vertexbuffer_destroy(tmpvbuf);
	}
	obs_leave_graphics();

	bfree(data);
}

static obs_properties_t *pg_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_color(props, "throttle_color",
				 obs_module_text("Color.Throttle"));
	obs_properties_add_color(props, "brake_color",
				 obs_module_text("Color.Brake"));
	obs_properties_add_color(props, "steer_color",
				 obs_module_text("Color.Steer"));
	obs_properties_add_color(props, "background_color",
				 obs_module_text("Color.Background"));

	// obs_properties_add_button(props, "throttle_button", obs_module_text("ThrottleButton"), getButton(2));
	obs_properties_add_int_slider(props, "deadzone", obs_module_text("Deadzone"), 0, THUMBLX_MAX, 1);
	obs_properties_add_int_slider(props, "player_id", obs_module_text("Controller"), 0, 3, 1);

	obs_properties_add_int(props, "width",
			       obs_module_text("Width"), 0, 4096,
			       1);

	obs_properties_add_int(props, "height",
			       obs_module_text("Height"), 0, 4096,
			       1);

	return props;
}

void build_graphics(struct pg_data *context) {
	struct gs_vb_data *vdata = gs_vertexbuffer_get_data(context->vbuf);
	struct vec2 *tvarray = vdata->tvarray[0].array;
	float width = (float)context->width;
	float height = (float)context->height;
	float w_middle = width / 2.0f;
	float h_middle = height / 2.0f;
	float button_width = width * 0.09f;
	float button_cap = height * 0.1f;
	float gap = width * 0.02f;
	float gap2 = gap / 2.0f;

	float throttle_button[5][2] =
		{
			{w_middle, 0.0f}, // Top
			{w_middle - button_width, 0.0f + button_cap}, // TopLeft
			{w_middle + button_width, 0.0f + button_cap}, // TopRight
			{w_middle - button_width, h_middle - gap2}, // BottomLeft
			{w_middle + button_width, h_middle - gap2}, // Bottomright
		};
	float brake_button[5][2] =
		{
			{w_middle, height}, // Top
			{w_middle + button_width, height - button_cap}, // TopRight
			{w_middle - button_width, height - button_cap}, // TopLeft
			{w_middle + button_width, h_middle + gap2}, // Bottomright
			{w_middle - button_width, h_middle + gap2}, // BottomLeft
		};
	for (int i = 0; i < 5; ++i) {
		vec3_set(vdata->points + i, throttle_button[i][0], throttle_button[i][1], 0.0f);
	}
	for (int i = 0; i < 5; ++i) {
		vec3_set(vdata->points + 5 + i, brake_button[i][0], brake_button[i][1], 0.0f);
	}

	// Left Steer
	float left_inner = w_middle - button_width - gap;
	float right_inner = w_middle + button_width + gap;
	float tri_top = 0.0f + button_cap + gap2;
	float tri_bot = height - button_cap - gap2;
	vec3_set(vdata->points + 10, 0.0f, h_middle, 0.0f);
	vec3_set(vdata->points + 11, left_inner, tri_top, 0.0f);
	vec3_set(vdata->points + 12, left_inner, tri_bot, 0.0f);

	// Right Steer
	vec3_set(vdata->points + 13, width, h_middle, 0.0f);
	vec3_set(vdata->points + 14, right_inner, tri_top, 0.0f);
	vec3_set(vdata->points + 15, right_inner, tri_bot, 0.0f);

	// Steer stuff
	float tri_width = left_inner; // a
	float tri_height = h_middle - tri_top; // b
	float deadzone = (float)context->deadzone;
	float steer_input = ((abs((float)context->steer_input) - deadzone) / (1 - deadzone / (float)THUMBLX_MAX) / (float)THUMBLX_MAX);
	float steer_width = tri_width * steer_input;

	float angle = tri_height / tri_width; // tan(rho) = b / a
	float steer_height = tri_height - angle * steer_width; // b' = tan(rho) * a

	// Left Steer Active
	vec3_set(vdata->points + 16, left_inner - steer_width, h_middle - steer_height, 0.0f);
	vec3_set(vdata->points + 17, left_inner - steer_width, h_middle + steer_height, 0.0f);
	vec3_set(vdata->points + 18, left_inner, tri_top, 0.0f);
	vec3_set(vdata->points + 19, left_inner, tri_bot, 0.0f);
	// Right Steer Active
	vec3_set(vdata->points + 20, right_inner + steer_width, h_middle - steer_height, 0.0f);
	vec3_set(vdata->points + 21, right_inner + steer_width, h_middle + steer_height, 0.0f);
	vec3_set(vdata->points + 22, right_inner, tri_top, 0.0f);
	vec3_set(vdata->points + 23, right_inner, tri_bot, 0.0f);
}

static void pg_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct pg_data *context = data;

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 colorVal;

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_vertbuffer_t *vbuf = context->vbuf;
	build_graphics(context);
	gs_vertexbuffer_flush(vbuf);
	gs_load_vertexbuffer(vbuf);
	gs_load_indexbuffer(NULL);

	// Throttle
	bool tp = context->throttle_pressed;
	vec4_from_rgba(&colorVal, tp ? context->throttle_color : context->background_color);
	gs_effect_set_vec4(color, &colorVal);
	gs_draw(GS_TRISTRIP, 0, 5);

	// Brake
	bool bp = context->brake_pressed;
	vec4_from_rgba(&colorVal, bp ? context->brake_color : context->background_color);
	gs_effect_set_vec4(color, &colorVal);
	gs_draw(GS_TRISTRIP, 5, 5);

	// Left Background
	vec4_from_rgba(&colorVal, context->background_color);
	gs_effect_set_vec4(color, &colorVal);
	gs_draw(GS_TRIS, 10, 3);
	// Right Background
	gs_draw(GS_TRIS, 13, 3);

	vec4_from_rgba(&colorVal, context->steer_color);
	gs_effect_set_vec4(color, &colorVal);
	if (abs(context->steer_input) > context->deadzone) {
		if (context->steer_input < 0) {
			// Left Input
			gs_draw(GS_TRISTRIP, 16, 4);
		} else {
			// Right
			gs_draw(GS_TRISTRIP, 20, 4);
		}
	}

	// gs_draw(GS_TRIS, 0, 5); // Throttle
	// gs_draw(GS_TRIS, 5, 5); // Brake
	// gs_draw(GS_TRIS, 10, 3); // Left
	// gs_draw(GS_TRIS, 13, 3); // Right
	// gs_draw_sprite(0, 0, context->width, context->height);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void pg_tick(void *data, float seconds) {
	UNUSED_PARAMETER(seconds);
	struct pg_data *context = data;

	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));
	WORD res = XInputGetState(context->player_id, &state);

	if(res != ERROR_SUCCESS) {
		return;
	}

	WORD buttons = state.Gamepad.wButtons;
	if (isPressed(context->throttle_button, buttons)) {
		context->throttle_pressed = true;
	} else {
		context->throttle_pressed = false;
	}
	if (isPressed(context->brake_button, buttons)) {
		context->brake_pressed = true;
	} else {
		context->brake_pressed = false;
	}
	context->steer_input = state.Gamepad.sThumbLX;
	bool tp = context->throttle_pressed;
}

static uint32_t pg_get_width(void *data)
{
	struct pg_data *context = data;
	return context->width;
}

static uint32_t pg_get_height(void *data)
{
	struct pg_data *context = data;
	return context->height;
}

static void pg_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "throttle_color", 0xFF4DEC53);
	obs_data_set_default_int(settings, "brake_color", 0xFFEC584D);
	obs_data_set_default_int(settings, "steer_color", 0xFF4DBCEC);
	obs_data_set_default_int(settings, "background_color", 0xFFE2E2E2);
	obs_data_set_default_int(settings, "width", 500);
	obs_data_set_default_int(settings, "height", 300);
	obs_data_set_default_int(settings, "player_id", 0);
	obs_data_set_default_int(settings, "throttle_button", XINPUT_GAMEPAD_A);
	obs_data_set_default_int(settings, "brake_button", XINPUT_GAMEPAD_X);
	obs_data_set_default_int(settings, "deadzone", 7000);
}

struct obs_source_info pg_source_info = {
	.id = "precision_gamepad_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = pg_getname,
	.create = pg_create,
	.destroy = pg_destroy,
	.update = pg_update,
	.video_render = pg_render,
	.video_tick = pg_tick,
	.get_defaults = pg_defaults,
	.get_properties = pg_properties,
	// .icon_type = OBS_ICON_TYPE_GAME_CAPTURE,
	.get_width = pg_get_width,
	.get_height = pg_get_height,
};

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE("precision-gamepad-source", "en-US")

bool obs_module_load(void)
{
	obs_register_source(&pg_source_info);
	return true;
}
