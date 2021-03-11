/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */
#pragma once

#include "usd_reader_xform.h"

#include <pxr/usd/usdGeom/xform.h>

struct Collection;

namespace blender::io::usd {

/* Wraps the UsdGeomXform schema. Creates a Blender Empty object. */

class USDInstanceReader : public USDXformReader {

 public:
  USDInstanceReader(pxr::UsdStageRefPtr stage,
                    const pxr::UsdPrim &object,
                    const USDImportParams &import_params,
                    ImportSettings &settings);

  bool valid() const override;

  void create_object(Main *bmain, double motionSampleTime) override;

  void set_instance_collection(Collection *coll);

  pxr::SdfPath proto_path() const;
};

}  // namespace blender::io::usd
