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
 * Copyright 2021, Blender Foundation.
 */

#include "COM_SeparateXYZNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

SeparateXYZNode::SeparateXYZNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SeparateXYZNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext &UNUSED(context)) const
{
  NodeInput *vector_socket = this->get_input_socket(0);
  NodeOutput *output_x_socket = this->get_output_socket(0);
  NodeOutput *output_y_socket = this->get_output_socket(1);
  NodeOutput *output_z_socket = this->get_output_socket(2);

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(0);
    converter.add_operation(operation);
    converter.map_input_socket(vector_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_x_socket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(1);
    converter.add_operation(operation);
    converter.map_input_socket(vector_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_y_socket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(2);
    converter.add_operation(operation);
    converter.map_input_socket(vector_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_z_socket, operation->get_output_socket(0));
  }
}

}  // namespace blender::compositor
