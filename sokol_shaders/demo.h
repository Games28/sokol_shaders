#define SOKOL_GLCORE
#include "sokol_engine.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include <iostream>

#include "shd.glsl.h"

#include "math/v3d.h"
#include "math/mat4.h"
#include "v2d.h"

#include "mesh.h"

//for time
#include <ctime>

#include "texture_utils.h"
#include "Object.h"
#include "font.h"


#pragma region STATIC FUNCTIONS

static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw) * std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw) * std::cos(pitch)
	};
}

static vf3d polarToCartesian(float yaw, float pitch) {
	return {
		std::sin(yaw) * std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw) * std::cos(pitch)
	};
}

//x y z => y p
//0 0 1 => 0 0
static void cartesianToPolar(const vf3d& pt, float& yaw, float& pitch) {
	//flatten onto xz
	yaw = std::atan2(pt.x, pt.z);
	//vertical triangle
	pitch = std::atan2(pt.y, std::sqrt(pt.x * pt.x + pt.z * pt.z));
}

//coordinate system transform
static mat4 makeTransformMatrix(const vf3d& x, const vf3d& y, const vf3d& z, const vf3d& t) {
	mat4 m;
	m(0, 0) = x.x, m(0, 1) = y.x, m(0, 2) = z.x, m(0, 3) = t.x;
	m(1, 0) = x.y, m(1, 1) = y.y, m(1, 2) = z.y, m(1, 3) = t.y;
	m(2, 0) = x.z, m(2, 1) = y.z, m(2, 2) = z.z, m(2, 3) = t.z;
	m(3, 3) = 1;
	return m;
}

#pragma endregion

#pragma region STRUCTS

struct
{
	vf3d pos{ 0,2,2 };
	vf3d dir;
	float yaw = 0;
	float pitch = 0;
	const float near_plane = .001f, far_plane = 1000;
	mat4 proj, view;
	mat4 view_proj;
}cam;

struct Light
{
	vf3d pos;
	sg_color col;

};

typedef struct Player
{
	vf3d position;
	float radius;
	vf3d velocity;
	bool grounded;
};

struct
{
	Object object;

	int num_x = 0, num_y = 0;
	int num_ttl = 0;

	float anim_timer = 0;
	int anim = 0;

	sg_pipeline pip{};
	sg_bindings bind{};
	sg_view gui_image{};

}gGui;

struct
{
	sg_buffer vbuf{};
	sg_pipeline colorview_pip{};
	cmn::Font fancy;
	cmn::Font monogram;
	cmn::Font round;

	struct {
		bool to_render = true;

		//colored positions
		float kx = 0, ky = 0;
		float rx = 0, ry = 0;
		float gx = 0, gy = 0;
		float bx = 0, by = 0;
	}test;
}fonts;

struct sfontSting {
	cmn::Font* font;
	std::string str;
	float scl;
	sg_color tint;
};

#pragma endregion




struct Demo : SokolEngine {
	sg_pipeline default_pip{};
	sg_pipeline line_pip{};
	float radian = 0.0174532777777778;
	//cam info


	int anim_index = 0;
	float obj_dist = 0;

	struct {
		sg_sampler linear{};
		sg_sampler nearest{};
	} samplers;

	struct {
		sg_pipeline pip{};
		Object faces[6];

	}skybox;

	struct {
		sg_pass_action pass_action{};

		sg_image color_img{ SG_INVALID_ID };
		sg_view color_attach{ SG_INVALID_ID };
		sg_view color_tex{ SG_INVALID_ID };

		sg_image depth_img{ SG_INVALID_ID };
		sg_view depth_attach{ SG_INVALID_ID };
	} render;


	struct {
		sg_pass_action pass_action{};

		sg_pipeline pip{};

		sg_buffer vbuf{};
		sg_buffer ibuf{};
	} process;

	//grab object test
	vf3d current_dir, prevous_dir;
	Object* held_obj = nullptr;
	vf3d grab_ctr, grab_norm;

	
	std::vector<Light> lights;
	Light* mainlight;

	sfontSting input_text;

	std::vector<Object> scene_objects;
	std::vector<Object> objectlist;



	
	const std::vector<std::string> Structurefilenames{
		"assets/models/deserttest.txt",
		"assets/models/tathouse1.txt",
	};

	const std::vector<std::string> spritefileNames = {
		"assets/sprites/probidletest.png",
		"assets/sprites/r2idletest.png",
		"assets/sprites/trooper.png",
		"assets/sprites/fps_fireball1.png"
	};


	sg_view tex_blank{};
	sg_view tex_uv{};

	sg_view gui_image{};

	sg_pass_action display_pass_action{};

	Object platform;

	bool contact_test = false;
	bool grounded = false;
	bool render_outlines = false;
	//player camera test
	vf3d player_pos, player_vel, gravity = { 0,-9.8,0 };
	float player_height = 0.25f, player_rad = 0.1f, player_airtime = 0;
	bool player_camera = false, player_on_ground = false;
	float ground = 1.0f;
	vf2d min, max;

	

