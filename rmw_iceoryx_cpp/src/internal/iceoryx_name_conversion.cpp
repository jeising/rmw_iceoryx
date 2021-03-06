// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <regex>
#include <string>
#include <tuple>

#include "rosidl_generator_c/primitives_sequence.h"
#include "rosidl_generator_c/primitives_sequence_functions.h"

#include "rosidl_typesupport_cpp/message_type_support.hpp"

#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"

#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"

#include "rmw/error_handling.h"

#include "rmw_iceoryx_cpp/iceoryx_name_conversion.hpp"
#include "rmw_iceoryx_cpp/iceoryx_type_info_introspection.hpp"

std::tuple<std::string, std::string, std::string> get_service_description_elements(
  const std::string & topic_name,
  const std::string & type_name)
{
  std::string delimiter_msg = "_ara_msgs/msg/";
  auto pos_msg = type_name.find(delimiter_msg);

  // ROS 2.0 Naming
  if (pos_msg == std::string::npos) {
    // service, instance, event
    return std::make_tuple(type_name, topic_name, "data");
  }

  // Could check more detailed!!
  auto service = type_name.substr(0, pos_msg);
  auto pos_type_name = topic_name.find(type_name);
  auto pos_package_name = topic_name.find(service);
  if (pos_type_name == std::string::npos || pos_package_name == std::string::npos) {
    throw std::runtime_error("message topic and type are inconsistent");
  }

  auto instance = topic_name.substr(1, pos_package_name - 2);       // / before and after
  auto event = type_name.substr(pos_msg + delimiter_msg.size(), type_name.size());

  return std::make_tuple(service, instance, event);
}

std::string to_message_type(const std::string & in)
{
  std::string return_value(in);

  auto pos = return_value.find("__");
  if (pos == std::string::npos) {
    pos = return_value.find("::");
  }

  return_value.replace(pos, 2, "/");
  return return_value;
}

void extract_type(
  const rosidl_message_type_support_t * type_support,
  std::string & service_name,
  std::string & event_name)
{
  service_name = to_message_type(rmw_iceoryx_cpp::iceoryx_get_message_namespace(type_support));
  event_name = rmw_iceoryx_cpp::iceoryx_get_message_name(type_support);
}

namespace rmw_iceoryx_cpp
{

std::tuple<std::string, std::string>
get_name_n_type_from_iceoryx_service_description(
  const std::string & service,
  const std::string & instance,
  const std::string & event)
{
  std::string topic_name;
  std::string type_name;

  if (service.find("/msg/") != std::string::npos) {
    // ROS 2.0 Naming
    return std::make_tuple(instance, service);
  } else {
    // ARA Naming
    std::string delimiter_msg = "_ara_msgs/msg/";

    return std::make_tuple("/" + instance + "/" + service + "/" + event,
             service + delimiter_msg + event);
  }
}

iox::capro::ServiceDescription
get_iceoryx_service_description(
  const std::string & topic_name,
  const rosidl_message_type_support_t * type_supports)
{
  std::string package_name;
  std::string type_name;
  extract_type(type_supports, package_name, type_name);
  type_name = package_name + "/" + type_name;

  auto serviceDescriptionTuple = get_service_description_elements(topic_name, type_name);
  return iox::capro::ServiceDescription(std::get<0>(serviceDescriptionTuple),
           std::get<1>(serviceDescriptionTuple),
           std::get<2>(serviceDescriptionTuple));
}

}  // namespace rmw_iceoryx_cpp
