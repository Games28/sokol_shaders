
///basic tinted 3d shader///////////////////////////////

@vs vs_mesh

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

layout(binding = 0) uniform p_vs_mesh{
	mat4 u_mvp;
};

void main(){
	gl_Position = u_mvp * vec4(i_pos, 1);
}

@end

@fs fs_mesh

out vec4 o_frag_col;

layout(binding = 1) uniform p_fs_mesh{
	vec4 u_tint;
};

void main(){
	o_frag_col = u_tint;

}

@end

@program mesh vs_mesh fs_mesh

///////// shadowing and lighting mesh shader//////////////////////

@vs vs_shaded

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;
out vec3 norm;
out vec2 uv;

layout(binding = 0) uniform p_vs_shaded{
	mat4 u_model;
	mat4 u_mvp;
};

void main(){
	pos = (u_model * vec4(i_pos,1)).xyz;
	norm = normalize(mat3(u_model) * i_norm);
	uv = i_uv;
	gl_Position = u_mvp * vec4(i_pos, 1);
}

@end

@fs fs_shaded

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_shaded {
	vec2 u_tl;
	vec2 u_br;
	vec3 u_eye_pos;
	vec3 u_light_pos;
	float u_cam_near;
	float u_cam_far;
};

layout(binding=0) uniform texture2D b_shaded_tex;
layout(binding=0) uniform sampler b_shaded_smp;

layout(binding=1) uniform textureCube b_shaded_shadow_tex;
layout(binding=1) uniform sampler b_shaded_shadow_smp;

//int->float
float decodeFloat(vec4 enc) {
	return dot(enc, 1./vec4(1., 255., 65025., 16581375.));
}


void main() {
	vec3 N=normalize(norm);
	vec3 L=normalize(u_light_pos-pos);
	vec3 V=normalize(u_eye_pos-pos);
	vec3 R=reflect(-L, N);

	float amb_mag=.2;
	float diff_mag=.7*max(dot(L, N), 0);
	float spec_mag=.3*pow(max(dot(R, V), 0), 32);

	//is in shadow?
	vec4 rgba=texture(samplerCube(b_shaded_shadow_tex, b_shaded_shadow_smp), -L);
	float dist01=decodeFloat(rgba);
	float dist=u_cam_near+dist01*(u_cam_far-u_cam_near);
	if(length(u_light_pos-pos)>dist+.05) diff_mag=0, spec_mag=0;
	
	//base texture color
	vec4 col=texture(sampler2D(b_shaded_tex, b_shaded_smp), u_tl + uv * (u_br - u_tl));
	//white specular
	vec3 spec=spec_mag*vec3(1, 1, 1);

	o_frag_col=vec4(spec+col.rgb*(amb_mag+diff_mag), 1);
}

@end

@program shaded vs_shaded fs_shaded


///////// color shader //////////////////

@vs vs_colorview

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform p_vs_colorview {
	vec2 u_tl;
	vec2 u_br;
};

void main() {
	uv=u_tl+i_uv*(u_br-u_tl);
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_colorview

in vec2 uv;

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_colorview {
	vec4 u_tint;
};

layout(binding=0) uniform texture2D b_colorview_tex;
layout(binding=0) uniform sampler b_colorview_smp;

void main() {
	vec4 col=texture(sampler2D(b_colorview_tex, b_colorview_smp), uv);
	o_frag_col=u_tint*col;
}

@end

@program colorview vs_colorview fs_colorview


/////////texview shader ////////////////////////////////////

@vs vs_texview

in vec2 v_pos;
in vec2 v_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(v_pos, .5, 1);
	uv.x = v_uv.x;
	uv.y = -v_uv.y;
}

@end

@fs fs_texview

layout(binding = 0) uniform texture2D texview_tex;
layout(binding = 0) uniform sampler texview_smp;

layout(binding = 0) uniform fs_texview_params
{
	float brightness;
	vec4 u_tint;
	vec2 u_tl;
	vec2 u_br;
};

in vec2 uv;

out vec4 frag_color;

float fmod(float x, float y){
	return x - y * trunc(x / y);
}

struct HSV{
float hue;
float saturation, value;

};

vec4 hsv2rgb(int hue, float saturation, float value)
{
		float c = value * saturation;
		float x = c * (1 - abs(1 - fmod(hue/ 60.0, 2)));
		float m = value - c;
		float r = 0, g = 0, b = 0;
		switch(hue/60) {
		case 0: r=c, g=x, b=0; break;
		case 1: r=x, g=c, b=0; break;
		case 2: r=0, g=c, b=x; break;
		case 3: r=0, g=x, b=c; break;
		case 4: r=x, g=0, b=c; break;
		case 5: r=c, g=0, b=x; break;
	}
	
	
	r += m;
	g += m;
	b += m;

	return vec4(r, g, b, 1);
	
}

