/*
    fake_communication.cpp
    Copyright (C) 2017 Niryo
    All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "niryo_one_driver/fake_communication.h"

FakeCommunication::FakeCommunication(int hardware_version,rclcpp::Node::SharedPtr node)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Starting Fake Communication... It will just echo cmd into current position");

    this->hardware_version = hardware_version;
    this->node = node;

    double pos_0, pos_1, pos_2;
    node->get_parameter("stepper_1_home_position", pos_0);
    node->get_parameter("stepper_2_home_position", pos_1);
    node->get_parameter("stepper_3_home_position", pos_2);
   
    if (hardware_version == 1) {
        double pos_3;
        node->get_parameter("stepper_4_home_position", pos_3);

        echo_pos[0] = pos_0;
        echo_pos[1] = pos_1;
        echo_pos[2] = pos_2;
        echo_pos[3] = pos_3;
        echo_pos[4] = 0.0;
        echo_pos[5] = 0.0;
    }
    else if (hardware_version == 2) {
        echo_pos[0] = pos_0;
        echo_pos[1] = pos_1;
        echo_pos[2] = pos_2;
        echo_pos[3] = 0.0;
        echo_pos[4] = 0.0;
        echo_pos[5] = 0.0;
    }
}

int FakeCommunication::init()
{
    return 0; // success
}

void FakeCommunication::manageHardwareConnection()
{
    // no hardware, nothing to manage
}

void FakeCommunication::startHardwareControlLoop()
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Start hardware control loop : nothing to do in fake mode.");
}

void FakeCommunication::stopHardwareControlLoop()
{
    // nothing
}

void FakeCommunication::resumeHardwareControlLoop()
{
    // nothing
}

void FakeCommunication::synchronizeMotors(bool begin_traj)
{
    // nothing
}

bool FakeCommunication::isConnectionOk()
{
    return true;
}

int FakeCommunication::allowMotorsCalibrationToStart(int mode, std::string &result_message)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Motor calibration with mode : %d", mode);
    return 200;
}

void FakeCommunication::requestNewCalibration() 
{
    // nothing
}

bool FakeCommunication::isCalibrationInProgress()
{
    return false;
}

void FakeCommunication::sendPositionToRobot(const double cmd[6])
{
    for (int i = 0 ; i < 6 ; i++) {
        echo_pos[i] = cmd[i]; 
    }
}

void FakeCommunication::getCurrentPosition(double pos[6])
{
    for (int i = 0 ; i < 6 ; i++) {
        pos[i] = echo_pos[i];
    }
}

void FakeCommunication::getCurrentGripperPosition(double& pos)
{
    pos = gripper_pos; 
}

void FakeCommunication::getCurrentGripperEffort(double& eff)
{
    eff = 0.0; 
}
        
void FakeCommunication::addCustomDxlCommand(int motor_type, uint8_t id, uint32_t value,
        uint32_t reg_address, uint32_t byte_number)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Add custom Dxl command");
}

void FakeCommunication::rebootMotors()
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Reboot Motors");
}
        
void FakeCommunication::getHardwareStatus(bool *is_connection_ok, std::string &error_message, 
        int *calibration_needed, bool *calibration_in_progress,
        std::vector<std::string> &motor_names, std::vector<std::string> &motor_types,
        std::vector<int32_t> &temperatures, std::vector<double> &voltages,
        std::vector<int32_t> &hw_errors)
{
    //RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Get Hardware Status");
    *(is_connection_ok) = true;
    *(calibration_needed) = false;
    *(calibration_in_progress) = false;
}

void FakeCommunication::getFirmwareVersions(std::vector<std::string> &motor_names,
        std::vector<std::string> &firmware_versions)
{
    //RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Get firmware versions");
}

void FakeCommunication::activateLearningMode(bool activate)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Activate learning mode : %d", activate);
}

bool FakeCommunication::setLeds(std::vector<int> &leds, std::string &message)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Set leds");
    return true;
}
        
int FakeCommunication::pullAirVacuumPump(uint8_t id, uint16_t pull_air_position, uint16_t pull_air_hold_torque)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Pull air on vacuum pump with id : %03d", id);
    return VACUUM_PUMP_STATE_PULLED;
}
int FakeCommunication::pushAirVacuumPump(uint8_t id, uint16_t push_air_position)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Push air on vacuum pump with id : %03d", id);
    return VACUUM_PUMP_STATE_PUSHED;
}
        
int FakeCommunication::pingAndSetDxlTool(uint8_t id, std::string name)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Ping gripper with id : %03d", id);
    return TOOL_STATE_PING_OK;
}
int FakeCommunication::pingAndSetConveyor(uint8_t id, bool activate, std::string &message)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"activate stepper with id : %03d", id);
    return TOOL_STATE_PING_OK;
}
int FakeCommunication::moveConveyor(uint8_t id, bool activate, int16_t speed, int8_t direction, std::string &message){
	RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"move stepper with id : %03d", id);
    return TOOL_STATE_PING_OK;
}
int  FakeCommunication::updateIdConveyor(uint8_t old_id, uint8_t new_id, std::string &message) {
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"update  conveyor id  with id : %03d", new_id);
    return TOOL_STATE_PING_OK;
}

 void FakeCommunication::getConveyorFeedBack(uint8_t conveyor_id, bool* connection_state, bool* running, int16_t* speed, int8_t* direction)
 {
     //nothing 
 }

int FakeCommunication::openGripper(uint8_t id, uint16_t open_position, uint16_t open_speed, uint16_t open_hold_torque)
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Open gripper with id : %03d", id);
    return GRIPPER_STATE_OPEN;
}

int FakeCommunication::closeGripper(uint8_t id, uint16_t close_position, uint16_t close_speed, uint16_t close_hold_torque, uint16_t close_max_torque) 
{
    RCLCPP_INFO(rclcpp::get_logger("FakeCommunication"),"Close gripper with id : %03d", id);
    return GRIPPER_STATE_CLOSE;
}
