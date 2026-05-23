# Lab Catania ROS 2 Jazzy Setup

This guide explains how to install and start the ROS 2 Jazzy setup for the Lab Catania project using Distrobox, LIO-SAM, Velodyne and the TM IMU.

## Requirements

- Ubuntu 24.04
- Distrobox
- ROS 2 Jazzy container image
- Velodyne VLP-16
- TM IMU

ROS 2 Jazzy is used in this project, therefore Ubuntu 24.04 is required.

---

## Quick Start

### 1. Create and enter the ROS 2 Jazzy Distrobox container

Run this in the **host terminal**:

```bash
distrobox create --name ros-jazzy --image docker.io/osrf/ros:jazzy-desktop --pull
distrobox enter ros-jazzy
```

### 2. Install dependencies inside the Distrobox container

Run the following commands **inside the Distrobox container**:

```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
source ~/.bashrc

sudo apt update

sudo apt install -y \
    curl \
    gnupg \
    software-properties-common \
    python3-colcon-common-extensions \
    ros-dev-tools \
    ros-jazzy-navigation2 \
    ros-jazzy-nav2-bringup \
    ros-jazzy-robot-localization \
    ros-jazzy-robot-state-publisher \
    ros-jazzy-perception-pcl \
    ros-jazzy-pcl-msgs \
    ros-jazzy-vision-opencv \
    ros-jazzy-xacro \
    ros-jazzy-velodyne \
    libboost-all-dev \
    cmake \
    libmetis-dev \
    libeigen3-dev
```

### 3. Install GTSAM

```bash
sudo add-apt-repository ppa:borglab/gtsam-release-4.2
sudo apt update
sudo apt install -y libgtsam-dev libgtsam-unstable-dev
```

### 4. Create the ROS 2 workspace and clone the repository

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

git clone https://github.com/Spodymun/lab_catania
```

### 5. Install ROS dependencies

```bash
cd ~/ros2_ws

source /opt/ros/jazzy/setup.bash

rosdep install -i --from-path src --rosdistro jazzy -y
rosdep install --from-path src --ignore-src -r -y
```

### 6. Build the workspace

```bash
cd ~/ros2_ws

source /opt/ros/jazzy/setup.bash

colcon build --symlink-install --cmake-args -DGTSAM_DIR=/usr/lib/x86_64-linux-gnu/cmake/GTSAM

source install/setup.bash
```

### 7. Configure automatic sourcing

This makes sure that ROS 2 Jazzy and the workspace are sourced automatically whenever a new terminal is opened.

```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
echo "source ~/ros2_ws/install/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

---

## Velodyne Ethernet Settings

Set the Ethernet IPv4 settings manually:

```text
IP address: 10.0.1.1
Netmask:    255.255.255.0
```

After this, the Velodyne browser interface should be reachable at:

```text
http://10.0.1.7
```

If this does not connect, check the current Velodyne IP address. In our case, the IP address changed several times, so it may be necessary to find the correct IP manually.

---

## Starting the System

After the installation, start each component in its own terminal.

> **Important:** The launch order matters. The Velodyne and IMU need a short moment before LIO-SAM is started.

### Terminal 1: Start the IMU

```bash
ros2 launch tm_imu imu.launch.py
```

While launching the IMU, hold it as still as possible so that it can calibrate correctly.

### Terminal 2: Start the Velodyne driver

```bash
ros2 launch velodyne_driver velodyne_driver_node-VLP16-launch.py
```

### Terminal 3: Start the Velodyne pointcloud transformer

```bash
ros2 launch velodyne_pointcloud velodyne_transform_node-VLP16-launch.py
```

### Wait 10-15 seconds

Before starting LIO-SAM, wait around 10-15 seconds so that the IMU and Velodyne data streams are stable.

### Terminal 4: Start LIO-SAM

```bash
ros2 launch lio-sam run.launch.py
```

RViz2 should now open and start mapping the environment.

---

## Check the TF Tree

To check the TF tree, open a new terminal and run:

```bash
ros2 run tf2_tools view_frames
```

The resulting TF tree should look like the reference file in the repository:

```text
documentation/tf_chain.pdf
```


