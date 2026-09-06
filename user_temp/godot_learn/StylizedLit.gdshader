shader_type spatial;

// Inspector automatically exposes these ShaderMaterial parameters.
uniform vec4 base_color : source_color = vec4(0.12, 0.35, 0.90, 1.0);
uniform float roughness : hint_range(0.0, 1.0, 0.01) = 0.45;
uniform float metallic : hint_range(0.0, 1.0, 0.01) = 0.0;
uniform vec4 rim_color : source_color = vec4(0.10, 0.65, 1.0, 1.0);
uniform float rim_power : hint_range(0.5, 8.0, 0.1) = 3.0;
uniform float rim_intensity : hint_range(0.0, 10.0, 0.05) = 1.5;

void fragment() {
	float n_dot_v = clamp(dot(normalize(NORMAL), normalize(VIEW)), 0.0, 1.0);
	float rim = pow(1.0 - n_dot_v, rim_power);

	ALBEDO = base_color.rgb;
	ROUGHNESS = roughness;
	METALLIC = metallic;
	EMISSION = rim_color.rgb * rim * rim_intensity;
}
