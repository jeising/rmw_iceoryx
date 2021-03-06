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

#include <vector>

#include "iceoryx_posh/mepoo/chunk_info.hpp"
#include "iceoryx_posh/popo/publisher.hpp"

#include "rcutils/error_handling.h"

#include "rmw/impl/cpp/macros.hpp"
#include "rmw/rmw.h"

#include "rmw_iceoryx_cpp/iceoryx_serialize.hpp"
#include "rmw_iceoryx_cpp/iceoryx_type_info_introspection.hpp"

#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"

#include "./types/iceoryx_publisher.hpp"

extern "C"
{
namespace details
{
rmw_ret_t
send_payload(
  iox::popo::Publisher * iceoryx_publisher,
  const void * serialized_ros_msg,
  size_t size)
{
  if (serialized_ros_msg == nullptr) {
    RMW_SET_ERROR_MSG("serialized message pointer is null");
    return RMW_RET_ERROR;
  }
  void * chunk = iceoryx_publisher->allocateChunk(size, true);

  memcpy(chunk, serialized_ros_msg, size);

  iceoryx_publisher->sendChunk(chunk);

  return RMW_RET_OK;
}
}  // namespace details

rmw_ret_t
rmw_publish(
  const rmw_publisher_t * publisher,
  const void * ros_message,
  rmw_publisher_allocation_t * allocation)
{
  if (!ros_message) {
    fprintf(stderr, "ros message is empty\n");
    return RMW_RET_ERROR;
  }
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);
  (void)allocation;

  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    rmw_publish
    : publisher, publisher->implementation_identifier,
    rmw_get_implementation_identifier(), return RMW_RET_ERROR);

  auto iceoryx_publisher = static_cast<IceoryxPublisher *>(publisher->data);
  if (!iceoryx_publisher) {
    RMW_SET_ERROR_MSG("publisher data is null");
    return RMW_RET_ERROR;
  }

  auto iceoryx_sender = iceoryx_publisher->iceoryx_sender_;
  if (!iceoryx_sender) {
    RMW_SET_ERROR_MSG("iceoryx_sender is null");
    return RMW_RET_ERROR;
  }

  // if messages have a fixed size, we can just memcpy
  if (iceoryx_publisher->is_fixed_size_) {
    return details::send_payload(iceoryx_sender, ros_message, iceoryx_publisher->message_size_);
  }

  // this should never happen if checked already at rmw_create_publisher
  if (!rmw_iceoryx_cpp::iceoryx_is_valid_type_support(&iceoryx_publisher->type_supports_)) {
    RMW_SET_ERROR_MSG("Use either C typesupport or CPP typesupport");
    return RMW_RET_ERROR;
  }

  // message is neither loaned nor fixed size, so we have to serialize
  std::vector<char> payload_vector{};

  // serialize with cpp typesupport
  auto ts_cpp = get_message_typesupport_handle(
    &iceoryx_publisher->type_supports_,
    rosidl_typesupport_introspection_cpp::typesupport_identifier);
  if (ts_cpp != nullptr) {
    auto members =
      static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(ts_cpp->data);
    rmw_iceoryx_cpp::serialize(ros_message, members, payload_vector);
  }

  // serialize with c typesupport
  auto ts_c = get_message_typesupport_handle(
    &iceoryx_publisher->type_supports_,
    rosidl_typesupport_introspection_c__identifier);
  if (ts_c != nullptr) {
    auto members =
      static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(ts_c->data);
    rmw_iceoryx_cpp::serialize(ros_message, members, payload_vector);
  }

  // send composed payload
  return details::send_payload(iceoryx_sender, payload_vector.data(), payload_vector.size());
}

rmw_ret_t
rmw_publish_serialized_message(
  const rmw_publisher_t * publisher,
  const rmw_serialized_message_t * serialized_message,
  rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_ERROR);

  assert(false);
  return RMW_RET_OK;
}

