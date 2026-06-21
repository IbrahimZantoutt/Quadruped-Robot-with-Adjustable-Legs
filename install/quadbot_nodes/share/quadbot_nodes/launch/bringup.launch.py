import os

from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription, RegisterEventHandler, ExecuteProcess,
)
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory


# ============================================================================
# Edit your launch pose here (radians). Order MUST match controllers.yaml:
#   leg1 front-left, leg2 back-left, leg3 front-right, leg4 back-right
#   each: [hip(main), knee(2), ankle(3)]
# All zeros = legs straight down (robot stands tall on its wheels).
# ============================================================================
SAFE_HOME_ANGLES = [
    0.6, 0.0, 0.0,   # leg1 front-left
    -0.6, 0.0, 0.0,   # leg2 back-left
    0.6, 0.0, 0.0,   # leg3 front-right
    -0.6, 0.0, 0.0,   # leg4 back-right
]

HOME_ANGLES = [
    0.6, -1.2, 0.4,   # leg1 front-left
    -0.6, 1.2, -0.4,   # leg2 back-left
    0.6, -1.2, 0.4,   # leg3 front-right
    -0.6, 1.2, -0.4,   # leg4 back-right
]

# Leg joints in the same order as HOME_ANGLES / controllers.yaml.
LEG_JOINTS = [
    'leg1_mainConnection', 'leg1_2Rotation', 'leg1_3Rotation',
    'leg2_mainConnection', 'leg2_2Rotation', 'leg2_3Rotation',
    'leg3_mainConnection', 'leg3_2Rotation', 'leg3_3Rotation',
    'leg4_mainConnection', 'leg4_2Rotation', 'leg4_3Rotation',
]

# Seconds the startup move into the home pose should take (smooth, not a snap).
HOME_MOVE_TIME = 2

# Height to spawn the base at. Natural standing height (straight legs) ~0.07 m,
# so this drops it ~3 cm onto its wheels. Raise it if you use a crouched pose.
SPAWN_Z = 0.11


def generate_launch_description():
    pkg = get_package_share_directory('quadbot_nodes')
    urdf_path = os.path.join(pkg, 'urdf', 'robot.urdf')
    rviz_path = os.path.join(pkg, 'rviz', 'model.rviz')

    robot_description = ParameterValue(
        Command(['xacro ', urdf_path]), value_type=str
    )

    # Gazebo (running, NOT paused — controllers can only ACTIVATE while sim time
    # advances, so a paused world makes the activation switch time out).
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('gazebo_ros'),
                         'launch', 'gazebo.launch.py')
        ),
    )

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description}],
    )

    rviz = Node(
        package='rviz2',
        executable='rviz2',
        output='screen',
        arguments=['-d', rviz_path],
    )

    spawn_entity = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        output='screen',
        arguments=['-topic', 'robot_description', '-entity', 'quadbot',
                   '-z', str(SPAWN_Z)],
    )

    joint_state_broadcaster = Node(
        package='controller_manager', executable='spawner',
        arguments=['joint_state_broadcaster'], output='screen',
    )

    trajectory_controller = Node(
        package='controller_manager', executable='spawner',
        arguments=['joint_trajectory_controller'], output='screen',
    )

    velocity_controller = Node(
        package='controller_manager', executable='spawner',
        arguments=['velocity_controller'], output='screen',
    )

    # Once the trajectory controller is active, send ONE trajectory that moves
    # the legs from their spawn pose to the home pose over HOME_MOVE_TIME seconds
    # (smooth, interpolated). JTC then holds the final pose rigidly.
    home_traj = (
        '{joint_names: [' + ', '.join(LEG_JOINTS) + '], '
        'points: [{positions: [' + ', '.join(str(a) for a in SAFE_HOME_ANGLES) + '], '
        'time_from_start: {sec: ' + str(HOME_MOVE_TIME) + ', nanosec: 0}}]}'
    )
    home_pose = ExecuteProcess(
        cmd=['ros2', 'topic', 'pub', '--once',
             '/joint_trajectory_controller/joint_trajectory',
             'trajectory_msgs/msg/JointTrajectory', home_traj],
        output='screen',
    )

    # Hold the wheels at 0 rad/s so they brake instead of free-rolling.
    wheel_hold = ExecuteProcess(
        cmd=['ros2', 'topic', 'pub', '--rate', '10', '--times', '10',
             '/velocity_controller/commands',
             'std_msgs/msg/Float64MultiArray',
             '{data: [0.0, 0.0, 0.0, 0.0]}'],
        output='screen',
    )

    return LaunchDescription([
        gazebo,
        robot_state_publisher,
        rviz,
        spawn_entity,
        RegisterEventHandler(OnProcessExit(
            target_action=spawn_entity, on_exit=[joint_state_broadcaster])),
        RegisterEventHandler(OnProcessExit(
            target_action=joint_state_broadcaster, on_exit=[trajectory_controller])),
        RegisterEventHandler(OnProcessExit(
            target_action=trajectory_controller, on_exit=[velocity_controller])),
        RegisterEventHandler(OnProcessExit(
            target_action=velocity_controller, on_exit=[home_pose, wheel_hold])),
    ])