	float obj_index = 0;

#pragma region BASICS SETUP
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment = sglue_environment();
		sg_setup(desc);
	}

	void setupLights()
	{
		//white
		lights.push_back({ {-1,3,1},{1,1,1,1} });
		mainlight = &lights.back();

	}

	//primitive textures to debug with
	void setupTextures() {
		tex_blank = makeBlankTexture();
		tex_uv = makeUVTexture(512, 512);
	}

	//if texture loading fails, default to uv tex.
	sg_view getTexture(const std::string& filename) {
		sg_view tex;
		auto status = makeTextureFromFile(tex, filename);
		if (!status.valid) tex = tex_uv;
		return tex;
	}

	void setupSampler() {
		{
			sg_sampler_desc sampler_desc{};
			samplers.linear = sg_make_sampler(sampler_desc);
		}

		{
			sg_sampler_desc sampler_desc{};
			sampler_desc.min_filter = SG_FILTER_NEAREST;
			sampler_desc.mag_filter = SG_FILTER_NEAREST;
			samplers.nearest = sg_make_sampler(sampler_desc);
		}
	}

	//clear to bluish
	void setupDisplayPassAction() {
		display_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value = { .25f, .45f, .65f, 1.f };
	}

	void setupDefaultPipeline() {
		sg_pipeline_desc pipeline_desc{};
		pipeline_desc.layout.attrs[ATTR_default_v_pos].format = SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_default_v_norm].format = SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_default_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pipeline_desc.shader = sg_make_shader(default_shader_desc(sg_query_backend()));
		pipeline_desc.index_type = SG_INDEXTYPE_UINT32;
		pipeline_desc.cull_mode = SG_CULLMODE_FRONT;
		pipeline_desc.depth.write_enabled = true;
		pipeline_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;

		pipeline_desc.colors[0].blend.enabled = true;
		pipeline_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
		pipeline_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pipeline_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
		pipeline_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

		default_pip = sg_make_pipeline(pipeline_desc);
	}

	//works with linemesh
	void setupLinePipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_line_i_pos].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_line_i_col].format = SG_VERTEXFORMAT_FLOAT4;
		pip_desc.shader = sg_make_shader(line_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_LINES;
		pip_desc.index_type = SG_INDEXTYPE_UINT32;
		pip_desc.depth.write_enabled = true;
		pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
		line_pip = sg_make_pipeline(pip_desc);
	}


#pragma endregion

#pragma region SCENE OBJECTS SETUP

	void setupSceneObjects( )
	{
		std::vector<vf3d> coords = {
			{11,-2,4},
			//{-14,-2,17},
			//{16,-2,-14},
			//{-14,-2,-14},
		};
		for (int i = 0; i < coords.size(); i++)
		{
			Object b;
			Mesh& m = b.mesh;
			auto status = Mesh::loadFromOBJ(m, Structurefilenames[1]);
			if (!status.valid) m = Mesh::makeCube();
			b.scale = { 1,1,1 };
			b.translation = coords[i];
			b.updateMatrixes();
			b.tex = getTexture("assets/poust_1.png");
			b.addLineMesh();
			scene_objects.push_back(b);
			
			
		}
	}

	void setupScenePlatform() {
		Object b;
		Mesh& m = b.mesh;
		auto status = Mesh::loadFromOBJ(m, Structurefilenames[0]);
		if (!status.valid) m = Mesh::makeCube();
		b.scale = { 5,1,5 };
		b.translation = { 0,-2,0 };
		b.updateMatrixes();
		b.tex = getTexture("assets/poust_1.png");
		b.addLineMesh();
		scene_objects.push_back(b);
	
	}

	void setupSceneBillboard() {
		Object obj;
		Mesh& m = obj.mesh;
		m.verts = {
			{{-.5f, .5f, 0}, {0, 0, 1}, {0, 0}},//tl
			{{.5f, .5f, 0}, {0, 0, 1}, {1, 0}},//tr
			{{-.5f, -.5f, 0}, {0, 0, 1}, {0, 1}},//bl
			{{.5f, -.5f, 0}, {0, 0, 1}, {1, 1}},//br

		};
		m.tris = {
			{0, 2, 1},
			{1, 2, 3},

		};
		m.updateVertexBuffer();
		m.updateIndexBuffer();

		obj.translation = { 0, 1, 0 };
		obj.isbillboard = true;
		obj.draggable = true;
		obj.addLineMesh();
		obj.tex = getTexture("assets/spritesheet.png");
		obj.num_x = 4, obj.num_y = 4;
		obj.num_ttl = obj.num_x * obj.num_y;
		scene_objects.push_back(obj);
	}

	void setup_Quad()
	{
		//2d texture quad
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_texview_v_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_texview_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(texview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;

		//with alpha blending
		pip_desc.colors[0].blend.enabled = true;
		pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		gGui.pip = sg_make_pipeline(pip_desc);

		//quad vertex buffer: xyuv
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 0}},//tl
			{{1, -1}, {1, 0}},//tr
			{{-1, 1}, {0, 1}},//bl
			{{1, 1}, {1, 1}}//br
		};

		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr = vertexes;
		vbuf_desc.data.size = sizeof(vertexes);
		gGui.bind.vertex_buffers[0] = sg_make_buffer(vbuf_desc);
		gGui.bind.samplers[SMP_texview_smp] = samplers.linear;

		gGui.gui_image = getTexture("assets/hands.png");

		//setup texture animatons
		gGui.num_x = 3; gGui.num_y = 1;
		gGui.num_ttl = gGui.num_x * gGui.num_y;

	}

