#pragma once

#include "runtime/transport/zmq/ZmqPeer.hpp"
#include "message/MessageRouter.hpp"
#include "message/MessageFrame.hpp"
#include "message/EventState.hpp"
#include "log.hpp"
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <thread>

#include "message/PoseItem.hpp"

namespace perception {

class ArmController {
 public:
  ArmController();
  ~ArmController();

  bool Initialize(const std::string &endpoint);
  bool Start();
  void Stop();

  // 事件处理入口（由内部 PeerEventHandlerImpl 调用）
  void OnPeerMessage(const std::string &remote_id, const std::vector<uint8_t> &data);
  void OnPeerConnection(const std::string &remote_id, bool connected);
  void OnPeerError(const std::string &remote_id, int error_code, const std::string &error_message);

  EventState GetCurrentState() const;

  // 业务流程入口
  void HandleManualModeMonitorRequest();
  // 通用：执行动作并发送完成响应（合并了 SimulateStep 和 CompleteSync）
  void SimulateAction(const MessageKey &key);

  // 触发墙壁按钮事件（模拟物理按钮触发）
  void TriggerWallEvent(uint16_t message_id);

  // 通信接口
  bool SendResponse(uint16_t message_id, uint8_t sub_id, uint16_t status_code,
                    const std::vector<uint8_t> &payload = {});

  PoseItem GeneratePoseForCommand(const MessageKey &key);

  // 状态管理（连接状态和设备状态逻辑相关，使用同一个锁保护）
  EventState current_state_ = EventState::DEVICE_IDLE;
  std::string active_connection_;
  mutable std::mutex state_mutex_;

  // 通信组件
  std::shared_ptr<ZmqPeer> peer_;
  std::shared_ptr<IEventHandler> peer_event_handler_;
  std::shared_ptr<MessageRouter> message_router_;

  // 运行控制
  bool initialized_ = false;
  bool running_ = false;

  // 模拟的姿态状态
  PoseItem last_pose_;
  int calibration_pose_index_ = 0;
  int charging_pose_index_ = 0;
};

}  // namespace perception
