#include "tm_imu/tm_imu_node.hpp"

EasyObjectDictionary eOD;
EasyProfile eP(&eOD);

#ifdef DEBUG_MODE
#define DEBUG_MODE_PRINT_TIMER_MS_  (10000)  // 10 seconds
#endif

TMSerial::TMSerial() : rclcpp::Node("tm_imu")
{
    this->declare_parameter("imu_baudrate",    921600);
    this->declare_parameter("imu_port",        "/dev/ttyUSB0");
    this->declare_parameter("imu_frame_id",    "imu_link");
    this->declare_parameter("parent_frame_id", "chassis_link");
    this->declare_parameter("timer_period",     5);            // Unit: ms
    this->declare_parameter("transform",        std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    serialib1 = new serialib;
    SerialportOpen();
    #ifdef DEBUG_MODE
    count    = 0;
    count2   = 0;
    timer_10 = this->create_wall_timer(std::chrono::milliseconds(DEBUG_MODE_PRINT_TIMER_MS_), std::bind(&TMSerial::TimerCallback2, this));
    #endif
    imu_data_msg.header.frame_id     = this->get_parameter("imu_frame_id").as_string();
    imu_data_rpy_msg.header.frame_id = this->get_parameter("imu_frame_id").as_string();
    imu_data_mag_msg.header.frame_id = this->get_parameter("imu_frame_id").as_string();
    publisher_IMU = this->create_publisher<sensor_msgs::msg::Imu>(
        "imu/data",
        rclcpp::SensorDataQoS()
    );

    publisher_IMU_RPY = this->create_publisher<sensor_msgs::msg::MagneticField>(
        "imu/data_rpy",
        rclcpp::SensorDataQoS()
    );

    publisher_IMU_MAG = this->create_publisher<sensor_msgs::msg::MagneticField>(
        "imu/data_mag",
        rclcpp::SensorDataQoS()
    );
    std::chrono::milliseconds period = std::chrono::milliseconds(this->get_parameter("timer_period").as_int());
    timer_ = this->create_wall_timer(period, std::bind(&TMSerial::TimerCallback, this));
    tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);
}


TMSerial::~TMSerial()
{
    if(serialib1){
        serialib1->closeDevice();
        delete serialib1;
        serialib1 = 0;
    }
}


void TMSerial::TimerCallback()
{
    bool res = OnSerialRX();
    if (!res) return;
    imu_data_msg.header.stamp     = this->get_clock()->now();
    imu_data_rpy_msg.header.stamp = this->get_clock()->now();
    imu_data_mag_msg.header.stamp = this->get_clock()->now();
    FillCovarianceMatrices();
    //PublishTransform(); //changed this
    publisher_IMU->publish(imu_data_msg);
    publisher_IMU_RPY->publish(imu_data_rpy_msg);
    publisher_IMU_MAG->publish(imu_data_mag_msg);
}


#ifdef DEBUG_MODE
void TMSerial::TimerCallback2()
{
    RCLCPP_INFO(this->get_logger(),
    "Rx byte cnt=%d, TrasnducerM pkg cnt = %d (%f Hz)", count2, count, 1000*((float)count)/(DEBUG_MODE_PRINT_TIMER_MS_));
    count = 0;
    count2 = 0;
}
#endif


char TMSerial::SerialportOpen()
{
    int Ret;
    unsigned int baudrate = this->get_parameter("imu_baudrate").as_int();
    Ret=serialib1->openDevice(this->get_parameter("imu_port").as_string().c_str(), baudrate,
        SERIAL_DATABITS_8, SERIAL_PARITY_NONE, SERIAL_STOPBITS_1);
    if (Ret!=1) {
        RCLCPP_INFO(this->get_logger(), "Error while opening port. Permission problem ?");
        RCLCPP_INFO(this->get_logger(), "imu_port:%s imu_baudrate:%d",this->get_parameter("imu_port").as_string().c_str(),baudrate);
        return Ret;
    }
    RCLCPP_INFO(this->get_logger(), "Serial port opened successfully !");
    RCLCPP_INFO(this->get_logger(), "imu_port:%s imu_baudrate:%d",this->get_parameter("imu_port").as_string().c_str(),baudrate);
    return 1;
}


