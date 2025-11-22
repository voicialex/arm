#include "ArmController.hpp"
#include "ArmControllerRouterTable.hpp"
#include "message/MessageFrame.hpp"
#include "message/EventState.hpp"
#include "log.hpp"
#include <chrono>
#include <thread>

#define LOG_TAG "ArmController"

namespace perception {

ArmController::ArmController() : last_pose_() {
  // 初始化默认位姿（全0，confidence=0表示无效）
  last_pose_.Reset();
}

ArmController::~ArmController() { Stop(); }

bool ArmController::Initialize(const std::string &endpoint) {
  if (initialized_) return true;

  try {
    ZmqEndpointConfig config;
    config.endpoint = endpoint;
    peer_ = std::make_shared<ZmqPeer>(config);

    class PeerEventHandlerImpl : public IEventHandler {
     public:
      explicit PeerEventHandlerImpl(ArmController *ctrl) : controller_(ctrl) {}
      void OnMessage(const std::string &remote_id, const std::vector<uint8_t> &data) override {
        if (controller_) controller_->OnPeerMessage(remote_id, data);
      }
      void OnConnection(const std::string &remote_id, bool connected) override {
        if (controller_) controller_->OnPeerConnection(remote_id, connected);
      }
      void OnError(const std::string &remote_id, int error_code, const std::string &error_message) override {
        if (controller_) controller_->OnPeerError(remote_id, error_code, error_message);
      }

     private:
      ArmController *controller_;
    };

    peer_event_handler_ = std::make_shared<PeerEventHandlerImpl>(this);
    peer_->RegisterEventHandler(peer_event_handler_);

    message_router_ = std::make_shared<MessageRouter>();
    message_router_->InitializeRoutes(ArmControllerRouterTable::Get(this));

    current_state_ = EventState::DEVICE_IDLE;

    initialized_ = true;
    LOG_INFO_STREAM(LOG_TAG) << "ArmController 初始化完成";
    return true;
  } catch (const std::exception &e) {
    LOG_ERROR_STREAM(LOG_TAG) << "初始化失败: " << e.what();
    return false;
  }
}

bool ArmController::Start() {
  if (!initialized_ || running_) return running_;

  if (!peer_->Start()) {
    LOG_ERROR_STREAM(LOG_TAG) << "ZMQ 节点启动失败";
    return false;
  }

  running_ = true;
  LOG_INFO_STREAM(LOG_TAG) << "ArmController 启动完成";
  return true;
}

void ArmController::Stop() {
  if (!running_) return;

  running_ = false;
  if (peer_) peer_->Stop();

  LOG_INFO_STREAM(LOG_TAG) << "ArmController 已停止";
}

void ArmController::OnPeerMessage(const std::string &remote_id, const std::vector<uint8_t> &data) {
  LOG_DEBUG_STREAM(LOG_TAG) << "收到消息: 来源=" << remote_id << ", 大小=" << data.size() << " 字节";

  if (!message_router_) {
    LOG_ERROR_STREAM(LOG_TAG) << "消息路由器未初始化";
    return;
  }

  try {
    // 使用路由器处理消息并路由到对应的处理函数
    std::string error_msg;
    if (!message_router_->ProcessMessageData(data, &error_msg)) {
      LOG_WARN_STREAM(LOG_TAG) << "消息路由失败: " << error_msg;
    }
  } catch (const std::exception &e) {
    LOG_ERROR_STREAM(LOG_TAG) << "消息处理异常: " << e.what();
  }
}

void ArmController::OnPeerConnection(const std::string &remote_id, bool connected) {
  std::lock_guard<std::mutex> lock(state_mutex_);

  if (connected) {
    active_connection_ = remote_id;
    current_state_ = EventState::DEVICE_IDLE;
    LOG_INFO_STREAM(LOG_TAG) << "连接建立: " << remote_id << " - 状态: " << GetEventStateDescription(current_state_);
  } else if (active_connection_ == remote_id) {
    active_connection_.clear();
    current_state_ = EventState::DEVICE_DISCONNECTED;
    LOG_WARN_STREAM(LOG_TAG) << "连接断开: " << remote_id << " - 状态: " << GetEventStateDescription(current_state_);
  }
}

void ArmController::OnPeerError(const std::string &remote_id, int error_code, const std::string &error_message) {
  LOG_ERROR_STREAM(LOG_TAG) << remote_id << " 通信错误(" << error_code << "): " << error_message;
}

EventState ArmController::GetCurrentState() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return current_state_;
}

void ArmController::HandleManualModeMonitorRequest() {
  // 路由表中已发送 PROCESSING 响应，这里直接执行动作
  MessageKey key;
  key.message_id = MessageIds::MANUAL_REMOTE_CONTROL;
  key.sub_message_id = SubMessageIds::IDLE;
  key.status = StatusCode::PROCESSING;
  SimulateAction(key);
  LOG_INFO_STREAM(LOG_TAG) << "进入手动监控模式";
}

