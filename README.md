# LIO-SAM Catania ROS 2 Jazzy Setup

This guide explains how to install and start the ROS 2 Jazzy setup for the LIO-SAM Catania project using Distrobox.

## Requirements

- Ubuntu 24.04
- Distrobox
- ROS 2 Jazzy container image
- Velodyne VLP-16
- Transducer TM210 IMU

---

## Choose Your Installation Method

This guide focuses mainly on the quick-start installation using our prepared repository.  
However, it also includes the alternative steps for building the setup from scratch.

N.B: If you choose the from-scratch installation, follow the additional instructions below the install guide to apply the required changes manually.

### 1. Create and enter the ROS 2 Jazzy Distrobox container

Run this in the terminal:

```bash
distrobox create --name ros-jazzy --image docker.io/osrf/ros:jazzy-desktop --pull
distrobox enter ros-jazzy
```

### 2. Install dependencies inside the Distrobox container

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

## 4. Create the ROS 2 workspace and clone the repository

Create the ROS 2 workspace:

```bash
mkdir -p ~/ros2_LIO_SAM_ws/src
cd ~/ros2_LIO_SAM_ws/src
```

### Option A: Quick start with our prepared repository

Use this option if you want to use the already prepared project repository:

```bash
git clone https://github.com/Spodymun/lab_catania
```

### Option B: Build the setup from scratch

Use this option if you want to start from the original repositories and apply the required changes manually.

Clone LIO-SAM and Velodyne:

```bash
git clone https://github.com/pixwyh/LIO-SAM-ROS2.git
git clone https://github.com/ros-drivers/velodyne.git
```
Then apply the required `CMakeLists.txt` changes for LIO-SAM:

```bash
cd ~/ros2_LIO_SAM_ws/src/LIO-SAM-ROS2

python3 - <<'PY'
from pathlib import Path

p = Path("CMakeLists.txt")
text = p.read_text()

text = text.replace("find_package(Eigen REQUIRED)", "find_package(Eigen3 REQUIRED)")
text = text.replace("Eigen\n", "Eigen3\n")
text = text.replace("Eigen ", "Eigen3 ")

new_include = '''include_directories(
    include
    include/lio_sam
    ${PCL_INCLUDE_DIRS}
    ${Eigen3_INCLUDE_DIRS}
    "/usr/include/gtsam"
)
'''

if "include_directories(" in text:
    start = text.find("include_directories(")
    depth = 0
    end = None

    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                end = i + 1
                break

    if end is not None:
        text = text[:start] + new_include + text[end:]
else:
    text = new_include + "\n" + text

p.write_text(text)
print("CMakeLists.txt updated.")
PY
```

Then download the TransducerM ROS 2 example package from:

```text
https://www.syd-dynamics.com/download/transducerm_example_ros2-pkg/
```

After downloading it:

1. Go to your `Downloads` folder.
2. Unpack the downloaded archive.
3. Open the unpacked folder.
4. Go one folder deeper if there is another folder inside.
5. Rename the actual ROS 2 package folder to:

```text
tm_imu
```

6. Copy the renamed `tm_imu` folder into your workspace source folder:

```bash
cp -r ~/Downloads/tm_imu ~/ros2_LIO_SAM_ws/src/
```

If the renamed `tm_imu` folder is still inside another unpacked folder, use this pattern instead:

```bash
cp -r ~/Downloads/<UNPACKED_FOLDER>/<INNER_FOLDER>/tm_imu ~/ros2_LIO_SAM_ws/src/
```

At the end, your source folder should contain:

```text
~/ros2_LIO_SAM_ws/src/LIO-SAM-ROS2
~/ros2_LIO_SAM_ws/src/velodyne
~/ros2_LIO_SAM_ws/src/tm_imu
```


### 5. Install ROS dependencies

```bash
cd ~/ros2_LIO_SAM_ws

source /opt/ros/jazzy/setup.bash

rosdep install -i --from-path src --rosdistro jazzy -y
rosdep install --from-path src --ignore-src -r -y
```

### 6. Build the workspace

```bash
cd ~/ros2_LIO_SAM_ws

source /opt/ros/jazzy/setup.bash

colcon build --symlink-install --cmake-args -DGTSAM_DIR=/usr/lib/x86_64-linux-gnu/cmake/GTSAM

source install/setup.bash
```

### 7. Configure automatic sourcing

This makes sure that ROS 2 Jazzy and the workspace are sourced automatically whenever a new terminal is opened.

```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
echo "source ~/ros2_LIO_SAM_ws/install/setup.bash" >> ~/.bashrc
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

If this does not connect, check the current Velodyne IP address. In our case, the IP address changed several times from some colleague, so it may be necessary to find the correct IP manually.

---

## Starting the System

After the installation OF THE QUICK START, start each component in its own terminal. Otherwise you'll need to come back after doing the changes manually (under this chapter)

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

### Terminal 4: Start LIO-SAM

Before starting LIO-SAM, wait around 10-15 seconds so that the IMU and Velodyne data streams are stable.
```bash
ros2 launch LIO-SAM-ROS2 run.launch.py
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


