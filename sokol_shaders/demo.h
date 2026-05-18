#define SOKOL_GLCORE
#include "sokol_engine.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include <iostream>

#include "shd.glsl.h"
 #include "scene_shd.glsl.h"
//#include "window_shd.glsl.h"
//#include "object_shd.glsl.h"

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
	const float near_plane = 0.01f, far_plane = 1000;
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

	struct {
		sg_sampler linear{};
		sg_sampler nearest{};
		sg_sampler scene{};

	}samplers;

	struct {
		sg_view blank{};
		sg_view uv{};
		sg_view checker{};

	}textures;

	sg_pipeline mesh_pip{};
	sg_pipeline shaded_mesh_pip{};
	sg_pipeline line_pip{};

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};
	} colorview;

	struct {
		sg_pass_action pass_action{};
		sg_pipeline attach_pip{};

		sg_view color_attach[6];
		sg_view depth_attach{};
		sg_view color_view{};

		vf3d pos{ 0, 2, 0 };

		//only works for GL...
		const vf3d face_sys[6][3]{
			{{0, 0, -1}, {0, -1, 0}, {-1, 0, 0}},//px
			{{0, 0, 1}, {0, -1, 0}, {1, 0, 0}},//nx
			{{1, 0, 0}, {0, 0, 1}, {0, -1, 0}},//py
			{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},//ny
			{{1, 0, 0}, {0, -1, 0}, {0, 0, -1}},//px
			{{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}}//nz
		};

		mat4 view[6];

		mat4 face_proj;
	} shadow_map;


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

	struct {
		sg_pass_action pass_action{};

		sg_image color_img{ SG_INVALID_ID };
		sg_view color_attach{ SG_INVALID_ID };
		sg_view color_tex{ SG_INVALID_ID };

		sg_image depth_img{ SG_INVALID_ID };
		sg_view depth_attach{ SG_INVALID_ID };
	} canvas;

	struct {
		sg_pipeline pip{};

		sg_buffer vbuf{};

		mat4 model[6];

		sg_view tex[6];
	} skybox;


	struct {
		sg_pass_action pass_action{};

		std::list<sg_pipeline> pips;
		sg_pipeline* pip_to_use;

		sg_buffer vbuf{};

		float time = 0;
	} post_process;

	
	const std::vector<std::string> Structurefilenames{
		"assets/models/lakeside.txt",
		"assets/models/watering.txt",
		"assets/models/house.txt"
	};

	const std::vector<std::string> texturefilenames{
		"assets/grass.png",
		"assets/watersprite.png",
		"assets/colorstone.png"
	};

	const std::vector<std::string> spritefileNames = {
		"assets/sprites/probidletest.png",
		"assets/sprites/r2idletest.png",
		"assets/sprites/trooper.png",
		"assets/sprites/fps_fireball1.png"
	};


	std::vector<Object> objectlist;

	bool  render_outlines = false;

	float brightness = .5f;

