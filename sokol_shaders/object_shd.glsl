@vs vs_object_default

layout(binding=0) uniform vs_object_default_params {
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

@fs fs_object_default

layout(binding=0) uniform texture2D object_default_tex;
layout(binding=0) uniform sampler object_default_smp;

layout(binding=1) uniform fs_object_default_params {
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
	//vec3 base_col_srgb=texture(sampler2D(object_default_tex, object_default_smp),u_tl + uv * (u_br - u_tl)).rgb;
	vec3 base_col_srgb=texture(sampler2D(object_default_tex, object_default_smp), uv).rgb;
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

@program object_default vs_object_default fs_object_default


///////////////// linemesh shader ///////////////////////////////////

@vs vs_object_line

in vec3 i_pos;
in vec4 i_col;

out vec4 col;

layout(binding = 0) uniform vs_object_line_params {
	mat4 u_mvp;
};

void main()
{
	col = i_col;
	gl_Position = u_mvp * vec4(i_pos, 1);
}

@end

@fs fs_object_line

in vec4 col;

out vec4 o_frag_col;

layout(binding = 1) uniform fs_object_line_params{
	vec4 u_tint;
};

void main()
{
	o_frag_col = u_tint * col;
}

@end

@program object_line vs_object_line fs_object_line