#pragma once

#include "kernel/light/light.h"

CCL_NAMESPACE_BEGIN

ccl_device float light_tree_bounding_box_angle(const float3 bbox_min,
                                               const float3 bbox_max,
                                               const float3 P,
                                               const float3 point_to_centroid)
{
  /* Want to iterate through all 8 possible points of the bounding box. */
  float theta_u = 0;
  float3 corners[8];
  corners[0] = bbox_min;
  corners[1] = make_float3(bbox_min.x, bbox_min.y, bbox_max.z);
  corners[2] = make_float3(bbox_min.x, bbox_max.y, bbox_min.z);
  corners[3] = make_float3(bbox_min.x, bbox_max.y, bbox_max.z);
  corners[4] = make_float3(bbox_max.x, bbox_min.y, bbox_min.z);
  corners[5] = make_float3(bbox_max.x, bbox_min.y, bbox_max.z);
  corners[6] = make_float3(bbox_max.x, bbox_max.y, bbox_min.z);
  corners[7] = bbox_max;
  for (int i = 0; i < 8; ++i) {
    float3 point_to_corner = normalize(corners[i] - P);
    const float cos_theta_u = dot(point_to_centroid, point_to_corner);
    theta_u = fmaxf(fast_acosf(cos_theta_u), theta_u);
  }
  return theta_u;
}

/* This is the general function for calculating the importance of either a cluster or an emitter.
 * Both of the specialized functions obtain the necessary data before calling this function.
 * to-do: find a better way to handle this? */
ccl_device float light_tree_node_importance(const float3 P,
                                            const float3 N,
                                            const float3 bbox_min,
                                            const float3 bbox_max,
                                            const float3 bcone_axis,
                                            const float theta_o,
                                            const float theta_e,
                                            const float energy)
{
  const float3 centroid = 0.5f * bbox_min + 0.5f * bbox_max;
  const float3 point_to_centroid = normalize(centroid - P);

  /* Since we're not using the splitting heuristic, we clamp
   * the distance to half the radius of the cluster. */
  const float distance_squared = fminf(len_squared(centroid - P),
                                       0.25f * len_squared(bbox_max - centroid));

  const float theta = fast_acosf(dot(bcone_axis, -point_to_centroid));
  const float theta_i = fast_acosf(dot(point_to_centroid, N));
  const float theta_u = light_tree_bounding_box_angle(bbox_min, bbox_max, P, point_to_centroid);

  /* to-do: compare this with directly using fmaxf and cosf. */
  /* Avoid using cosine until needed. */
  const float theta_prime = fmaxf(theta - theta_o - theta_u, 0);
  if (theta_prime >= theta_e) {
    return 0;
  }
  const float cos_theta_prime = fast_cosf(theta_prime);

  float cos_theta_i_prime = 1;
  if (theta_i - theta_u > 0) {
    cos_theta_i_prime = fabsf(fast_cosf(theta_i - theta_u));
  }

  /* to-do: find a good approximation for this value. */
  const float f_a = 1;

  float importance = f_a * cos_theta_i_prime * energy / distance_squared * cos_theta_prime;
  return importance;
}

ccl_device float light_tree_emitter_importance(KernelGlobals kg,
                                               const float3 P,
                                               const float3 N,
                                               int emitter_index)
{
  ccl_global const KernelLightDistribution *kdistribution = &kernel_tex_fetch(__light_distribution,
                                                                              emitter_index);
  const int prim = kdistribution->prim;

  if (prim >= 0) {
    /* to-do: handle case for mesh lights. */
  }

  /* If we're not at a mesh light, then we should be at a point, spot, or area light. */
  const int lamp = -prim - 1;
  const ccl_global KernelLight *klight = &kernel_tex_fetch(__lights, lamp);
  const float radius = klight->spot.radius;
  const float3 bbox_min = make_float3(
      klight->co[0] - radius, klight->co[1] - radius, klight->co[2] - radius);
  const float3 bbox_max = make_float3(
      klight->co[0] + radius, klight->co[1] + radius, klight->co[2] + radius);
  const float3 bcone_axis = make_float3(
      klight->spot.dir[0], klight->spot.dir[1], klight->spot.dir[2]);
  const float3 rgb_strength = make_float3(
      klight->strength[0], klight->strength[1], klight->strength[2]);

  return light_tree_node_importance(
      P, N, bbox_min, bbox_max, bcone_axis, M_PI_F, M_PI_2_F, linear_rgb_to_gray(kg, rgb_strength));
}