#pragma endregion

#pragma region OBJECTS SETUP

	void setupObjectBillboards() {
		
	
		std::vector<vf3d> coords = {
			{5,2, 17},
			{-5, 2 , 15},
			{5,2, -10},
		

		};

		for (int i = 0; i < coords.size(); i++)
		{
			Object obj;
			Mesh& m = obj.mesh;
			m.verts = {
				{{-.5f, .5f, 0}, {0, 0, 1}, {0, 0}},//tl
				{{.5f, .5f, 0}, {0, 0, 1}, {1, 0}},//tr
				{{-.5f, -.5f, 0}, {0, 0, 1}, {0, 1}},//bl
				{{.5f, -.5f, 0}, {0, 0, 1}, {1, 1}},//br

			};
			m.tris = {
				{0, 2, 1},
				{1, 2, 3},

			};
			m.updateVertexBuffer();
			m.updateIndexBuffer();

			obj.translation = coords[i];
			obj.isbillboard = true;
			obj.draggable = true;
			obj.addLineMesh();
			obj.tex = getTexture(spritefileNames[i]);
			obj.num_x = 4, obj.num_y = 4;
			obj.num_ttl = obj.num_x * obj.num_y;
			objectlist.push_back(obj);
		}
	}

#pragma endregion

#pragma region SKYBOX SETUP

	void setupSkybox()
	{
		//pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_skybox_v_pos].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_v_norm].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(skybox_shader_desc(sg_query_backend()));
		pip_desc.index_type = SG_INDEXTYPE_UINT32;
		skybox.pip = sg_make_pipeline(pip_desc);

		const vf3d rot_trans[6][2]{
		{Pi * vf3d(.5f, -.5f, 0), {.5f, 0, 0}},
		{Pi * vf3d(.5f, .5f, 0), {-.5f, 0, 0}},
		{Pi * vf3d(0, 0, 1), {0, .5f, 0}},
		{Pi * vf3d(1, 0, 1), {0, -.5f, 0}},
		{Pi * vf3d(.5f, 1, 0), {0, 0, .5f}},
		{Pi * vf3d(.5f, 0, 0), {0, 0, -.5f}}
		};

		Mesh face_mesh;
		face_mesh.verts = {
			{{-1, 0, -1}, {0, 1, 0}, {0, 0}},
			{{1, 0, -1}, {0, 1, 0}, {1, 0}},
			{{-1, 0, 1}, {0, 1, 0}, {0, 1}},
			{{1, 0, 1}, {0, 1, 0}, {1, 1}}
		};
		face_mesh.tris = {
			{0, 2, 1},
			{1, 2, 3}
		};
		face_mesh.updateVertexBuffer();
		face_mesh.updateIndexBuffer();

		const std::string filenames[6]{
			"assets/img/skybox/px.png",
			"assets/img/skybox/nx.png",
			"assets/img/skybox/py.png",
			"assets/img/skybox/ny.png",
			"assets/img/skybox/pz.png",
			"assets/img/skybox/nz.png"
		};

		for (int i = 0; i < 6; i++)
		{
			auto& obj = skybox.faces[i];

			//orient meshes
			obj.mesh = face_mesh;
			obj.scale = .5f * vf3d(1, 1, 1);
			obj.rotation = rot_trans[i][0];
			obj.translation = rot_trans[i][1];
			obj.updateMatrixes();

			sg_view& tex = obj.tex;
			if (!makeTextureFromFile(tex, filenames[i]).valid) tex = tex_uv;

		}

	}

#pragma endregion



#pragma region FONT SETUP

	void setupFontPipeline()
	{
		//2d tristrip pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_fontview_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_fontview_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(fontview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		//with alpha blending
		pip_desc.colors[0].blend.enabled = true;
		pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		fonts.colorview_pip = sg_make_pipeline(pip_desc);

		//xyuv
		//flip y
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 1}},
			{{1, -1}, {1, 1}},
			{{-1, 1}, {0, 0}},
			{{1, 1}, {1, 0}}
		};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr = vertexes;
		vbuf_desc.data.size = sizeof(vertexes);
		fonts.vbuf = sg_make_buffer(vbuf_desc);
	}

	void setupFonts() {
		fonts.fancy = cmn::Font("assets/font/fancy_8x16.png", 8, 16);
		fonts.monogram = cmn::Font("assets/font/monogram_6x9.png", 6, 9);
		fonts.round = cmn::Font("assets/font/round_6x6.png", 6, 6);
	}

	void setupFontStrings()
	{
		input_text.font = &fonts.fancy;
		input_text.scl = 1;
		input_text.tint = { 0,1,1,1 };


		
	}

#pragma endregion

