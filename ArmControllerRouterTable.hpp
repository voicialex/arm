#ifndef PERCEPTION_ARM_CONTROLLER_ROUTER_TABLE_HPP
#define PERCEPTION_ARM_CONTROLLER_ROUTER_TABLE_HPP

#include "message/MessageRouter.hpp"

namespace perception {

class ArmController;

class ArmControllerRouterTable {
 public:
  static MessageRouter::RouteTable Get(ArmController *controller);
};

}  // namespace perception

#endif  // PERCEPTION_ARM_CONTROLLER_ROUTER_TABLE_HPP