bool TMSerial::OnSerialRX()
{
    char serialBuffer[1024];
    int ret = serialib1->readBytes(serialBuffer, sizeof(serialBuffer), 1, 1);
    #ifdef DEBUG_MODE
    count2 += ret;
    #endif
    if (ret <= 0) return false;

    char*  rxData = serialBuffer;
    int    rxSize = ret;
    Ep_Header header;
    while(EP_SUCC_ == eP.On_RecvPkg(rxData, rxSize, &header)){
        rxData = 0;
        rxSize = 0;

        uint32 fromId = header.fromId;
        (void)fromId;

        switch(header.cmd){

            case EP_CMD_ACK_:{
                Ep_Ack ep_Ack;
                if(EP_SUCC_ == eOD.Read_Ep_Ack(&ep_Ack)){}
            }break;

            case EP_CMD_STATUS_:{
                Ep_Status ep_Status;
                if(EP_SUCC_ == eOD.Read_Ep_Status(&ep_Status)){}
            }break;

            case EP_CMD_Raw_GYRO_ACC_MAG_: {
                Ep_Raw_GyroAccMag ep_Raw_GyroAccMag;

                if (EP_SUCC_ == eOD.Read_Ep_Raw_GyroAccMag(&ep_Raw_GyroAccMag)) {
                    static constexpr float GRAVITY_MS2 = 9.80665f;

                    // TM210 RAW:
                    // gyro: rad/s
                    // acc: g
                    // mag: one earth magnetic field
                    float gyro_x = ep_Raw_GyroAccMag.gyro[0];
                    float gyro_y = ep_Raw_GyroAccMag.gyro[1];
                    float gyro_z = ep_Raw_GyroAccMag.gyro[2];

                    float acc_x = ep_Raw_GyroAccMag.acc[0] * GRAVITY_MS2;
                    float acc_y = ep_Raw_GyroAccMag.acc[1] * GRAVITY_MS2;
                    float acc_z = ep_Raw_GyroAccMag.acc[2] * GRAVITY_MS2;

                    float mag_x = ep_Raw_GyroAccMag.mag[0];
                    float mag_y = ep_Raw_GyroAccMag.mag[1];
                    float mag_z = ep_Raw_GyroAccMag.mag[2];

                    imu_data_msg.angular_velocity.x = gyro_x;
                    imu_data_msg.angular_velocity.y = gyro_y;
                    imu_data_msg.angular_velocity.z = gyro_z;

                    imu_data_msg.linear_acceleration.x = acc_x;
                    imu_data_msg.linear_acceleration.y = acc_y;
                    imu_data_msg.linear_acceleration.z = acc_z;

                    imu_data_mag_msg.magnetic_field.x = mag_x;
                    imu_data_mag_msg.magnetic_field.y = mag_y;
                    imu_data_mag_msg.magnetic_field.z = mag_z;

                    RCLCPP_INFO_THROTTLE(
                        this->get_logger(), *this->get_clock(), 1000,
                        "[RAW]\n"
                        "  acc_raw=[%.4f %.4f %.4f] g -> m/s2=[%.4f %.4f %.4f] |norm|=%.4f\n"
                        "  gyro=[%.6f %.6f %.6f] rad/s\n"
                        "  mag=[%.6f %.6f %.6f] earth-field",
                        ep_Raw_GyroAccMag.acc[0],
                        ep_Raw_GyroAccMag.acc[1],
                        ep_Raw_GyroAccMag.acc[2],
                        acc_x, acc_y, acc_z,
                        std::sqrt(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z),
                        gyro_x, gyro_y, gyro_z,
                        mag_x, mag_y, mag_z
                    );
                }
            } break;

            case EP_CMD_Q_S1_S_:{
                Ep_Q_s1_s ep_Q_s1_s;
                if(EP_SUCC_ == eOD.Read_Ep_Q_s1_s(&ep_Q_s1_s)){}
            }break;

            case EP_CMD_Q_S1_E_:{
                Ep_Q_s1_e ep_Q_s1_e;
                if(EP_SUCC_ == eOD.Read_Ep_Q_s1_e(&ep_Q_s1_e)){
                    float q1 = ep_Q_s1_e.q[0];
                    float q2 = ep_Q_s1_e.q[1];
                    float q3 = ep_Q_s1_e.q[2];
                    float q4 = ep_Q_s1_e.q[3];

                    imu_data_msg.orientation.w = q1;
                    imu_data_msg.orientation.x = q2;
                    imu_data_msg.orientation.y = q3;
                    imu_data_msg.orientation.z = q4;

                    // ── LOGGING ──────────────────────────────────────────
                    float qnorm = std::sqrt(q1*q1 + q2*q2 + q3*q3 + q4*q4);
                    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                        "[Q_S1_E] q(w,x,y,z)=[%.6f %.6f %.6f %.6f]  |norm|=%.6f",
                        q1, q2, q3, q4, qnorm);
                }
            }break;

            case EP_CMD_EULER_S1_S_:{
                Ep_Euler_s1_s ep_Euler_s1_s;
                if(EP_SUCC_ == eOD.Read_Ep_Euler_s1_s(&ep_Euler_s1_s)){}
            }break;

            case EP_CMD_EULER_S1_E_:{
                Ep_Euler_s1_e ep_Euler_s1_e;
                if(EP_SUCC_ == eOD.Read_Ep_Euler_s1_e(&ep_Euler_s1_e)){}
            }break;

            case EP_CMD_RPY_:{
                Ep_RPY ep_RPY;
                if(EP_SUCC_ == eOD.Read_Ep_RPY(&ep_RPY)){
                    float roll  = ep_RPY.roll;
                    float pitch = ep_RPY.pitch;
                    float yaw   = ep_RPY.yaw;

                    imu_data_rpy_msg.magnetic_field.x = roll;
                    imu_data_rpy_msg.magnetic_field.y = pitch;
                    imu_data_rpy_msg.magnetic_field.z = yaw;

                    // ── LOGGING ──────────────────────────────────────────
                    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                        "[RPY] roll=%.4f deg  pitch=%.4f deg  yaw=%.4f deg",
                        roll, pitch, yaw);
                }
            }break;

            case EP_CMD_GRAVITY_:{
                Ep_Gravity ep_Gravity;
                if(EP_SUCC_ == eOD.Read_Ep_Gravity(&ep_Gravity)){}
            }break;
            case EP_CMD_COMBO_: {
                Ep_Combo ep_Combo;
                if(EP_SUCC_ == eOD.Read_Ep_Combo(&ep_Combo)){
                    // Accelerometer:
                    float ax = (ep_Combo.ax)*(1e-5f);                     // Unit: 1g, 1g = 9.794m/(s^2)
                    float ay = (ep_Combo.ay)*(1e-5f);
                    float az = (ep_Combo.az)*(1e-5f);
                    // Gyroscope:
                    float wx = (ep_Combo.wx)*(1e-5f);                     // Unit: rad/s
                    float wy = (ep_Combo.wy)*(1e-5f);
                    float wz = (ep_Combo.wz)*(1e-5f);
                    // Magnetometer:
                    float mx = (ep_Combo.mx)*(1e-3f);                     // Unit: one earth magnetic field
                    float my = (ep_Combo.my)*(1e-3f);                     // vector (mx, my, mz) is used as direction reference of the local magnetic field.
                    float mz = (ep_Combo.mz)*(1e-3f);
                    // Quaternion in (q1,q2,q3,q4)=(w,x,y,z) format
                    float q1 = (ep_Combo.q1)*(1e-7f);
                    float q2 = (ep_Combo.q2)*(1e-7f);
                    float q3 = (ep_Combo.q3)*(1e-7f);
                    float q4 = (ep_Combo.q4)*(1e-7f);
                    // RPY:
                    float roll  = (ep_Combo.roll)*(1e-2f);                // Unit: degree
                    float pitch = (ep_Combo.pitch)*(1e-2f);               // Unit: degree
                    float yaw   = (ep_Combo.yaw)*(1e-2f);                 // Unit: degree

                    imu_data_msg.angular_velocity.x =  wx;
                    imu_data_msg.angular_velocity.y =  wy;
                    imu_data_msg.angular_velocity.z =  wz;

                    imu_data_msg.linear_acceleration.x = ax; 
                    imu_data_msg.linear_acceleration.y = ay;
                    imu_data_msg.linear_acceleration.z = az;

                    imu_data_msg.orientation.x = q1;
                    imu_data_msg.orientation.y = q2;
                    imu_data_msg.orientation.z = q3;
                    imu_data_msg.orientation.w = q4;

                    imu_data_rpy_msg.magnetic_field.x = roll;
                    imu_data_rpy_msg.magnetic_field.y = pitch;
                    imu_data_rpy_msg.magnetic_field.z = yaw;

                    imu_data_mag_msg.magnetic_field.x = mx;
                    imu_data_mag_msg.magnetic_field.y = my;
                    imu_data_mag_msg.magnetic_field.z = mz;
                    #ifdef DEBUG_MODE
                    //RCLCPP_INFO(this->get_logger(), "Combo pkg");
                    #endif
                    }
                } break;
            }                
        #ifdef DEBUG_MODE
        count++;
        #endif
    } // while()
    return true;
}


