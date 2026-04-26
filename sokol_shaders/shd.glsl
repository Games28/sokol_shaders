@vs vs_default

layout(binding=0) uniform vs_params {
    mat4 u_model;
	mat4 u_mvp;
};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec3 pos;
out vec3 norm;
out vec2 uv;

void main() {
    gl_Position=u_mvp*vec4(v_pos, 1);
    pos=(u_model*vec4(v_pos, 1)).xyz;
    norm=normalize(mat3(u_model)*v_norm);
    uv=v_uv;
}

@end

@fs fs_default

layout(binding=0) uniform texture2D default_tex;
layout(binding=0) uniform sampler default_smp;

layout(binding=1) uniform fs_params {
	vec4 u_tint;
	vec2 u_tl;
	vec2 u_br;
	vec3 u_view_pos;
	int u_num_lights;
	vec4 u_light_pos[16];
	vec4 u_light_col[16];

};

in vec3 pos;
in vec3 norm;
in vec2 uv;


out vec4 frag_color;

void main() {
	const float amb_mag=1;//0-1
	const vec3 amb_col=vec3(0.03);
	
	const float shininess=64;//32-256
	const float spec_mag=1;//0-1

	const float att_k0=1.0;
	const float att_k1=0.09;
	const float att_k2=0.032;

	//srgb->linear
	//vec3 base_col_srgb=texture(sampler2D(default_tex, default_smp),u_tl + uv * (u_br - u_tl)).rgb;
	vec3 base_col_srgb=texture(sampler2D(default_tex, default_smp), uv).rgb;
	vec3 base_col=pow(base_col_srgb, vec3(2.2));

	vec3 n=normalize(norm);
	vec3 v=normalize(u_view_pos- pos);

	//start with ambient
	vec3 col=amb_col*base_col*amb_mag;
	for(int i=0; i<u_num_lights; i++) {
		vec3 l_pos=u_light_pos[i].xyz;
		vec3 l_col=u_light_col[i].rgb;

		vec3 l=l_pos-pos;
		float dist=length(l);
		l/=dist;

		//diffuse(Lambert)
		float n_dot_l=max(0, dot(n, l));
		vec3 diffuse=l_col*base_col*n_dot_l;

		//blinn-phong specular
		vec3 h=normalize(l+v);//half-vector
		float n_dot_h=max(0, dot(n, h));
		float spec_factor=pow(n_dot_h, shininess);
		vec3 specular=l_col*spec_factor*spec_mag;

		//attenuation (apply equally to all channels)
		float att=1/(att_k0+att_k1*dist+att_k2*dist*dist);

		col+=att*(diffuse+specular);
	}
	
	//linear->srgb
	vec3 col_srgb=pow(col, vec3(1/2.2));
	vec4 tex_col =vec4(col_srgb, 1);
	frag_color = u_tint * tex_col;

}

@end

@program default vs_default fs_default

///////// 2d texture with/without animation //////////////////

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
	vec4 u_tint;
	vec2 u_tl;
	vec2 u_br;
};

in vec2 uv;

out vec4 frag_color;