#pragma region EFFECT SHADER SETUP

	//since will be called on resize,
	//  this needs to free & remake resources
	void setupRenderTarget() {
		//make color img
		{
			sg_destroy_image(render.color_img);
			sg_image_desc image_desc{};
			image_desc.usage.color_attachment = true;
			image_desc.width = sapp_width();
			image_desc.height = sapp_height();
			render.color_img = sg_make_image(image_desc);

			//make color attach
			{
				sg_destroy_view(render.color_attach);
				sg_view_desc view_desc{};
				view_desc.color_attachment.image = render.color_img;
				render.color_attach = sg_make_view(view_desc);
			}

			//make color tex
			{
				sg_destroy_view(render.color_tex);
				sg_view_desc view_desc{};
				view_desc.texture.image = render.color_img;
				render.color_tex = sg_make_view(view_desc);
			}
		}

		{
			//make depth img
			sg_destroy_image(render.depth_img);
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment = true;
			image_desc.width = sapp_width();
			image_desc.height = sapp_height();
			image_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
			render.depth_img = sg_make_image(image_desc);

			//make depth attach
			sg_destroy_view(render.depth_attach);
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image = render.depth_img;
			render.depth_attach = sg_make_view(view_desc);
		}
	}

	void setupRender() {
		setupRenderTarget();

		//clear to grey
		render.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		render.pass_action.colors[0].clear_value = { .5f, .5f, .5f, 1 };
	}

	void setupProcess() {
		process.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		process.pass_action.colors[0].clear_value = { 0, 0, 0, 1 };

		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_process_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_process_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(process_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		process.pip = sg_make_pipeline(pip_desc);

		//xyuv
		float vertexes[4][4]{
			{-1, -1, 0, 0},
			{1, -1, 1, 0},
			{-1, 1, 0, 1},
			{1, 1, 1, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data = SG_RANGE(vertexes);
		process.vbuf = sg_make_buffer(buffer_desc);
	}

	

#pragma endregion

	


	void userCreate() override {
		
		//basics
		setupEnvironment();
		setupDefaultPipeline();
		setupLinePipeline();
		setupTextures();
        setupSampler();
		setupDisplayPassAction();
		setupLights();
		setup_Quad();
		setupSkybox();
		
		//scene objects
		setupScenePlatform();
		setupSceneObjects();
        setupSceneBillboard();

		//fonts
		setupFontPipeline();
		setupFonts();
		setupFontStrings();
		
		//objects
		setupObjectBillboards();

		//display shader
		setupRender();
		setupProcess();



	}


#pragma region INPUT UPDATE

	void updateCameraMatrixes() {
		mat4 look_at = mat4::makeLookAt(cam.pos, cam.pos + cam.dir, { 0, 1, 0 });
		cam.view = mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp = sapp_widthf() / sapp_heightf();
		cam.proj = mat4::makePerspective(90, asp, cam.near_plane, cam.far_plane);

		cam.view_proj = mat4::mul(cam.proj, cam.view);
	}

	void updateCameraRay()
	{
		prevous_dir = current_dir;


		float ndc_x = 2 * cam.pos.x / sapp_widthf() - 1;
		float ndc_y = 1 - 2 * cam.pos.y / sapp_heightf();
		vf3d clip(ndc_x, ndc_y, 1);
		float w = 1;
		vf3d world = matMulVec(cam.view_proj, clip, w);
		world /= w;

		current_dir = (world - cam.pos).norm();

	}

	void handleCameraLooking(float dt) {
		//left/right
		if (getKey(SAPP_KEYCODE_LEFT).held) cam.yaw += dt;
		if (getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw -= dt;

		//up/down
		if (getKey(SAPP_KEYCODE_UP).held) cam.pitch += dt;
		if (getKey(SAPP_KEYCODE_DOWN).held) cam.pitch -= dt;

		//clamp camera pitch
		if (cam.pitch > Pi / 2) cam.pitch = Pi / 2 - .001f;
		if (cam.pitch < -Pi / 2) cam.pitch = .001f - Pi / 2;

		//turn player_camera off and on
		if (getKey(SAPP_KEYCODE_Z).pressed)
		{
			if (!player_camera) {

				player_vel = { 0, 0, 0 };
				player_on_ground = false;
				player_pos = cam.pos - vf3d(0, player_height, 0);
			}
			player_camera ^= true;
		}
	}

	void handleCameraMovement(float dt) {


		if (!player_camera)
		{
			//move up, down
			if (getKey(SAPP_KEYCODE_SPACE).held) cam.pos.y += 4.f * dt;
			if (getKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y -= 4.f * dt;


			//move forward, backward
			vf3d fb_dir(std::sin(cam.yaw), 0, std::cos(cam.yaw));
			if (getKey(SAPP_KEYCODE_W).held) cam.pos += 5.f * dt * fb_dir;
			if (getKey(SAPP_KEYCODE_S).held) cam.pos -= 3.f * dt * fb_dir;

			//move left, right
			vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
			if (getKey(SAPP_KEYCODE_A).held) cam.pos += 4.f * dt * lr_dir;
			if (getKey(SAPP_KEYCODE_D).held) cam.pos -= 4.f * dt * lr_dir;
		}

	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		//grab and drag with camera 
		{
			const auto grab_action = getKey(SAPP_KEYCODE_F);
			if (grab_action.pressed)
			{
				handleGrabActionBegin();
			}
			if (grab_action.held)
			{
				handleGrabActionUpdate();
			}
			if (grab_action.released)
			{
				handleGrabActionEnd();
			}
		}
		//polar to cartesian
		cam.dir = polar3D(cam.yaw, cam.pitch);
		if (getKey(SAPP_KEYCODE_R).held) mainlight->pos = cam.pos;
		if (getKey(SAPP_KEYCODE_O).pressed) render_outlines ^= true;


		handleCameraMovement(dt);
	}


	

#pragma endregion


#pragma region TELEKINESIS UPDATE
	vf3d rayIntersectPlane(const vf3d& a, const vf3d& b, const vf3d& ctr, const vf3d& norm, float* tp = nullptr)
	{
		float t = norm.dot(ctr - a) / norm.dot(b - a);
		if (tp) *tp = t;
		return a + t * (b - a);
	}

	
	void handleGrabActionBegin()
	{
		contact_test = false;
		handleGrabActionEnd();
		//contact_test = false;
		float record = -1;
		Object* close_obj = nullptr;
		for (auto& o : scene_objects)
		{
			if (o.isbillboard)
			{
				float dist = intersectRay(o, cam.pos, cam.dir);
				if (dist < 0) continue;
				//"sort" while iterating
				if (record < 0 || dist < record) {
					record = dist;
					close_obj = &o;
					contact_test = true;
				}
			}
		}
		if (!close_obj) return;
		if (!close_obj->draggable) return;
		if (close_obj)
		{
			held_obj = close_obj;
			obj_dist = record;
		}



	}

	void handleGrabActionEnd()
	{
		held_obj = nullptr;
	}

	void handleGrabActionUpdate()
	{
		if (!held_obj) return;


		held_obj->translation = cam.pos + obj_dist * cam.dir;
		held_obj->updateMatrixes();
	}


#pragma endregion

#pragma region 2D/BILLBOARD UPDATES
	//make billboard always point at camera.
	void updateBillboard(Object& obj, float dt) {
		//move with player 
		vf3d eye_pos = obj.translation;
		vf3d target = cam.pos;

		vf3d y_axis(0, 1, 0);
		vf3d z_axis = (target - eye_pos).norm();
		vf3d x_axis = y_axis.cross(z_axis).norm();
		y_axis = z_axis.cross(x_axis);

		//slightly different than makeLookAt.
		mat4& m = obj.model;
		m(0, 0) = x_axis.x, m(0, 1) = y_axis.x, m(0, 2) = z_axis.x, m(0, 3) = eye_pos.x;
		m(1, 0) = x_axis.y, m(1, 1) = y_axis.y, m(1, 2) = z_axis.y, m(1, 3) = eye_pos.y;
		m(2, 0) = x_axis.z, m(2, 1) = y_axis.z, m(2, 2) = z_axis.z, m(2, 3) = eye_pos.z;
		m(3, 3) = 1;

		float angle = atan2f(z_axis.z, z_axis.x);
		//
		//int i = 0;
		//
		if (angle < -0.70 && angle > -2.35)
		{
			obj.anim = 1; //front
		}
		if (angle < -2.35 && angle < 2.35)
		{
			obj.anim = 4; //left
		}
		if (angle > -0.70 && angle < 0.70)
		{
			obj.anim = 8; //right
		}
		if (angle > 0.70 && angle < 2.35)
		{
			obj.anim = 12; //back
		}
		//obj.anim_timer-=dt;
		//if(obj.anim_timer<0) {
		//	obj.anim_timer+=.5f;
		//
		//	//increment animation index and wrap
		//	obj.anim++;
		//	obj.anim%=obj.num_ttl;
		//}
	}

	void updateGui(float dt)
	{
		gGui.anim_timer -= dt;
		if (gGui.anim_timer < 0)
		{
			gGui.anim_timer += .5f;

			//increment animation index and wrap
			gGui.anim++;
			gGui.anim %= gGui.num_ttl;
		}
	}



#pragma endregion

#pragma region PHYSICS UPDATE

	bool sphereIntersectBox(const vf3d& ctr, float rad, const vf3d& min, const vf3d& max)
	{
		vf3d close_pt{//clamp to box
		ctr.x<min.x ? min.x : ctr.x>max.x ? max.x : ctr.x,
		ctr.y<min.y ? min.y : ctr.y>max.y ? max.y : ctr.y,
		ctr.z<min.z ? min.z : ctr.z>max.z ? max.z : ctr.z,
		};
		return (ctr - close_pt).mag() < rad;
	}

	float intersectRay(Object& obj, const vf3d& orig_world, const vf3d& dir_world)
	{
		float w = 1;
		mat4 inv_model = mat4::inverse(obj.model);
		vf3d orig_local = matMulVec(inv_model, orig_world, w);
		w = 0;
		vf3d dir_local = matMulVec(inv_model, dir_world, w);

		dir_local = dir_local.norm();

		float record = -1;
		for (const auto& t : obj.mesh.tris) {
			float dist = obj.mesh.rayIntersectTri(
				orig_local,
				dir_local,
				obj.mesh.verts[t.a].pos,
				obj.mesh.verts[t.b].pos,
				obj.mesh.verts[t.c].pos
			);

			if (dist < 0) continue;

			//sort while iterating
			if (record < 0 || dist < record) record = dist;


		}

		if (record < 0) return -1;

		vf3d p_local = orig_local + record * dir_local;
		w = 1;
		vf3d p_world = matMulVec(obj.model, p_local, w);
		return(p_world - orig_world).mag();
	}

	void objectVertCollision(Object& obj, float dt)
	{
		vf3d object_pos = obj.translation;

		float total_dist = 10000;
		obj.grounded = false;
		for (auto& scene : scene_objects)
		{
			float dist = 0;
			dist = intersectRay(scene, obj.translation, { 0,-1,0 });
			if (dist == -1) continue;
			if (dist < total_dist)
			{
				total_dist = dist;
			}


			//obj_index = total_dist;
			if (total_dist <= obj.ground)
			{
				obj.grounded = true;
			}


		}


		if (!obj.grounded)
		{
			obj.object_velocity += gravity * dt;
		}
		else
		{
			obj.object_velocity = { 0,0,0 };
		}

		if (obj.grounded)
		{
			float yoffset = obj.ground - total_dist;

	        obj.object_velocity.y += yoffset;
			player_airtime = 0;

			
		}

		obj.translation += obj.object_velocity * dt;

		
		
	}

	void sceneVertCollision(float dt)
	{
		
		
		//dist_text = intersectRay(objects[0], cam.pos, {0,-1,0});

		if (player_camera)
		{


			vf3d movement;
			//move forward, backward
			vf3d fb_dir(std::sin(cam.yaw), 0, std::cos(cam.yaw));
			if (getKey(SAPP_KEYCODE_W).held) movement += 5.f * dt * fb_dir;
			if (getKey(SAPP_KEYCODE_S).held) movement -= 3.f * dt * fb_dir;

			//move left, right
			vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
			if (getKey(SAPP_KEYCODE_A).held) movement += 4.f * dt * lr_dir;
			if (getKey(SAPP_KEYCODE_D).held) movement -= 4.f * dt * lr_dir;

			obj_index = movement.z;
			float total_dist = 100000;
			player_airtime += dt;
			grounded = false;
			for (int i = 0; i < scene_objects.size(); i++)
			{
				
				float dist = 0;
				dist = intersectRay(scene_objects[i], player_pos + (movement * 2), {0,-1,0});
			   if (dist == -1) continue;

				if (dist < total_dist)
				{
					total_dist = dist;
				}


				//obj_index = total_dist;
				if (total_dist <= ground)
				{
					grounded = true;
				}
			}

			if (!grounded)
			{
				player_vel += gravity * dt;
			}
			else
			{
				player_vel = { 0,0,0 };
			}

			if(grounded)
			{
				float yoffset = ground - total_dist;
				
				player_pos.y += yoffset;
				player_airtime = 0;
				
				if (getKey(SAPP_KEYCODE_SPACE).pressed)
				{
					if (player_airtime < 0.1f)
					{
						player_vel.y = 10;
						player_on_ground = false;
					}
				}
				else
				{
					player_vel = { 0,0,0 };
				}

				
				
			}
			

			player_pos += player_vel * dt;

			player_pos.x += movement.x;
			player_pos.z += movement.z;

		
			for (int i = 1; i < scene_objects.size(); i++)
			{
				AABB3 bounds = scene_objects[i].getAABB();
				
				if (sphereIntersectBox(player_pos, player_rad, bounds.min, bounds.max))
				{
					player_pos.x -= movement.x;
					player_pos.z -= movement.z;
				}

			}
			
			

			
			cam.pos = player_pos + vf3d(0, player_height, 0);
		}

	}

	void objectHorzCollision(float dt)
	{
		vf2d vPotentialPosition;
		vPotentialPosition.x = cam.pos.x + player_vel.x * dt;
		vPotentialPosition.y = cam.pos.z + player_vel.z * dt;
		vf2d player_pos; player_pos.x = cam.pos.x; player_pos.y = cam.pos.z;
		
		//player vs object
		for (auto& obj : objectlist)
		{
			vf2d obj_pos; obj_pos.x = obj.translation.x; obj_pos.y = obj.translation.z;

            if ((player_pos - obj_pos).mag() <= (player_rad + obj.object_rad) * (player_rad + obj.object_rad))
			{
				float fDistance = (player_pos - obj_pos).mag();
				float fOverlap = 1.0f * (fDistance - obj.object_rad - player_rad);

				//object will always give way to target
				vPotentialPosition -= (obj_pos - player_pos) / fDistance * fOverlap;

				obj.translation.x += (player_pos.x - obj_pos.x) / fDistance * fOverlap;
				obj.translation.z += (player_pos.y - obj_pos.y) / fDistance * fOverlap;
			}
		}
	}



	void sceneHorzCollision( float dt)
	{
		

		
	}

#pragma endregion




	void userUpdate(float dt) {

		updateCameraMatrixes();
		updateCameraRay();
		handleUserInput(dt);
		updateGui(dt);

		

		for (auto& obj : scene_objects)
		{
			if (obj.isbillboard)
			{
				updateBillboard(obj, dt);
				
			}

			
		}

		for (auto& obj : objectlist)
		{
			updateBillboard(obj, dt);
			if(player_camera)
				objectVertCollision(obj, dt);
		}

		objectHorzCollision(dt);
		sceneVertCollision( dt);
		//sceneHorzCollision(dt);
	
	
	}

#pragma region SCENE OBJECT RENDER 

	void renderLinemesh(const LineMesh& l, const mat4& model, const sg_color& col) {
		sg_apply_pipeline(line_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0] = l.vbuf;
		bind.index_buffer = l.ibuf;
		sg_apply_bindings(bind);

		vs_line_params_t vs_line_params{};
		mat4 mvp = mat4::mul(cam.view_proj, model);
		std::memcpy(vs_line_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_line_params, SG_RANGE(vs_line_params));

		fs_line_params_t fs_line_params{};
		fs_line_params.u_tint[0] = col.r;
		fs_line_params.u_tint[1] = col.g;
		fs_line_params.u_tint[2] = col.b;
		fs_line_params.u_tint[3] = col.a;
		sg_apply_uniforms(UB_fs_line_params, SG_RANGE(fs_line_params));

		sg_draw(0, 2 * l.lines.size(), 1);
	}

	void renderObjects(Object& obj) {
		sg_apply_pipeline(default_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0]=obj.mesh.vbuf;
		bind.index_buffer= obj.mesh.ibuf;
		bind.samplers[SMP_default_smp]=samplers.linear;
		bind.views[VIEW_default_tex] = obj.tex;
		sg_apply_bindings(bind);

		//pass transformation matrix
		mat4 mvp=mat4::mul(cam.view_proj, obj.model);
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_model, obj.model.m, sizeof(vs_params.u_model));
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//render entire texture.
		//fs_params_t fs_params{};
		//lighting test
		fs_params_t fs_params{};
		{

			fs_params.u_num_lights = lights.size();
			int idx = 0;
			for (const auto& l : lights)
			{
				fs_params.u_light_pos[idx][0] = l.pos.x;
				fs_params.u_light_pos[idx][1] = l.pos.y;
				fs_params.u_light_pos[idx][2] = l.pos.z;
				fs_params.u_light_col[idx][0] = l.col.r;
				fs_params.u_light_col[idx][1] = l.col.g;
				fs_params.u_light_col[idx][2] = l.col.b;
				idx++;
			}
		}

		fs_params.u_view_pos[0] = cam.pos.x;
		fs_params.u_view_pos[1] = cam.pos.y;
		fs_params.u_view_pos[2] = cam.pos.z;
	
		{
			fs_params.u_tint[0] = 1;
			fs_params.u_tint[1] = 1;
			fs_params.u_tint[2] = 1;
			fs_params.u_tint[3] = 1;
		}
		

		fs_params.u_tl[0]=0, fs_params.u_tl[1]=0;
		fs_params.u_br[0]=1, fs_params.u_br[1]=1;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3* obj.mesh.tris.size(), 1);
	}
	
	void renderBillboard(Object& obj) {
		sg_apply_pipeline(default_pip);
		sg_bindings bind{};
		bind.vertex_buffers[0]= obj.mesh.vbuf;
		bind.index_buffer= obj.mesh.ibuf;
		bind.samplers[SMP_default_smp]=samplers.linear;
		bind.views[VIEW_default_tex]= obj.tex;
		sg_apply_bindings(bind);

		//pass transformation matrix
		mat4 mvp=mat4::mul(cam.view_proj, obj.model);
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//which region of texture to sample?

		fs_params_t fs_params{};
		int row= obj.anim/ obj.num_x;
		int col= obj.anim% obj.num_x;
		float u_left=col/float(obj.num_x);
		float u_right=(1+col)/float(obj.num_x);
		float v_top=row/float(obj.num_y);
		float v_btm=(1+row)/float(obj.num_y);
		fs_params.u_tl[0]=u_left;
		fs_params.u_tl[1]=v_top;
		fs_params.u_br[0]=u_right;
		fs_params.u_br[1]=v_btm;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3* obj.mesh.tris.size(), 1);
	}

	void render_Quad()
	{
		//separate animation stuff later
		

		int row = gGui.anim / gGui.num_x;
		int col = gGui.anim % gGui.num_x;
		float u_left = col / float(gGui.num_x);
		float u_right = (1 + col) / float(gGui.num_x);
		float v_top = row / float(gGui.num_y);
		float v_btm = (1 + row) / float(gGui.num_y);

		sg_apply_pipeline(gGui.pip);

		gGui.bind.views[VIEW_texview_tex] = gGui.gui_image;
		sg_apply_bindings(gGui.bind);

		fs_texview_params_t fs_tex_params{};
		fs_tex_params.u_tl[0] = u_left;
		fs_tex_params.u_tl[1] = v_top;
		fs_tex_params.u_br[0] = u_right;
		fs_tex_params.u_br[1] = v_btm;
		
	
		
		fs_tex_params.u_tint[0] = 1.0f;
		fs_tex_params.u_tint[1] = 1.0f;
		fs_tex_params.u_tint[2] = 1.0f;
		fs_tex_params.u_tint[3] = 1.0f;

		sg_apply_uniforms(UB_fs_texview_params, SG_RANGE(fs_tex_params));
		sg_apply_viewport(2, 2, 100, 100, true);


		//4 verts = 1quad
		sg_draw(0, 4, 1);

	}

	void renderobjectOutlines(Object& obj)
	{
		//for (const auto& obj : objectlist)
		{
			renderLinemesh(obj.linemesh, obj.model, { 1,1,1,1 });
		}
	}

	

#pragma endregion

#pragma region FONT RENDER

	void renderChar(const cmn::Font& f, float x, float y, char c, float scl, sg_color tint) {
		sg_apply_pipeline(fonts.colorview_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0] = fonts.vbuf;
		bind.samplers[SMP_u_fontview_smp] = samplers.nearest;
		bind.views[VIEW_u_fontview_tex] = f.tex;
		sg_apply_bindings(bind);

		vs_fontview_params_t vs_fontview_params{};
		f.getRegion(c,
			vs_fontview_params.u_tl[0],
			vs_fontview_params.u_tl[1],
			vs_fontview_params.u_br[0],
			vs_fontview_params.u_br[1]
		);
		sg_apply_uniforms(UB_vs_fontview_params, SG_RANGE(vs_fontview_params));

		fs_fontview_params_t fs_fontview_params{};
		fs_fontview_params.u_tint[0] = tint.r;
		fs_fontview_params.u_tint[1] = tint.g;
		fs_fontview_params.u_tint[2] = tint.b;
		fs_fontview_params.u_tint[3] = tint.a;
		sg_apply_uniforms(UB_fs_fontview_params, SG_RANGE(fs_fontview_params));

		sg_apply_viewportf(x, y, scl * f.char_w, scl * f.char_h, true);

		sg_draw(0, 4, 1);
	}

	void renderString(const cmn::Font& f, float x, float y, const std::string& str, float scl, sg_color tint) {
		int ox = 0, oy = 0;
		for (const auto& c : str) {
			if (c == '\n') ox = 0, oy += f.char_h;
			//tabsize=2
			else if (c == '\t') ox += 2 * f.char_w;
			else if (c >= 32 && c <= 127) {
				renderChar(f, x + scl * ox, y + scl * oy, c, scl, tint);
				ox += f.char_w;
			}
		}
	}

	void renderFonts()
	{
		std::string s = "dist: " + std::to_string(obj_index);
		input_text.str = s;

		renderString(
			*input_text.font,
			10,
			10,
			input_text.str,
			input_text.scl,
			input_text.tint
		);

		std::string min_text = "min.x: " + std::to_string(min.x) + "min.z: " + std::to_string(min.y);
		input_text.str = min_text;

		renderString(
			*input_text.font,
			10,
			25,
			input_text.str,
			input_text.scl,
			input_text.tint
		);

		std::string max_text = "max.x: " + std::to_string(max.x) + "max.z: " + std::to_string(max.y);
		input_text.str = max_text;

		renderString(
			*input_text.font,
			10,
			35,
			input_text.str,
			input_text.scl,
			input_text.tint
		);

	}

#pragma endregion

#pragma region SKYBOX RENDER

	void renderSkybox()
	{
		//imagine camera at origin
		mat4 look_at = mat4::makeLookAt({ 0,0,0 }, cam.dir, { 0,1,0 });
		mat4 view = mat4::inverse(look_at);
		mat4 view_proj = mat4::mul(cam.proj, view);

		for (int i = 0; i < 6; i++)
		{
			auto& obj = skybox.faces[i];

			sg_apply_pipeline(skybox.pip);

			sg_bindings bind{};
			bind.samplers[SMP_skybox_smp] = samplers.linear;
			bind.vertex_buffers[0] = obj.mesh.vbuf;
			bind.index_buffer = obj.mesh.ibuf;
			bind.views[VIEW_skybox_tex] = obj.tex;
			sg_apply_bindings(bind);

			vs_skybox_params_t vs_skybox_params{};
			mat4 mvp = mat4::mul(view_proj, obj.model);
			std::memcpy(vs_skybox_params.u_mvp, mvp.m, sizeof(vs_skybox_params.u_mvp));
			sg_apply_uniforms(UB_vs_skybox_params, SG_RANGE(vs_skybox_params));

			sg_draw(0, 3 * obj.mesh.tris.size(), 1);

		}
	}

#pragma endregion




	void userRender() {


		
		//scene rendering
		{
			
			sg_pass pass{};
			pass.action = render.pass_action;
			pass.attachments.colors[0] = render.color_attach;
			pass.attachments.depth_stencil = render.depth_attach;
			sg_begin_pass(pass);
		
			renderSkybox();
		
			
			
		
			for (auto& obj : scene_objects)
			{
				if (render_outlines)
				{
		
					renderobjectOutlines(obj);
				}
				else
				{
		
					renderObjects(obj);
				}
		
			}
		
			for (auto& obj : objectlist)
			{
				if (render_outlines)
				{
					renderobjectOutlines(obj);
				}
				else
				{
					renderObjects(obj);
				}
			}
		
			render_Quad();
		
			renderFonts();
		
			sg_end_pass();
		
			
		}

		//display
		{
			sg_pass pass{};
			pass.action = render.pass_action;
			pass.swapchain = sglue_swapchain();
			sg_begin_pass(pass);

			sg_apply_pipeline(process.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0] = process.vbuf;
			bind.samplers[SMP_u_process_smp] = samplers.linear;
			bind.views[VIEW_u_process_tex] = render.color_tex;
			sg_apply_bindings(bind);

			sg_draw(0, 4, 1);

			sg_end_pass();
		}
		
		
		sg_commit();
	}
};