void TMSerial::FillCovarianceMatrices()
{
    for(int i = 0; i < 9; i++){
        imu_data_msg.orientation_covariance[i]        = 0.1;
        imu_data_msg.angular_velocity_covariance[i]   = 0.1;
        imu_data_msg.linear_acceleration_covariance[i]= 0.1;
        imu_data_rpy_msg.magnetic_field_covariance[i] = 0.1;
        imu_data_mag_msg.magnetic_field_covariance[i] = 0.1;
    }
}


void TMSerial::PublishTransform()
{
    geometry_msgs::msg::TransformStamped transform_;
    transform_.header.stamp = this->get_clock()->now();
    transform_.header.frame_id = "" + this->get_parameter("parent_frame_id").as_string();
    transform_.child_frame_id = this->get_parameter("imu_frame_id").as_string();
    transform_.transform.translation.x = this->get_parameter("transform").as_double_array()[0];
    transform_.transform.translation.y = this->get_parameter("transform").as_double_array()[1];
    transform_.transform.translation.z = this->get_parameter("transform").as_double_array()[2];

    transform_.transform.rotation.x = 0.0;
    transform_.transform.rotation.y = 0.0;
    transform_.transform.rotation.z = 0.0;
    transform_.transform.rotation.w = 1.0;
    tf_broadcaster_->sendTransform(transform_);
}


int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<TMSerial>());
    rclcpp::shutdown();
    return 0;
}