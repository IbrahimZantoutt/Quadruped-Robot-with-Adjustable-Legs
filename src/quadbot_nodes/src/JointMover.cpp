// Drives the legs via joint_trajectory_controller by publishing a standard
// trajectory_msgs/JointTrajectory: a set of joint angles + a duration to reach
// them. This node alternates between two poses so you can see the smooth motion.
//
// Joint order MUST match config/quadbot_controllers.yaml:
//   [0..2]  leg1 front-left : hip, knee, ankle
//   [3..5]  leg2 back-left  : hip, knee, ankle
//   [6..8]  leg3 front-right: hip, knee, ankle
//   [9..11] leg4 back-right : hip, knee, ankle

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"

// this is a comment from when i first transfered to the champquadbot proj first,  remeber before continuiing to know what sim to use, so that the slipping and exploding and twitching joints problems dont repeat again
// search online for tutorials on custom quadrupeds first, then what to use and the phy.

using namespace std::chrono_literals;

class JointMover : public rclcpp::Node
{
public:
  JointMover() : Node("joint_mover")
  {
    pub_ = create_publisher<trajectory_msgs::msg::JointTrajectory>(
      "/joint_trajectory_controller/joint_trajectory", 10);

    // Don't publish here directly: at construction time the controller hasn't
    // discovered this publisher yet, so the message would be dropped. Instead
    // poll until the controller is connected, send ONE trajectory, then stop.
    timer_ = create_wall_timer(200ms, [this]() {
      if (pub_->get_subscription_count() == 0) {
        return;  // controller not subscribed yet — wait for discovery
      }
      send_pose(use_first_ ? pose_ : pose_1, MOVE_TIME);
      timer_->cancel();  // one-shot: send the trajectory only once
    });

    RCLCPP_INFO(get_logger(),
                "Waiting for joint_trajectory_controller, then sending one trajectory");
  }

private:
  void send_pose(const std::vector<double> & angles, double duration_sec)
  {
    trajectory_msgs::msg::JointTrajectory msg;
    msg.joint_names = joint_names_;

    trajectory_msgs::msg::JointTrajectoryPoint point;
    point.positions = angles;                       // the target angles
    point.time_from_start.sec =                     // when to arrive (the duration)
      static_cast<int32_t>(duration_sec);
    point.time_from_start.nanosec =
      static_cast<uint32_t>((duration_sec - point.time_from_start.sec) * 1e9);

    msg.points.push_back(point);
    pub_->publish(msg);
    RCLCPP_INFO(get_logger(), "Commanded a pose (reach in %.1f s)", duration_sec);
  }

  static constexpr double MOVE_TIME = 3.0;  // seconds per move

  rclcpp::Publisher<trajectory_msgs::msg::JointTrajectory>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  bool use_first_ = true;

  std::vector<std::string> joint_names_ = {
    "leg1_mainConnection", "leg1_2Rotation", "leg1_3Rotation",
    "leg2_mainConnection", "leg2_2Rotation", "leg2_3Rotation",
    "leg3_mainConnection", "leg3_2Rotation", "leg3_3Rotation",
    "leg4_mainConnection", "leg4_2Rotation", "leg4_3Rotation",
  };

  std::vector<double> pose_ = {
    0.0, 0.5, 0.0,   // leg1 front-left
    0.0, 0.5, 0.0,   // leg2 back-left
    0.0, 0.5, 0.0,   // leg3 front-right
    0.0, 0.5, 0.0,   // leg4 back-right
  };
  std::vector<double> pose_1 = {
    0.6, -1.2, 0.4,   // leg1 front-left
    -0.6, 1.2, -0.4,  // leg2 back-left
    0.6, -1.2, 0.4,   // leg3 front-right
    -0.6, 1.2, -0.4,  // leg4 back-right
  };
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<JointMover>());
  rclcpp::shutdown();
  return 0;
}
