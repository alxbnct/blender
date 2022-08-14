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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 *
 * Structs used for the sculpt brush system
 */
#pragma once

#include "DNA_color_types.h"
#include "DNA_listBase.h"

#include "BLI_assert.h"

struct GHash;

typedef struct BrushCurve {
  CurveMapping *curve;

  /** curve preset, see eBrushCurvePreset.
      Note: this differs from BrushMappingDef's preset field
    */
  int preset;
  char preset_slope_negative;
  char _pad[3];
} BrushCurve;

/* Input mapping struct.  An input mapping transform
   stroke inputs intos outputs.  Inputs can be device
   events (like pen pressure/tilt) or synethesize
   (cumulative stroke distance, random, etc).

   Inspired by Krita.
*/
typedef struct BrushMapping {
  /* note that we use a curve cache (see BKE_curvemapping_cache)
     and copy on write semantics.  BrushChannels are copied
     extensively (mostly to cache input mappings and resolve
     channel inheritance), to the point that copying the
     channel curves was a problem.

   */

  BrushCurve curve;

  float factor;
  int blendmode; /* blendmode, a subset of the MA_BLEND_XXX enums*/

  int flag, type;

  float min, max;
  float premultiply_factor; /** factor to premultiply input data with */

  int mapfunc; /** mapping function, see eBrushMappingFunc.  Most are periodic. */

  /** threshold for BRUSH_MAPFUNC_CUTOFF and BRUSH_MAPFUNC_SQUARE mapping functions */
  float func_cutoff;

  /** controls whether this channel should inherit from scene defaults,
   *  see eBrushMappingInheritMode */
  char inherit_mode, _pad[3];
} BrushMapping;

typedef struct BrushChannel {
  struct BrushChannel *next, *prev;

  char idname[64]; /* The RNA property name */
  char uiname[64]; /** user-friendly name */
  char *category;  /** category; if NULL, def->category will be used */

  struct BrushChannelType *def; /* Brush channel definition */

  /*
  Cached channel values.
  */
  float fvalue;    /** floating point value */
  int ivalue;      /** stores integer, boolean, enum and bitmasks */
  float vector[4]; /* stores 3 and 4 component vectors */

  /* For curve channels. */
  BrushCurve curve;

  /* Input device mappings */
  BrushMapping mappings[7]; /* dimension should always be BRUSH_MAPPING_MAX */

  short type; /** eBrushChannelType */
  short ui_order;
  int flag; /** eBrushChannelFlag */
  int ui_flag, evaluated_flag;
  int active_mapping, _pad[1];
} BrushChannel;

typedef struct BrushChannelSet {
  ListBase channels;
  int channels_num, _pad[1];

  void *channelmap; /** idname -> channel map. */
} BrushChannelSet;

#define BRUSH_CHANNEL_MAX_IDNAME sizeof(((BrushChannel){0}).idname)

/* BrushMapping->flag */
typedef enum eBrushMappingFlags {
  BRUSH_MAPPING_ENABLED = 1 << 0,
  BRUSH_MAPPING_INVERT = 1 << 1,
  BRUSH_MAPPING_UI_EXPANDED = 1 << 2,
} eBrushMappingFlags;

/* BrushMapping->inherit_mode */
typedef enum eBrushMappingInheritMode {
  BRUSH_MAPPING_INHERIT_NEVER,
  BRUSH_MAPPING_INHERIT_ALWAYS,
  /* Use channel's inheritance mode. */
  BRUSH_MAPPING_INHERIT_CHANNEL
} eBrushMappingInheritMode;

/* BrushMapping->mapfunc */
typedef enum eBrushMappingFunc {
  BRUSH_MAPFUNC_NONE,
  BRUSH_MAPFUNC_SAW,
  BRUSH_MAPFUNC_TENT,
  BRUSH_MAPFUNC_COS,
  BRUSH_MAPFUNC_CUTOFF,
  BRUSH_MAPFUNC_SQUARE, /* square wave */
} eBrushMappingFunc;

/* Input device mapping types. */
typedef enum eBrushMappingType {
  BRUSH_MAPPING_PRESSURE = 0,
  BRUSH_MAPPING_XTILT = 1,
  BRUSH_MAPPING_YTILT = 2,
  BRUSH_MAPPING_ANGLE = 3,
  BRUSH_MAPPING_SPEED = 4,
  BRUSH_MAPPING_RANDOM = 5,
  BRUSH_MAPPING_STROKE_T = 6,
  BRUSH_MAPPING_MAX = 7  // see BrushChannel.mappings
} eBrushMappingType;

BLI_STATIC_ASSERT(offsetof(BrushChannel, type) - offsetof(BrushChannel, mappings) ==
                      sizeof(BrushMapping) * BRUSH_MAPPING_MAX,
                  "BrushChannel.mappings must == BRUSH_MAPPING_MAX");

typedef enum eBrushChannelFlag {
  BRUSH_CHANNEL_INHERIT = 1 << 0,
  BRUSH_CHANNEL_INHERIT_IF_UNSET = 1 << 1,
  BRUSH_CHANNEL_NO_MAPPINGS = 1 << 2,
  BRUSH_CHANNEL_UI_EXPANDED = 1 << 3,
  BRUSH_CHANNEL_APPLY_MAPPING_TO_ALPHA = 1 << 4,
  BRUSH_CHANNEL_NEEDS_EVALUATE = 1 << 5,

  /* Set in scene channels; forces inheritance on brush properties. */
  BRUSH_CHANNEL_FORCE_INHERIT = 1 << 6,

  /* Set in local brush channels; ignores BRUSH_CHANNEL_FORCE_INHERIT. */
  BRUSH_CHANNEL_IGNORE_FORCE_INHERIT = 1 << 7,
} eBrushChannelFlag;

typedef enum eBrushChannelUIFlag {
  BRUSH_CHANNEL_SHOW_IN_WORKSPACE = 1 << 0,
  /* Has user overriden this, used for version patching. */
  BRUSH_CHANNEL_SHOW_IN_WORKSPACE_USER_SET = 1 << 1,
  BRUSH_CHANNEL_SHOW_IN_HEADER = 1 << 2,
  BRUSH_CHANNEL_SHOW_IN_HEADER_USER_SET = 1 << 3,
  BRUSH_CHANNEL_SHOW_IN_CONTEXT_MENU = 1 << 4,
  BRUSH_CHANNEL_SHOW_IN_CONTEXT_MENU_USER_SET = 1 << 5,
} eBrushChannelUIFlag;

// BrushChannelType->type
typedef enum eBrushChannelType {
  BRUSH_CHANNEL_TYPE_FLOAT = 1 << 0,
  BRUSH_CHANNEL_TYPE_INT = 1 << 1,
  BRUSH_CHANNEL_TYPE_ENUM = 1 << 2,
  BRUSH_CHANNEL_TYPE_BITMASK = 1 << 3,
  BRUSH_CHANNEL_TYPE_BOOL = 1 << 4,
  BRUSH_CHANNEL_TYPE_VEC3 = 1 << 5,
  BRUSH_CHANNEL_TYPE_VEC4 = 1 << 6,
  BRUSH_CHANNEL_TYPE_CURVE = 1 << 7
} eBrushChannelType;

/* clang-format off */
typedef enum eBrushChannelSubType {
  BRUSH_CHANNEL_NONE,
  BRUSH_CHANNEL_COLOR,
  BRUSH_CHANNEL_FACTOR,
  BRUSH_CHANNEL_PERCENT,
  BRUSH_CHANNEL_PIXEL,
  BRUSH_CHANNEL_ANGLE
} eBrushChannelSubType;
/* clang-format on */
