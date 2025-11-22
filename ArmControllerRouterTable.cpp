#include "ArmControllerRouterTable.hpp"
#include "ArmController.hpp"
#include "message/MessageFrame.hpp"
#include "log.hpp"
#define LOG_TAG "ArmRouter"

namespace perception {

// 路由配置宏：统一处理方式
// 先发送 PROCESSING 响应（不带 Pose），然后执行动作并发送 COMPLETED 响应（带 Pose）
#define ARM_ROUTE_ENTRY(msg_id, sub_id)                                                       \
  {                                                                                           \
    {msg_id, sub_id, StatusCode::PROCESSING}, MessageRouter::RouteEntry {                     \
      [controller](const MessageKey &key, const std::vector<uint8_t> &) {                     \
        controller->SendResponse(key.message_id, key.sub_message_id, StatusCode::PROCESSING); \
        controller->SimulateAction(key);                                                      \
      }                                                                                       \
    }                                                                                         \
  }

#define ARM_MANUAL_ROUTE_ENTRY(msg_id, sub_id)                                                \
  {                                                                                           \
    {msg_id, sub_id, StatusCode::PROCESSING}, MessageRouter::RouteEntry {                     \
      [controller](const MessageKey &key, const std::vector<uint8_t> &) {                     \
        controller->SendResponse(key.message_id, key.sub_message_id, StatusCode::PROCESSING); \
        controller->HandleManualModeMonitorRequest();                                         \
      }                                                                                       \
    }                                                                                         \
  }

MessageRouter::RouteTable ArmControllerRouterTable::Get(ArmController *controller) {
  MessageRouter::RouteTable routes = {
      // ═══════════════════════════════════════════════════════════════════════════════════════
      // 充电流程路由配置
      // ═══════════════════════════════════════════════════════════════════════════════════════
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::DEVICE_SELF_CHECK),
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::OPEN_COVER),
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::ARM_MOVING_TO_INIT_POSITION),
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::ARM_MOVING_TO_TARGET_POSITION),
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::PATH_PLANNING),
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::CHARGING_INSERTION),
      ARM_ROUTE_ENTRY(MessageIds::START_CHARGING, SubMessageIds::CONNECTION_VERIFICATION),

      // ═══════════════════════════════════════════════════════════════════════════════════════
      // 标定流程路由配置
      // ═══════════════════════════════════════════════════════════════════════════════════════
      ARM_ROUTE_ENTRY(MessageIds::CALIBRATION, SubMessageIds::DEVICE_SELF_CHECK),
      ARM_ROUTE_ENTRY(MessageIds::CALIBRATION, SubMessageIds::IDLE),
      ARM_ROUTE_ENTRY(MessageIds::CALIBRATION, SubMessageIds::OPEN_COVER),
      ARM_ROUTE_ENTRY(MessageIds::CALIBRATION, SubMessageIds::ARM_MOVING_TO_INIT_POSITION),
      ARM_ROUTE_ENTRY(MessageIds::CALIBRATION, SubMessageIds::ARM_MOVING_TO_CALIB_POSITION),

      // ═══════════════════════════════════════════════════════════════════════════════════════
      // 系统复位流程路由配置
      // ═══════════════════════════════════════════════════════════════════════════════════════
      ARM_ROUTE_ENTRY(MessageIds::RESET, SubMessageIds::IDLE),
      ARM_ROUTE_ENTRY(MessageIds::RESET, SubMessageIds::CHARGING_REMOVAL),
      ARM_ROUTE_ENTRY(MessageIds::RESET, SubMessageIds::ARM_MOVING_TO_RESET_POSITION),
      ARM_ROUTE_ENTRY(MessageIds::RESET, SubMessageIds::CLOSE_COVER),

      // ═══════════════════════════════════════════════════════════════════════════════════════
      // 紧急停止路由配置
      // ═══════════════════════════════════════════════════════════════════════════════════════
      ARM_ROUTE_ENTRY(MessageIds::EMERGENCY_STOP, SubMessageIds::IDLE),
      ARM_ROUTE_ENTRY(MessageIds::EMERGENCY_STOP_RECOVERY, SubMessageIds::IDLE),

      // ═══════════════════════════════════════════════════════════════════════════════════════
      // 手动模式路由配置
      // ═══════════════════════════════════════════════════════════════════════════════════════
      ARM_MANUAL_ROUTE_ENTRY(MessageIds::MANUAL_REMOTE_CONTROL, SubMessageIds::IDLE),
  };
  return routes;
}

#undef ARM_ROUTE_ENTRY
#undef ARM_MANUAL_ROUTE_ENTRY

}  // namespace perception
