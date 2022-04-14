/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#include "GPU_state.h"
#include "GPU_texture.h"

#include "MEM_guardedalloc.h"

#include "VPC_context.hh"
#include "VPC_input_descriptor.hh"
#include "VPC_reduce_to_single_value_processor_operation.hh"
#include "VPC_result.hh"

namespace blender::viewport_compositor {

ReduceToSingleValueProcessorOperation::ReduceToSingleValueProcessorOperation(Context &context,
                                                                             ResultType type)
    : ProcessorOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = type;
  declare_input_descriptor(input_descriptor);
  populate_result(Result(type, texture_pool()));
}

void ReduceToSingleValueProcessorOperation::execute()
{
  /* Download the input pixel from the GPU texture. */
  const Result &input = get_input();
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float *pixel = static_cast<float *>(GPU_texture_read(input.texture(), GPU_DATA_FLOAT, 0));

  /* Allocate a single value result and set its value to the value of the downloaded pixel. */
  Result &result = get_result();
  result.allocate_single_value();
  switch (result.type()) {
    case ResultType::Color:
      result.set_color_value(pixel);
      break;
    case ResultType::Vector:
      result.set_vector_value(pixel);
      break;
    case ResultType::Float:
      result.set_float_value(*pixel);
      break;
  }

  /* Free the downloaded pixel. */
  MEM_freeN(pixel);
}

ProcessorOperation *ReduceToSingleValueProcessorOperation::construct_if_needed(
    Context &context, const Result &input_result)
{
  /* Input result is already a single value, the processor is not needed. */
  if (input_result.is_single_value()) {
    return nullptr;
  }

  /* The input is a full sized texture can can't be reduced to a single value, the processor is not
   * needed. */
  if (input_result.domain().size != int2(1)) {
    return nullptr;
  }

  /* The input is a texture of a single pixel and can be reduced to a single value. */
  return new ReduceToSingleValueProcessorOperation(context, input_result.type());
}

}  // namespace blender::viewport_compositor