rmw_ret_t
rmw_borrow_loaned_message(
  const rmw_publisher_t * publisher,
  const rosidl_message_type_support_t * type_support,
  void ** ros_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_ERROR);

  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    rmw_borrow_loaned_message
    : publisher, publisher->implementation_identifier,
    rmw_get_implementation_identifier(), return RMW_RET_ERROR);

  auto iceoryx_publisher = static_cast<IceoryxPublisher *>(publisher->data);
  if (!iceoryx_publisher) {
    RMW_SET_ERROR_MSG("publisher data is null");
    return RMW_RET_ERROR;
  }

  auto iceoryx_sender = iceoryx_publisher->iceoryx_sender_;
  if (!iceoryx_sender) {
    RMW_SET_ERROR_MSG("iceoryx_sender is null");
    return RMW_RET_ERROR;
  }

  // this should never happen if checked already at rmw_create_publisher
  if (!rmw_iceoryx_cpp::iceoryx_is_valid_type_support(&iceoryx_publisher->type_supports_)) {
    RMW_SET_ERROR_MSG("Use either C typesupport or CPP typesupport");
    return RMW_RET_ERROR;
  }

  auto msg_memory = iceoryx_sender->allocateChunk(
    static_cast<uint32_t>(iceoryx_publisher->message_size_), true);
  rmw_iceoryx_cpp::iceoryx_init_message(&iceoryx_publisher->type_supports_, msg_memory);
  *ros_message = msg_memory;
  return RMW_RET_OK;
}

rmw_ret_t
rmw_return_loaned_message_from_publisher(const rmw_publisher_t * publisher, void * loaned_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(loaned_message, RMW_RET_ERROR);

  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    rmw_return_loaned_message
    : publisher, publisher->implementation_identifier,
    rmw_get_implementation_identifier(), return RMW_RET_ERROR);

  auto iceoryx_publisher = static_cast<IceoryxPublisher *>(publisher->data);
  if (!iceoryx_publisher) {
    RMW_SET_ERROR_MSG("publisher data is null");
    return RMW_RET_ERROR;
  }

  auto iceoryx_sender = iceoryx_publisher->iceoryx_sender_;
  if (!iceoryx_sender) {
    RMW_SET_ERROR_MSG("iceoryx_sender is null");
    return RMW_RET_ERROR;
  }

  // this should never happen if checked already at rmw_create_publisher
  if (!rmw_iceoryx_cpp::iceoryx_is_valid_type_support(&iceoryx_publisher->type_supports_)) {
    RMW_SET_ERROR_MSG("Use either C typesupport or CPP typesupport");
    return RMW_RET_ERROR;
  }

  rmw_iceoryx_cpp::iceoryx_fini_message(&iceoryx_publisher->type_supports_, loaned_message);
  iceoryx_sender->freeChunk(loaned_message);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_publish_loaned_message(
  const rmw_publisher_t * publisher,
  void * ros_message,
  rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);
  (void)allocation;

  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    rmw_publish
    : publisher, publisher->implementation_identifier,
    rmw_get_implementation_identifier(), return RMW_RET_ERROR);

  auto iceoryx_publisher = static_cast<IceoryxPublisher *>(publisher->data);
  if (!iceoryx_publisher) {
    RMW_SET_ERROR_MSG("publisher data is null");
    return RMW_RET_ERROR;
  }

  auto iceoryx_sender = iceoryx_publisher->iceoryx_sender_;
  if (!iceoryx_sender) {
    RMW_SET_ERROR_MSG("iceoryx_sender is null");
    return RMW_RET_ERROR;
  }

  if (!iceoryx_publisher->is_fixed_size_) {
    RMW_SET_ERROR_MSG("iceoryx can't loan non-fixed sized messages");
    return RMW_RET_ERROR;
  }
  iceoryx_sender->sendChunk(ros_message);
  return RMW_RET_OK;
}
}  // extern "C"
