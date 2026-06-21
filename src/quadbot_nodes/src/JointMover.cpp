// Publishes a single target pose (12 leg-joint angles, radians) to the
// direct-position controller.  Edit `pose_` below to try your own poses.
//
// Joint order MUST match config/quadbot_controllers.yaml:
//   [0..2]  leg1 front-left : hip, knee, ankle
//   [3..5]  leg2 back-left  : hip, knee, ankle
//   [6..8]  leg3 front-right: hip, knee, ankle
//   [9..11] leg4 back-right : hip, knee, ankle

#include <chrono>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"

using namespace std::chrono_literals;

class JointMover : public rclcpp::Node
{
public:
  JointMover() : Node("joint_mover")
  {
    pub_ = create_publisher<std_msgs::msg::Float64MultiArray>(
      "/position_controller/commands", 10);

    // ---- your custom pose (radians) ----
    pose_ = {
      0.0, 0.5, -1.0,   // front-left
      0.0, 0.5, -1.0,   // back-left
      0.0, 0.5, -1.0,   // front-right
      0.0, 0.5, -1.0,   // back-right
    };

    // republish at 2 Hz so the command always reaches the controller,
    // even if it started a moment after this node.
    timer_ = create_wall_timer(500ms, [this]() {
      std_msgs::msg::Float64MultiArray msg;
      msg.data = pose_;
      pub_->publish(msg);
    });

    RCLCPP_INFO(get_logger(), "Publishing pose to /position_controller/commands");
  }

private:
  rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::vector<double> pose_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<JointMover>());
  rclcpp::shutdown();
  return 0;
}
