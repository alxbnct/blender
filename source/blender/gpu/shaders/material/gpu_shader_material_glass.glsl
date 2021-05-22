
void node_bsdf_glass(vec4 color,
                     float roughness,
                     vec3 N,
                     float weight,
                     float use_multiscatter,
                     out Closure result,
                     out float reflection_weight,
                     out float refraction_weight)
{
  float fresnel = 0.5;
  reflection_weight = fresnel;
  refraction_weight = 1.0 - fresnel;
  closure_weight_add(g_reflection_data, reflection_weight);
  closure_weight_add(g_refraction_data, refraction_weight);
}

void node_bsdf_glass_eval(vec4 color,
                          float roughness,
                          vec3 N,
                          float weight,
                          float use_multiscatter,
                          float reflection_weight,
                          float refraction_weight,
                          out Closure result)
{
  if (closure_weight_threshold(g_reflection_data, reflection_weight)) {
    g_reflection_data.color = color.rgb * reflection_weight;
    g_reflection_data.N = N;
    g_reflection_data.roughness = roughness;
  }
  if (closure_weight_threshold(g_refraction_data, refraction_weight)) {
    g_refraction_data.color = color.rgb * refraction_weight;
    g_refraction_data.N = N;
    g_refraction_data.roughness = roughness;
  }
}