#pragma region SETUP HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment = sglue_environment();
		sg_setup(desc);
	}

	void setupSamplers() {
		{
			sg_sampler_desc sampler_desc{};
			samplers.scene = sg_make_sampler(sampler_desc);

		}

		{
			sg_sampler_desc sampler_desc{};
			sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
			sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
			samplers.linear = sg_make_sampler(sampler_desc);
		}

		{
			sg_sampler_desc sampler_desc{};
			sampler_desc.min_filter = SG_FILTER_NEAREST;
			sampler_desc.mag_filter = SG_FILTER_NEAREST;
			samplers.nearest = sg_make_sampler(sampler_desc);
		}
	}

	//"primitive" textures to work with
	void setupTextures() {
		auto& b = textures.blank;
		b = cmn::makeBlankTexture();

		textures.uv = cmn::makeUVTexture(1024, 1024);

		auto& c = textures.checker;
		if (!cmn::bmakeTextureFromFile(c, "assets/img/checker.png")) c = b;
	}

	void setupMeshPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_mesh_i_pos].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_mesh_i_norm].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_mesh_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(mesh_shader_desc(sg_query_backend()));
		pip_desc.index_type = SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode = SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled = true;
		pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
		mesh_pip = sg_make_pipeline(pip_desc);
	}

	//works with mesh
	void setupShadedMeshPipeline() {
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_shaded_i_pos].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shaded_i_norm].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_shaded_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(shaded_shader_desc(sg_query_backend()));
		pip_desc.index_type = SG_INDEXTYPE_UINT32;
		pip_desc.cull_mode = SG_CULLMODE_FRONT;
		pip_desc.depth.write_enabled = true;
		pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
		shaded_mesh_pip = sg_make_pipeline(pip_desc);
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
		pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
		line_pip = sg_make_pipeline(pip_desc);
	}

	void setupColorview() {
		//2d tristrip pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_colorview_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_colorview_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(colorview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		//with alpha blending
		pip_desc.colors[0].blend.enabled = true;
		pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
		pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
		pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		colorview.pip = sg_make_pipeline(pip_desc);

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
		colorview.vbuf = sg_make_buffer(vbuf_desc);
	}

	void setup_Quad()
	{
		//2d on screen texture
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_texview_v_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_texview_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(texview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		pip_desc.depth.write_enabled = true;
		pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;

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
		gGui.bind.samplers[SMP_texview_smp] = samplers.scene;

		gGui.gui_image = getTexture("assets/pistolidle.png");

		//setup texture animatons
		gGui.num_x = 1; gGui.num_y = 1;
		gGui.num_ttl = gGui.num_x * gGui.num_y;


	}

	void setupFonts() {
		fonts.fancy = cmn::Font("assets/img/font/fancy_8x16.png", 8, 16);
		fonts.monogram = cmn::Font("assets/img/font/monogram_6x9.png", 6, 9);
		fonts.round = cmn::Font("assets/img/font/round_6x6.png", 6, 6);
	}

	//make pipeline, make render targets, & orient view matrixes
	void setupShadowMap() {
		//clear to black
		shadow_map.pass_action.depth.load_action = SG_LOADACTION_CLEAR;
		shadow_map.pass_action.depth.clear_value = 1;
		shadow_map.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		shadow_map.pass_action.colors[0].clear_value = { 0, 0, 0, 1 };

		{
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_shadow_map_i_pos].format = SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_shadow_map_i_norm].format = SG_VERTEXFORMAT_FLOAT3;
			pip_desc.layout.attrs[ATTR_shadow_map_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(shadow_map_shader_desc(sg_query_backend()));
			pip_desc.index_type = SG_INDEXTYPE_UINT32;
			pip_desc.face_winding = SG_FACEWINDING_CCW;
			pip_desc.cull_mode = SG_CULLMODE_BACK;
			pip_desc.depth.write_enabled = true;
			pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
			pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
			shadow_map.attach_pip = sg_make_pipeline(pip_desc);
		}

		//cube attach and view
		{
			sg_image_desc image_desc{};
			image_desc.type = SG_IMAGETYPE_CUBE;
			image_desc.usage.color_attachment = true;
			image_desc.width = 2048;
			image_desc.height = 2048;
			sg_image cube_img = sg_make_image(image_desc);

			for (int i = 0; i < 6; i++) {
				sg_view_desc view_desc{};
				view_desc.color_attachment.image = cube_img;
				view_desc.color_attachment.slice = i;
				shadow_map.color_attach[i] = sg_make_view(view_desc);
			}

			sg_view_desc view_desc{};
			view_desc.texture.image = cube_img;
			shadow_map.color_view = sg_make_view(view_desc);
		}

		{
			//make depth attachment
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment = true;
			image_desc.width = 2048;
			image_desc.height = 2048;
			image_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
			sg_image depth_img = sg_make_image(image_desc);
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image = depth_img;
			shadow_map.depth_attach = sg_make_view(view_desc);
		}

		//make projection matrix
		shadow_map.face_proj = mat4::makePerspective(90.f, 1, cam.near_plane, cam.far_plane);
	}

	//since will be called on resize,
	//  this needs to free & remake resources
	void resizeCanvasTarget() {
		//make color img
		{
			sg_destroy_image(canvas.color_img);
			sg_image_desc image_desc{};
			image_desc.usage.color_attachment = true;
			image_desc.width = sapp_width();
			image_desc.height = sapp_height();
			canvas.color_img = sg_make_image(image_desc);

			//make color attach
			{
				sg_destroy_view(canvas.color_attach);
				sg_view_desc view_desc{};
				view_desc.color_attachment.image = canvas.color_img;
				canvas.color_attach = sg_make_view(view_desc);
			}

			//make color tex
			{
				sg_destroy_view(canvas.color_tex);
				sg_view_desc view_desc{};
				view_desc.texture.image = canvas.color_img;
				canvas.color_tex = sg_make_view(view_desc);
			}
		}

		{
			//make depth img
			sg_destroy_image(canvas.depth_img);
			sg_image_desc image_desc{};
			image_desc.usage.depth_stencil_attachment = true;
			image_desc.width = sapp_width();
			image_desc.height = sapp_height();
			image_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
			canvas.depth_img = sg_make_image(image_desc);

			//make depth attach
			sg_destroy_view(canvas.depth_attach);
			sg_view_desc view_desc{};
			view_desc.depth_stencil_attachment.image = canvas.depth_img;
			canvas.depth_attach = sg_make_view(view_desc);
		}
	}
	sg_view getTexture(const std::string& filename) {
		sg_view tex;
		auto status = cmn::makeTextureFromFile(tex, filename);
		if (!status.valid) tex = textures.uv;
		return tex;
	}

	void setupCanvas() {
		resizeCanvasTarget();

		//clear to black
		canvas.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		canvas.pass_action.colors[0].clear_value = { 0, 0, 0, 1 };
	}

	//make pipeline, orient meshes, & load textures 
	void setupSkybox() {
		//pipeline
			//pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_skybox_i_pos].format = SG_VERTEXFORMAT_FLOAT3;
		pip_desc.layout.attrs[ATTR_skybox_i_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(skybox_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		pip_desc.depth.write_enabled = true;
		pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
		pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
		skybox.pip = sg_make_pipeline(pip_desc);

		//vertex buffer
		{
			//xyzuv
			float vertexes[4][5]{
				{-1, 0, -1, 0, 0},
				{1, 0, -1, 1, 0},
				{-1, 0, 1, 0, 1},
				{1, 0, 1, 1, 1}
			};
			sg_buffer_desc buffer_desc{};
			buffer_desc.data = SG_RANGE(vertexes);
			skybox.vbuf = sg_make_buffer(buffer_desc);
		}

		//orient faces
		const vf3d rot_trans[6][2]{
			{cmn::Pi * vf3d(.5f, -.5f, 0), {1, 0, 0}},
			{cmn::Pi * vf3d(.5f, .5f, 0), {-1, 0, 0}},
			{cmn::Pi * vf3d(0, 0, 1), {0, 1, 0}},
			{cmn::Pi * vf3d(1, 0, 1), {0, -1, 0}},
			{cmn::Pi * vf3d(.5f, 1, 0), {0, 0, 1}},
			{cmn::Pi * vf3d(.5f, 0, 0), {0, 0, -1}}
		};
		for (int i = 0; i < 6; i++) {
			mat4 rot_x = mat4::makeRotX(rot_trans[i][0].x);
			mat4 rot_y = mat4::makeRotY(rot_trans[i][0].y);
			mat4 rot_z = mat4::makeRotZ(rot_trans[i][0].z);
			mat4 rot = mat4::mul(rot_z, mat4::mul(rot_y, rot_x));
			mat4 trans = mat4::makeTranslation(rot_trans[i][1]);
			skybox.model[i] = mat4::mul(trans, rot);
		}

		//textures
		const std::string filenames[6]{
			"assets/img/skybox/px.png",
			"assets/img/skybox/nx.png",
			"assets/img/skybox/py.png",
			"assets/img/skybox/ny.png",
			"assets/img/skybox/pz.png",
			"assets/img/skybox/nz.png"
		};
		for (int i = 0; i < 6; i++) {
			sg_view& tex = skybox.tex[i];
			if (!cmn::bmakeTextureFromFile(tex, filenames[i])) tex = textures.blank;
		}
	
	}

	void setupSceneObjects()
	{
		std::vector<vf3d> coords = {
			{-150,-18,100},
			{0,-2,0},
			{25,0,5},
			//{-14,-2,-14},
		};

		std::vector<vf3d> scales =
		{
			{1,1,1},
			{12,1,12},
			{1,1,1}
		};
		for (int i = 0; i < coords.size(); i++)
		{
			Object b;
			Mesh& m = b.mesh;
			auto status = Mesh::loadFromOBJ(m, Structurefilenames[i]);
			if (!status.valid) m = Mesh::makeCube();
			b.scale = scales[i];
			b.translation = coords[i];
			b.updateMatrixes();
			b.tex = getTexture(texturefilenames[i]);
			b.addLineMesh();
			objectlist.push_back(b);


		}
	}

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


	void randomizePostProcess() {
		auto it = post_process.pips.begin();
		std::advance(it, std::rand() % post_process.pips.size());
		post_process.pip_to_use = &*it;
	}

	void setupPostProcess() {
		post_process.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
		post_process.pass_action.colors[0].clear_value = { 0, 0, 0, 1 };

		{//identity
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_identity_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(identity_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
		}

		{//crt
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_crt_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(crt_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
			//default to this
			post_process.pip_to_use = &post_process.pips.back();
		}

		{//halftone
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_halftone_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(halftone_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
			
		}

		{//crosshatch
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_crosshatch_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(crosshatch_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
		}

		{//ascii
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_ascii_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(ascii_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
		}

		{//kuwahara
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_kuwahara_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(kuwahara_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
		}

		{//mean of least variance
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_mlv_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(mlv_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
		}

		{//quantize
			sg_pipeline_desc pip_desc{};
			pip_desc.layout.attrs[ATTR_quantize_i_pos].format = SG_VERTEXFORMAT_FLOAT2;
			pip_desc.shader = sg_make_shader(quantize_shader_desc(sg_query_backend()));
			pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
			post_process.pips.push_back(sg_make_pipeline(pip_desc));
		}

		//xy
		float vertexes[4][2]{
			{-1, -1},
			{1, -1},
			{-1, 1},
			{1, 1}
		};
		sg_buffer_desc buffer_desc{};
		buffer_desc.data = SG_RANGE(vertexes);
		post_process.vbuf = sg_make_buffer(buffer_desc);
	}

	void setupCamera() {
		//place camera 3-4 units away from origin
		float dx = 1 - 2 * cmn::randFloat();
		float dy = cmn::randFloat();
		float dz = 1 - 2 * cmn::randFloat();
		float dist = cmn::randFloat(3, 4);
		cam.pos = dist * vf3d(dx, dy, dz).norm();

		//point towards origin
		cartesianToPolar(-cam.pos, cam.yaw, cam.pitch);
	}

#pragma endregion
	


	void userCreate() override {
		
		setupEnvironment();
		setupSamplers();
		setupTextures();
		setupShadedMeshPipeline();
		setupMeshPipeline();
		setupLinePipeline();
		setupColorview();
		setupFonts();

		setupShadowMap();
		setupCanvas();

		setupSkybox();

		setupSceneObjects();
		setupObjectBillboards();
		setup_Quad();

		setupPostProcess();

		setupCamera();
		
	}




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


	void userInput(const sapp_event* e) override {
		switch (e->type) {
		case SAPP_EVENTTYPE_RESIZED:
			resizeCanvasTarget();
			break;
		}
	}

	void handleCameraLooking(float dt) {
	

		//left/right
		if (getKey(SAPP_KEYCODE_LEFT).held) cam.yaw += dt;
		if (getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw -= dt;

		//up/down
		if (getKey(SAPP_KEYCODE_UP).held) cam.pitch += dt;
		if (getKey(SAPP_KEYCODE_DOWN).held) cam.pitch -= dt;

		//clamp camera pitch
		if (cam.pitch > cmn::Pi / 2) cam.pitch = cmn::Pi / 2 - .001f;
		if (cam.pitch < -cmn::Pi / 2) cam.pitch = .001f - cmn::Pi / 2;
	}

	void handleCameraMovement(float dt) {
	

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


	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		cam.dir = polarToCartesian(cam.yaw, cam.pitch);

		handleCameraMovement(dt);


		//look at origin
		if (getKey(SAPP_KEYCODE_HOME).held) cartesianToPolar(-cam.pos, cam.yaw, cam.pitch);

		//set light pos
		if (getKey(SAPP_KEYCODE_L).pressed) shadow_map.pos = cam.pos;

		if (getKey(SAPP_KEYCODE_0).held) brightness -= 1.0f * dt;
		if (getKey(SAPP_KEYCODE_1).held) brightness += 1.0f * dt;

		//toggle shape outlines
		if (getKey(SAPP_KEYCODE_O).pressed) render_outlines ^= true;
	}

	void updateCameraMatrixes() {
		mat4 look_at = mat4::makeLookAt(cam.pos, cam.pos + cam.dir, { 0, 1, 0 });
		cam.view = mat4::inverse(look_at);

		//cam proj can change with window resize
		float asp = sapp_widthf() / sapp_heightf();
		cam.proj = mat4::makePerspective(60, asp, cam.near_plane, cam.far_plane);

		cam.view_proj = mat4::mul(cam.proj, cam.view);
	}

	//update faces' view matrix w/ shadow_map pos
	void updateShadowMapMatrixes() {
		for (int i = 0; i < 6; i++) {
			const auto& x = shadow_map.face_sys[i][0];
			const auto& y = shadow_map.face_sys[i][1];
			const auto& z = shadow_map.face_sys[i][2];
			mat4 sys = makeTransformMatrix(x, y, z, shadow_map.pos);
			shadow_map.view[i] = mat4::inverse(sys);
		}
	}



	void userUpdate(float dt) override{

		handleUserInput(dt);

		updateCameraMatrixes();

		updateShadowMapMatrixes();

		for (auto& obj : objectlist)
		{
			if (obj.isbillboard)
				updateBillboard(obj, dt);
		}

		if (getKey(SAPP_KEYCODE_R).pressed) randomizePostProcess();

		post_process.time += dt;
		
	
	
	}


	void renderLinemesh(const LineMesh& l, const mat4& model, const sg_color& col) {
		sg_apply_pipeline(line_pip);

		sg_bindings bind{};
		bind.vertex_buffers[0] = l.vbuf;
		bind.index_buffer = l.ibuf;
		sg_apply_bindings(bind);

		vs_line_params_t p_vs_line{};
		mat4 mvp = mat4::mul(cam.view_proj, model);
		std::memcpy(p_vs_line.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_line_params, SG_RANGE(p_vs_line));

		fs_line_params_t p_fs_line{};
		p_fs_line.u_tint[0] = col.r;
		p_fs_line.u_tint[1] = col.g;
		p_fs_line.u_tint[2] = col.b;
		p_fs_line.u_tint[3] = col.a;
		sg_apply_uniforms(UB_fs_line_params, SG_RANGE(p_fs_line));

		sg_draw(0, 2 * l.lines.size(), 1);
	}


	void renderTex(float x, float y, float w, float h,
		const sg_view& tex, float l, float t, float r, float b,
		const sg_color& tint
	) {
		sg_apply_pipeline(colorview.pip);

		sg_bindings bind{};
		bind.vertex_buffers[0] = colorview.vbuf;
		bind.samplers[SMP_b_colorview_smp] = samplers.nearest;
		bind.views[VIEW_b_colorview_tex] = tex;
		sg_apply_bindings(bind);

		p_vs_colorview_t p_vs_colorview{};
		p_vs_colorview.u_tl[0] = l;
		p_vs_colorview.u_tl[1] = t;
		p_vs_colorview.u_br[0] = r;
		p_vs_colorview.u_br[1] = b;
		sg_apply_uniforms(UB_p_vs_colorview, SG_RANGE(p_vs_colorview));

		p_fs_colorview_t p_fs_colorview{};
		p_fs_colorview.u_tint[0] = tint.r;
		p_fs_colorview.u_tint[1] = tint.g;
		p_fs_colorview.u_tint[2] = tint.b;
		p_fs_colorview.u_tint[3] = tint.a;
		sg_apply_uniforms(UB_p_fs_colorview, SG_RANGE(p_fs_colorview));

		sg_apply_viewportf(x, y, w, h, true);

		sg_draw(0, 4, 1);
	}

	void renderChar(float x, float y, const cmn::Font& f, char c, float scl = 1, const sg_color& tint = { 1, 1, 1, 1 }) {
		float l, t, r, b;
		f.getRegion(c, l, t, r, b);
		renderTex(
			x, y, scl * f.char_w, scl * f.char_h,
			f.tex, l, t, r, b,
			tint
		);
	}

	void renderString(float x, float y, const cmn::Font& f, const std::string& str, float scl = 1, const sg_color& tint = { 1, 1, 1, 1 }) {
		int ox = 0, oy = 0;
		for (const auto& c : str) {
			if (c == '\n') ox = 0, oy += f.char_h;
			//tabsize=2
			else if (c == '\t') ox += 2 * f.char_w;
			else if (c >= 32 && c <= 127) {
				renderChar(x + scl * ox, y + scl * oy, f, c, scl, tint);
				ox += f.char_w;
			}
		}
	}


	void renderShapesIntoShadowMap() {
		for (int i = 0; i < 6; i++) {
			sg_pass pass{};
			pass.action = shadow_map.pass_action;
			pass.attachments.colors[0] = shadow_map.color_attach[i];
			pass.attachments.depth_stencil = shadow_map.depth_attach;
			sg_begin_pass(pass);

			mat4 view_proj = mat4::mul(shadow_map.face_proj, shadow_map.view[i]);
			for (const auto& shp : objectlist) {
				sg_apply_pipeline(shadow_map.attach_pip);

				sg_bindings bind{};
				bind.vertex_buffers[0] = shp.mesh.vbuf;
				bind.index_buffer = shp.mesh.ibuf;
				sg_apply_bindings(bind);

				mat4 mvp = mat4::mul(view_proj, shp.model);

				p_vs_shadow_map_t p_vs_shadow_map{};
				std::memcpy(p_vs_shadow_map.u_model, shp.model.m, sizeof(shp.model.m));
				std::memcpy(p_vs_shadow_map.u_mvp, mvp.m, sizeof(mvp.m));
				sg_apply_uniforms(UB_p_vs_shadow_map, SG_RANGE(p_vs_shadow_map));

				p_fs_shadow_map_t p_fs_shadow_map{};
				p_fs_shadow_map.u_light_pos[0] = shadow_map.pos.x;
				p_fs_shadow_map.u_light_pos[1] = shadow_map.pos.y;
				p_fs_shadow_map.u_light_pos[2] = shadow_map.pos.z;
				p_fs_shadow_map.u_cam_near = cam.near_plane;
				p_fs_shadow_map.u_cam_far = cam.far_plane;
				sg_apply_uniforms(UB_p_fs_shadow_map, SG_RANGE(p_fs_shadow_map));

				sg_draw(0, 3 * shp.mesh.tris.size(), 1);
			}

			sg_end_pass();
		}
	}

	void renderSkybox() {
		//view from eye at origin + camera projection
		mat4 look_at = mat4::makeLookAt({ 0, 0, 0 }, cam.dir, { 0, 1, 0 });
		mat4 view = mat4::inverse(look_at);
		mat4 view_proj = mat4::mul(cam.proj, view);

		for (int i = 0; i < 6; i++) {
			sg_apply_pipeline(skybox.pip);

			sg_bindings bind{};
			bind.vertex_buffers[0] = skybox.vbuf;
			bind.samplers[SMP_b_skybox_smp] = samplers.linear;
			bind.views[VIEW_b_skybox_tex] = skybox.tex[i];
			sg_apply_bindings(bind);

			mat4 mvp = mat4::mul(view_proj, skybox.model[i]);

			p_vs_skybox_t p_vs_skybox{};
			std::memcpy(p_vs_skybox.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_p_vs_skybox, SG_RANGE(p_vs_skybox));

			sg_draw(0, 4, 1);
		}
	}

	void renderObjects(Object& obj) {
		
			sg_apply_pipeline(shaded_mesh_pip);

			sg_bindings bind{};
			bind.vertex_buffers[0] = obj.mesh.vbuf;
			bind.index_buffer = obj.mesh.ibuf;
			bind.samplers[SMP_b_shaded_smp] = samplers.linear;
			bind.views[VIEW_b_shaded_tex] = obj.tex;
			bind.samplers[SMP_b_shaded_shadow_smp] = samplers.nearest;
			bind.views[VIEW_b_shaded_shadow_tex] = shadow_map.color_view;
			sg_apply_bindings(bind);

			p_vs_shaded_t p_vs_shaded{};
			std::memcpy(p_vs_shaded.u_model, obj.model.m, sizeof(obj.model.m));
			mat4 mvp = mat4::mul(cam.view_proj, obj.model);
			std::memcpy(p_vs_shaded.u_mvp, mvp.m, sizeof(mvp.m));
			sg_apply_uniforms(UB_p_vs_shaded, SG_RANGE(p_vs_shaded));

			p_fs_shaded_t p_fs_shaded{};
			p_fs_shaded.u_eye_pos[0] = cam.pos.x;
			p_fs_shaded.u_eye_pos[1] = cam.pos.y;
			p_fs_shaded.u_eye_pos[2] = cam.pos.z;
			p_fs_shaded.u_light_pos[0] = shadow_map.pos.x;
			p_fs_shaded.u_light_pos[1] = shadow_map.pos.y;
			p_fs_shaded.u_light_pos[2] = shadow_map.pos.z;
			p_fs_shaded.u_cam_near = cam.near_plane;
			p_fs_shaded.u_cam_far = cam.far_plane;
			sg_apply_uniforms(UB_p_fs_shaded, SG_RANGE(p_fs_shaded));

			sg_draw(0, 3 * obj.mesh.tris.size(), 1);
		
	}

	void renderObjectOutlines(Object& obj) {
		
			renderLinemesh(obj.linemesh, obj.model, { 1, 1, 1, 1 });
		
	}

	void renderIntoCanvas() {
		sg_pass pass{};
		pass.action = canvas.pass_action;
		pass.attachments.colors[0] = canvas.color_attach;
		pass.attachments.depth_stencil = canvas.depth_attach;
		sg_begin_pass(pass);

		renderSkybox();

		

		for (auto& obj : objectlist)
		{
			if (render_outlines) renderObjectOutlines(obj);
			else  renderObjects(obj);
		}

		
		render_Quad();


		sg_end_pass();
	}

	void renderPostProcess() {
		sg_pass pass{};
		pass.action = post_process.pass_action;
		pass.swapchain = sglue_swapchain();
		sg_begin_pass(pass);

		sg_apply_pipeline(*post_process.pip_to_use);

		sg_bindings bind{};
		bind.vertex_buffers[0] = post_process.vbuf;
		bind.samplers[SMP_b_crt_smp] = samplers.linear;
		bind.views[VIEW_b_crt_tex] = canvas.color_tex;
		sg_apply_bindings(bind);

		p_vs_post_process_t p_vs_post_process{};
		p_vs_post_process.u_time = post_process.time;
		p_vs_post_process.u_resolution[0] = sapp_widthf();
		p_vs_post_process.u_resolution[1] = sapp_heightf();
		sg_apply_uniforms(UB_p_vs_post_process, SG_RANGE(p_vs_post_process));

		sg_draw(0, 4, 1);

		sg_end_pass();
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



		fs_tex_params.brightness = brightness;

		fs_tex_params.u_tint[0] = 1.0f;
		fs_tex_params.u_tint[1] = 1.0f;
		fs_tex_params.u_tint[2] = 1.0f;
		fs_tex_params.u_tint[3] = 1.0f;

		sg_apply_uniforms(UB_fs_texview_params, SG_RANGE(fs_tex_params));
		sg_apply_viewport(sapp_width() / 2, sapp_height() - 200, 200, 200, true);


		//4 verts = 1quad
		sg_draw(0, 4, 1);

	}

	void userRender() override {

		renderShapesIntoShadowMap();

		renderIntoCanvas();
		

		renderPostProcess();

		

		sg_commit();
		
		
	}
};