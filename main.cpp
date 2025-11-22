/**
 * @file vision_arm_simulator_main.cpp
 * @brief 机械臂控制器模拟器入口程序
 *
 * 实现机械臂运动控制模拟节点，接收 VisionController 的充电相关指令并返回模拟响应。
 */

#include "ArmController.hpp"
#include "message/MessageFrame.hpp"
#include "log.hpp"
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <iostream>
#include <string>

#define LOG_TAG "ArmSim"

using namespace perception;

namespace {
std::atomic<bool> g_running{true};
}

void SignalHandler(int signal) {
  LOG_INFO_STREAM(LOG_TAG) << "收到信号: " << signal << ", 正在退出";
  g_running = false;
  // 触发 stdin 中断 (非标准，但在某些系统有效，或者依赖用户回车)
  std::cin.setstate(std::ios_base::eofbit);
}

int main() {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  ArmController simulator;
  // 使用默认的 IPC 地址
  std::string endpoint = "ipc:///tmp/vision_arm.ipc";
  if (!simulator.Initialize(endpoint)) {
    return 1;
  }

  if (!simulator.Start()) {
    return 1;
  }

  std::cout << "========================================" << std::endl;
  std::cout << "Arm Simulator CLI" << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  start   - Trigger WALL_START_CHARGING" << std::endl;
  std::cout << "  reset   - Trigger WALL_RESET" << std::endl;
  std::cout << "  estop   - Trigger WALL_EMERGENCY_STOP" << std::endl;
  std::cout << "  recovery - Trigger WALL_EMERGENCY_STOP_RECOVERY" << std::endl;
  std::cout << "  open    - Trigger WALL_OPEN_COVER" << std::endl;
  std::cout << "  close   - Trigger WALL_CLOSE_COVER" << std::endl;
  std::cout << "  mode    - Trigger WALL_SWITCH_MODE" << std::endl;
  std::cout << "  exit    - Exit simulator" << std::endl;
  std::cout << "========================================" << std::endl;

  std::string line;
  while (g_running.load()) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) break;

    if (line == "exit") {
      g_running = false;
      break;
    } else if (line == "start") {
      simulator.TriggerWallEvent(MessageIds::WALL_START_CHARGING);
    } else if (line == "reset") {
      simulator.TriggerWallEvent(MessageIds::WALL_RESET);
    } else if (line == "estop") {
      simulator.TriggerWallEvent(MessageIds::WALL_EMERGENCY_STOP);
    } else if (line == "recovery") {
      simulator.TriggerWallEvent(MessageIds::WALL_EMERGENCY_STOP_RECOVERY);
    } else if (line == "open") {
      simulator.TriggerWallEvent(MessageIds::WALL_OPEN_COVER);
    } else if (line == "close") {
      simulator.TriggerWallEvent(MessageIds::WALL_CLOSE_COVER);
    } else if (line == "mode") {
      simulator.TriggerWallEvent(MessageIds::WALL_SWITCH_MODE);
    } else if (!line.empty()) {
      std::cout << "Unknown command. Try 'start', 'reset', 'estop', 'open', 'close', 'mode', 'exit'." << std::endl;
    }
  }

  simulator.Stop();
  LOG_INFO_STREAM(LOG_TAG) << "Arm simulator stopped";
  return 0;
}
