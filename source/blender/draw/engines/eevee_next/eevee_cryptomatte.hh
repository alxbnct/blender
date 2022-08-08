/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Cryptomatte.
 *
 * Cryptomatte stores the output during rendering in a single texture.
 * Inside the film the output is extracted per enabled cryptomatte layer.
 * Each cryptomatte layer can hold N samples. These are stored in multiple
 * sequentially bound textures. The samples are sorted and merged.
 */

#pragma once

#include "eevee_shader_shared.hh"

extern "C" {
struct Material;
}

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Cryptomatte
 * \{ */

class Cryptomatte {
 private:
  class Instance &inst_;

  /**
   * Offsets of cryptomatte layers inside the components of a color. (-1 means the layer isn't
   * enabled.)
   */
  int object_offset_;
  int asset_offset_;
  int material_offset_;

  /** Number of enabled cryptomatte layers. */
  int layer_len_;

  DRWPass *cryptomatte_ps_ = nullptr;
  Framebuffer cryptomatte_fb_ = {"cryptomatte"};
  DRWShadingGroup *mesh_grp_;
  DRWShadingGroup *hair_grp_;

 public:
  Cryptomatte(Instance &inst) : inst_(inst){};
  ~Cryptomatte(){};

  void init();

  void sync();
  void sync_mesh(Object *ob);
  void sync_curves(Object *object, ModifierData *modifier_data);
  void sync_gpencil();  // TODO
  void render();

 private:
  void add_hash(const Object *object, float4 &r_hash) const;
  void add_hash(const Material *material, float4 &r_hash) const;
};

/** \} */

}  // namespace blender::eevee