bool ArmController::SendResponse(uint16_t message_id, uint8_t sub_id, uint16_t status_code,
                                 const std::vector<uint8_t> &payload) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (!peer_ || active_connection_.empty()) {
    LOG_WARN_STREAM(LOG_TAG) << "无法发送响应: 连接未建立";
    return false;
  }

  auto notify =
      Serializer::CreateMessage(MessageType::Notify, message_id, sub_id, /*device_id*/ 1, status_code, payload);
  if (!notify) {
    LOG_ERROR_STREAM(LOG_TAG) << "创建消息失败";
    return false;
  }

  bool sent = peer_->Send(notify->Serialize());
  if (sent) {
    EventState event_state =
        static_cast<EventState>(MakeEventState(message_id, sub_id, static_cast<uint8_t>(status_code)));
    // 如果 payload 是位姿信息（24字节 = 6个float），在响应日志中包含位姿信息
    if (!payload.empty() && payload.size() == 6 * sizeof(float)) {
      PoseItem pose;
      if (PoseItem::DeserializeFromPayload(payload, pose)) {
        LOG_INFO_STREAM(LOG_TAG) << "发送响应: " << GetEventStateDescription(event_state) << " - " << pose;
      } else {
        LOG_INFO_STREAM(LOG_TAG) << "发送响应: " << GetEventStateDescription(event_state);
      }
    } else {
      LOG_INFO_STREAM(LOG_TAG) << "发送响应: " << GetEventStateDescription(event_state);
    }
  } else {
    LOG_WARN_STREAM(LOG_TAG) << "发送响应失败";
  }
  return sent;
}

void ArmController::SimulateAction(const MessageKey &key) {
  EventState event_state = static_cast<EventState>(MakeEventState(key.message_id, key.sub_message_id, key.status));
  LOG_DEBUG_STREAM(LOG_TAG) << "处理请求: " << GetEventStateDescription(event_state);

  // 生成 payload：如果是移动命令，生成新位姿；否则使用 last_pose_
  std::vector<uint8_t> payload;
  const bool is_moving_command = key.sub_message_id == SubMessageIds::ARM_MOVING_TO_INIT_POSITION ||
                                 key.sub_message_id == SubMessageIds::ARM_MOVING_TO_CALIB_POSITION ||
                                 key.sub_message_id == SubMessageIds::ARM_MOVING_TO_TARGET_POSITION ||
                                 key.sub_message_id == SubMessageIds::ARM_MOVING_TO_RESET_POSITION;

  if (is_moving_command) {
    PoseItem pose = GeneratePoseForCommand(key);
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_pose_ = pose;
    }
    payload = pose.SerializeToPayload();
  } else {
    // 使用 last_pose_（如果存在）
    std::lock_guard<std::mutex> lock(state_mutex_);
    payload = last_pose_.SerializeToPayload();
  }

  // 同步延迟（模拟处理时间）
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // 发送完成响应（带 Pose 数据）
  SendResponse(key.message_id, key.sub_message_id, StatusCode::COMPLETED, payload);
}

void ArmController::TriggerWallEvent(uint16_t message_id) {
  // 模拟物理按钮触发：直接向 Vision 发送 Notify 消息
  // 状态码使用 PROCESSING 表示这是一个触发动作
  LOG_INFO_STREAM(LOG_TAG) << "模拟墙壁按钮触发: " << MessageIds::ToString(message_id);
  SendResponse(message_id, SubMessageIds::IDLE, StatusCode::PROCESSING);
}

PoseItem ArmController::GeneratePoseForCommand(const MessageKey &key) {
  PoseItem pose;
  const double base_z = 0.25;
  const bool is_calibration = key.message_id == MessageIds::CALIBRATION;
  const bool is_charging = key.message_id == MessageIds::START_CHARGING;

  if (is_calibration) {
    if (key.sub_message_id == SubMessageIds::ARM_MOVING_TO_INIT_POSITION) {
      calibration_pose_index_ = 0;
      pose.position = PoseItem::Position(0.30, -0.10, base_z);
      pose.rotation = PoseItem::Rotation(0.0, 0.0, 0.0);
    } else if (key.sub_message_id == SubMessageIds::ARM_MOVING_TO_CALIB_POSITION) {
      const double offset = 0.01 * static_cast<double>(calibration_pose_index_);
      pose.position = PoseItem::Position(0.30 + offset, -0.10 + offset * 0.5, base_z + offset * 0.2);
      pose.rotation = PoseItem::Rotation(0.05 * calibration_pose_index_, -0.03 * calibration_pose_index_,
                                         0.02 * calibration_pose_index_);
      ++calibration_pose_index_;
    }
  } else if (is_charging) {
    if (key.sub_message_id == SubMessageIds::ARM_MOVING_TO_INIT_POSITION) {
      charging_pose_index_ = 0;
      pose.position = PoseItem::Position(0.40, 0.05, base_z + 0.05);
      pose.rotation = PoseItem::Rotation(0.0, 0.0, 0.0);
    } else if (key.sub_message_id == SubMessageIds::ARM_MOVING_TO_TARGET_POSITION) {
      const double offset = 0.015 * static_cast<double>(charging_pose_index_);
      pose.position = PoseItem::Position(0.42 + offset, 0.06 - offset * 0.4, base_z + 0.04 - offset * 0.1);
      pose.rotation =
          PoseItem::Rotation(0.04 * charging_pose_index_, 0.02 * charging_pose_index_, -0.01 * charging_pose_index_);
      ++charging_pose_index_;
    }
  }

  pose.confidence = 1.0f;
  return pose;
}

}  // namespace perception
