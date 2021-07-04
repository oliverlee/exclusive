#pragma once

/// @brief Provides exclusive access to shared resources
namespace exclusive {

/// @brief A shared resource with synchronized access
/// @tparam T Shared resource type
template <class T>
struct shared_resource {};

}  // namespace exclusive
