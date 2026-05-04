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




////////////// sin wave shader //////////////////////////


@vs vs_sinwave

in vec2 i_pos;
in vec2 i_uv;

out vec2 uv;

void main() {
	uv.x =i_uv.x;
	uv.y = -i_uv.y;

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
	
	vec2 i_uv = uv;

    i_uv.y = -1.0 - uv.y;
	i_uv.x += sin(time + 10. *uv.y )/ 10.0;

	vec4 color = texture(sampler2D(u_sinwave_tex, u_sinwave_smp), i_uv);

	o_frag_col = color;
}

@end

@program sinwave vs_sinwave fs_sinwave