HSV rgb2hsv(float r, float g, float b, float brightness)
{
	HSV hsv;
	float R = r / 255;
		float G = g / 255;
		float B = b / 255;

		float cmax = max(R, max(G, B));
		float cmin = min(R, min(G, B));

		float diff = cmax - cmin;

		hsv.hue = 0, hsv.saturation = 0, hsv.value = 0;

		if (cmax == cmin) { hsv.hue = 0; }
		else if(cmax == R) { hsv.hue = fmod(60 * ((G - B) / diff) + 360, 360); }
		else if(cmax == G) { hsv.hue = fmod(60 * ((B - R) / diff) + 120, 360); }
		else if(cmax == B) { hsv.hue = fmod(60 * ((R - G) / diff) + 240, 360); }

		if (cmax == 0)
			hsv.saturation = 0;
		else
			hsv.saturation = (diff / cmax) * 100;

		hsv.value = cmax  * (100 * brightness);

		//hsv.value = hsv.value * brightness;

		//hsv.value = cmax * brightness;

		return hsv;
		
}


void main()
{

	vec4 base_col = texture(sampler2D(texview_tex, texview_smp), u_tl + uv * (u_br - u_tl));

	HSV hsv = rgb2hsv(base_col.r,base_col.g,base_col.b,brightness);

	vec4 new_col = hsv2rgb(int(hsv.hue),hsv.saturation,hsv.value);


	frag_color = u_tint * new_col;
}

@end

@program texview vs_texview fs_texview


///////////////// linemesh shader ///////////////////////////////////

@vs vs_line

in vec3 i_pos;
in vec4 i_col;

out vec4 col;

layout(binding = 0) uniform vs_line_params {
	mat4 u_mvp;
};

void main()
{
	col = i_col;
	gl_Position = u_mvp * vec4(i_pos, 1);
}

@end

@fs fs_line

in vec4 col;

out vec4 o_frag_col;

layout(binding = 1) uniform fs_line_params{
	vec4 u_tint;
};

void main()
{
	o_frag_col = u_tint * col;
}

@end

@program line vs_line fs_line

//////////////// FONT VIEW SHADER ///////////////////////

@vs vs_fontview

layout(binding = 0) uniform vs_fontview_params{
	vec2 u_tl;
	vec2 u_br;
};

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main(){
	uv = u_tl + i_uv * (u_br - u_tl);
	gl_Position = vec4(i_pos, .5, 1);
}

@end

@fs fs_fontview

layout(binding = 1) uniform fs_fontview_params{
	vec4 u_tint;
};

layout(binding = 0) uniform texture2D u_fontview_tex;
layout(binding = 0) uniform sampler u_fontview_smp;

in vec2 uv;

out vec4 o_frag_col;

void main(){
	o_frag_col = u_tint * texture(sampler2D(u_fontview_tex, u_fontview_smp), uv);
}

@end

@program fontview vs_fontview fs_fontview

/////////////skybox shader//////////////////////////////
//depth-avoided textured 3d shader

@vs vs_skybox

in vec3 i_pos;
in vec2 i_uv;

out vec2 uv;

layout(binding=0) uniform p_vs_skybox {
	mat4 u_mvp;
};

void main() {
	uv=i_uv;
	//z component=depth=1
	vec4 pos=u_mvp*vec4(i_pos, 1);
	gl_Position=pos.xyww;
}

@end

@fs fs_skybox

in vec2 uv;

out vec4 o_frag_col;

layout(binding=0) uniform texture2D b_skybox_tex;
layout(binding=0) uniform sampler b_skybox_smp;

void main() {
	o_frag_col=texture(sampler2D(b_skybox_tex, b_skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox


//encode shadows shader////////////////////////

@vs vs_shadow_map

in vec3 i_pos;
in vec3 i_norm;
in vec2 i_uv;

out vec3 pos;

layout(binding=0) uniform p_vs_shadow_map {
	mat4 u_model;
	mat4 u_mvp;
};

void main() {
	pos=(u_model*vec4(i_pos, 1)).xyz;
	gl_Position=u_mvp*vec4(i_pos, 1);
}

@end

@fs fs_shadow_map

in vec3 pos;

out vec4 o_frag_col;

layout(binding=1) uniform p_fs_shadow_map {
	vec3 u_light_pos;
	float u_cam_near;
	float u_cam_far;
};

//float->int
vec4 encodeFloat(float f) {
	vec4 enc=fract(f*vec4(1., 255., 65025., 16581375.));
	return enc-enc.yzwx*vec4(1./255., 1./255., 1./255., 0.);
}

void main() {
	float dist=length(u_light_pos-pos);
	float dist01=(dist-u_cam_near)/(u_cam_far-u_cam_near);
	o_frag_col=encodeFloat(dist01);
}

@end

@program shadow_map vs_shadow_map fs_shadow_map