ccl_device float light_tree_cluster_importance(KernelGlobals kg,
                                               const float3 P,
                                               const float3 N,
                                               const ccl_global KernelLightTreeNode *knode)
{
  /* Convert the data from the struct into float3 for calculations. */
  const float3 bbox_min = make_float3(
      knode->bounding_box_min[0], knode->bounding_box_min[1], knode->bounding_box_min[2]);
  const float3 bbox_max = make_float3(
      knode->bounding_box_max[0], knode->bounding_box_max[1], knode->bounding_box_max[2]);
  const float3 bcone_axis = make_float3(
      knode->bounding_cone_axis[0], knode->bounding_cone_axis[1], knode->bounding_cone_axis[1]);

  return light_tree_node_importance(
      P, N, bbox_min, bbox_max, bcone_axis, knode->theta_o, knode->theta_e, knode->energy);
}


/* to-do: for now, we're not going to worry about being in a volume for now,
 * but this seems to be a good way to differentiate whether we're in a volume or not. */
template<bool in_volume_segment>
ccl_device bool light_tree_sample(KernelGlobals kg,
                                  ccl_private const RNGState *rng_state,
                                  float randu,
                                  const float randv,
                                  const float time,
                                  const float3 N,
                                  const float3 P,
                                  const int bounce,
                                  const uint32_t path_flag,
                                  ccl_private LightSample *ls,
                                  float *pdf_factor)
{
  /* First traverse the light tree until a leaf node is reached. */
  /* Also keep track of the probability of traversing to a given node, */
  /* so that we can scale our PDF accordingly later. */
  int index = 0;
  *pdf_factor = 1.0f;

  /* to-do: is it better to generate a new random sample for each step of the traversal? */
  float tree_u = path_state_rng_1D(kg, rng_state, 1);
  const ccl_global KernelLightTreeNode *knode = &kernel_tex_fetch(__light_tree_nodes, index);
  while (knode->child_index > 0) {
    /* At an interior node, the left child is directly next to the parent,
     * while the right child is stored as the child index. */
    const ccl_global KernelLightTreeNode *left = &kernel_tex_fetch(__light_tree_nodes, index + 1);
    const ccl_global KernelLightTreeNode *right = &kernel_tex_fetch(__light_tree_nodes, knode->child_index);

    const float left_importance = light_tree_cluster_importance(kg, P, N, left);
    const float right_importance = light_tree_cluster_importance(kg, P, N, right);
    const float left_probability = left_importance / (left_importance + right_importance);

    if (tree_u < left_probability) {
      index = index + 1;
      knode = left;
      tree_u = tree_u * (left_importance + right_importance) / left_importance;
      *pdf_factor *= left_probability;
    }
    else {
      index = knode->child_index;
      knode = right;
      tree_u = (tree_u * (left_importance + right_importance) - left_importance) /
               right_importance;
      *pdf_factor *= (1 - left_probability);
    }
  }

  /* Right now, sampling is done by incrementing the CDF by the PDF.
   * However, we first need to calculate the total importance so that we can normalize the CDF. */
  float total_emitter_importance = 0.0f;
  for (int i = 0; i < knode->num_prims; i++) {
    const int prim_index = -knode->child_index + i;
    total_emitter_importance += light_tree_emitter_importance(kg, P, N, prim_index);
  }

  /* to-do: need to handle a case when total importance is 0. */
  if (total_emitter_importance == 0.0f) {
    return false;
  }

  /* Once we have the total importance, we can normalize the CDF and sample it. */
  const float inv_total_importance = 1 / total_emitter_importance;
  float emitter_cdf = 0.0f;
  for (int i = 0; i < knode->num_prims; i++) {
    const int prim_index = -knode->child_index + i;
    /* to-do: is there any way to cache these values, so that recalculation isn't needed? */
    const float emitter_pdf = light_tree_emitter_importance(kg, P, N, prim_index) *
                              inv_total_importance;
    emitter_cdf += emitter_pdf;
    if (tree_u < emitter_cdf) {
      *pdf_factor *= emitter_pdf;
      if (UNLIKELY(light_select_reached_max_bounces(kg, prim_index, bounce))) {
        return false;
      }
      return light_sample<in_volume_segment>(kg, prim_index, randu, randv, P, path_flag, ls);
    }
  }

  /* We should never reach this point. */
  assert(false);
  return false;
}

ccl_device bool light_tree_sample_from_position(KernelGlobals kg,
                                                ccl_private const RNGState *rng_state,
                                                float randu,
                                                const float randv,
                                                const float time,
                                                const float3 P,
                                                const float3 N,
                                                const int bounce,
                                                const uint32_t path_flag,
                                                ccl_private LightSample *ls)
{
  float pdf_factor;
  bool ret = light_tree_sample<false>(
      kg, rng_state, randu, randv, time, N, P, bounce, path_flag, ls, &pdf_factor);
  assert(pdf_factor != 0.0f);
  ls->pdf *= pdf_factor;
  return ret;
}

CCL_NAMESPACE_END