void main()
{
	vec4 col = texture(sampler2D(texview_tex, texview_smp), u_tl + uv * (u_br - u_tl));
	frag_color = u_tint * col;
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

@vs vs_skybox

layout(binding = 0) uniform vs_skybox_params{
	mat4 u_mvp;
};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec2 uv;

void main() {
	vec4 pos = u_mvp * vec4(v_pos, 1);
	gl_Position = pos.xyww;
	uv = v_uv;
}

@end

@fs fs_skybox

layout(binding = 0) uniform texture2D skybox_tex;
layout(binding = 0) uniform sampler skybox_smp;

in vec2 uv;

out vec4 frag_color;


void main(){
	frag_color = texture(sampler2D(skybox_tex, skybox_smp), uv);
}

@end

@program skybox vs_skybox fs_skybox


/*===== RENDER SHADER =====*/

@vs vs_render

layout(binding=0) uniform vs_render_params {
	mat4 u_mvp;
};

in vec3 i_pos;
in vec3 i_col;

out vec3 col;

void main() {
	gl_Position=u_mvp*vec4(i_pos, 1);
	col=i_col;
}

@end

@fs fs_render

in vec3 col;

out vec4 o_frag_col;

void main() {
	o_frag_col=vec4(col, 1);
}

@end

@program render vs_render fs_render




/*==== PROCESS SHADER ====*/

@vs vs_process

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv=i_uv;
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_process

layout(binding=0) uniform sampler u_process_smp;
layout(binding=0) uniform texture2D u_process_tex;

in vec2 uv;

out vec4 o_frag_col;

const float Curvature=5.1;
const float Blur=.012;
const float CAAmt=1.012;

void main() {
	//curving
	vec2 norm_uv=2*uv-1;
	vec2 offset=norm_uv.yx/Curvature;
	vec2 crt_uv=.5+.5*(1+offset*offset)*norm_uv;

	//border
	vec2 edge=smoothstep(0., Blur, crt_uv)*(1.-smoothstep(1.-Blur, 1., crt_uv));

	//chromatic abberation
	o_frag_col=vec4(edge.x*edge.y*vec3(
		texture(sampler2D(u_process_tex, u_process_smp), (crt_uv-.5)*CAAmt+.5).r,
		texture(sampler2D(u_process_tex, u_process_smp), crt_uv).g,
		texture(sampler2D(u_process_tex, u_process_smp), (crt_uv-.5)/CAAmt+.5).b
	), 1);

	//scanlines
	float scan=.75*.5*abs(sin(gl_FragCoord.y));
	o_frag_col.rgb=mix(o_frag_col.rgb, vec3(0), scan);
}

@end

@program process vs_process fs_process


////////// crosshatch shader ///////////////////////////////

@vs vs_crosshatch

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv=i_uv;
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_crosshatch

layout(binding = 0) uniform fs_crosshatch_params{
	float time;
};


layout(binding=0) uniform sampler u_crosshatch_smp;
layout(binding=0) uniform texture2D u_crosshatch_tex;

in vec2 uv;

out vec4 o_frag_col;

#define LUM(x) dot(x, vec3(.299,587,.114))

const float Pi = 3.1415927;
const int FramesPerSecond = 4;
const float VoronoiScale = 13.6;
const int NumLevels = 4;
const float LineWidth = 4.7;

float hash11(float p) {
	p=fract(.1031*p);
	p*=33.33+p;
	return fract(2*p*p);
}
vec2 hash21(float p) {
	vec3 p3=fract(p*vec3(.1031, .1030, .0973));
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}
vec2 hash22(vec2 p) {
	vec3 p3=fract(p.xyx*vec3(.1031, .1030, .0973));
	p3+=dot(p3, 33.33+p3.yzx);
	return fract(p3.zy*(p3.xx+p3.yz));
}

vec2 voronoi(vec2 x) {
	vec2 n=floor(x);
	vec2 f=x-n;

	vec3 m=vec3(8);
	for(int j=-1; j<=1; j++) {
		for(int i=-1; i<=1; i++) {
			vec2 g=vec2(i, j);
			vec2 o=hash22(n+g);
			vec2 r=g-f+(.5+.5*sin(2*Pi*o));
			float d=dot(r, r);
			if(d<m.x) m=vec3(d, o);
		}
	}

	return vec2(sqrt(m.x), m.y+m.z);
}

//sobel simplification: transpose x for y!
const int sobel[9]=int[9](-1,0,1,-2,0,2,-1,0,1);


void main(){

	//get random voronoi region
	vec2 c= voronoi(VoronoiScale * (gl_FragCoord.xy + hash21(time)));

	//sample image and get luminance
	vec4 base = texture(sampler2D(u_crosshatch_tex, u_crosshatch_smp), uv);
	float lum = LUM(base.rgb);

	//posterize to determine # hatch
	int num = NumLevels - int(float(1 + NumLevels) * lum);
	
	//offset coord a bit for jagged lines
	vec2 rand_coord = gl_FragCoord.xy - .5 + .4 * hash22(gl_FragCoord.xy);
	float rand_seed = 100 * c.y;
	float angle = 2 * Pi * hash11(rand_seed);
	
	float new_lum = 1.;
	for(int i=0; i<num; i++) {
		//offset angle to avoid moire
		rand_seed+=10;
		angle+=Pi*mix(
			1/float(1+num),
			1/float(num),
			hash11(rand_seed)
		);
		
		//procedural hatch pattern
		float d2l=abs(dot(rand_coord, vec2(cos(angle), sin(angle))));
		float repeat=abs(1-2*mod(d2l/LineWidth, 1));
		repeat=pow(repeat, 1.3);
		float line=smoothstep(.64, .65, repeat);
		new_lum-=line;
	}

	o_frag_col = vec4(base.rgb * clamp(new_lum, 0,1), 1);

	//add edge lines w/ sobel
	vec2 rand_uv = rand_coord/gl_FragCoord.xy;
	vec2 sum = vec2(0,0);
	for(int i = 0; i < 3; i++)
	{
		for(int j = 0; j < 3; j++)
		{
			vec2 offset_uv = vec2(i,j) / gl_FragCoord.xy;
			vec4 col = texture(sampler2D(u_crosshatch_tex, u_crosshatch_smp), rand_uv + offset_uv);

			float lum = LUM(col.rgb);
			sum += lum * vec2(sobel[i + 3 * j], sobel[j + 3 * i]);

		}
	}
	
	float mag = length(sum);
	o_frag_col.rgb *= 1 - smoothstep(.75, .9, mag);


}

@end

@program crosshatch vs_crosshatch fs_crosshatch

////////////// sin wave shader //////////////////////////


@vs vs_sinwave

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv=i_uv;
	gl_Position=vec4(i_pos, .5, 1);
}

@end

@fs fs_sinwave

layout(binding = 0) uniform fs_sinwave_params{
	float time;
};

layout(binding=0) uniform sampler u_sinwave_smp;
layout(binding=0) uniform texture2D u_sinwave_tex;

in vec2 uv;

out vec4 o_frag_col;

void main(){
	
	vec2 i_uv = gl_FragCoord.xy;

	i_uv.y = -1.0 - i_uv.y;
	i_uv.x += sin(i_uv.y * 10.0 + time)/ 10.0;

	vec4 color = texture(sampler2D(u_sinwave_tex, u_sinwave_smp), i_uv - uv);

	o_frag_col = color;
}

@end

@program sinwave vs_sinwave fs_sinwave
