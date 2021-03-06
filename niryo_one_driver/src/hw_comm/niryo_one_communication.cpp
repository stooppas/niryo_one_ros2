/*
    niryo_one_communication.cpp
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

#include "niryo_one_driver/niryo_one_communication.h"

using namespace std::chrono_literals;
NiryoOneCommunication::NiryoOneCommunication(int hardware_version,rclcpp::Node::SharedPtr node)
{
    this->hardware_version = hardware_version;
    this->node = node;
    
    node->get_parameter("can_enabled", can_enabled);
    node->get_parameter("dxl_enabled", dxl_enabled);
    node->get_parameter("niryo_one_hw_check_connection_frequency", niryo_one_hw_check_connection_frequency);

    if (!can_enabled) {
        RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"CAN communication is disabled for debug purposes");
    }
    if (!dxl_enabled) {
        RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"DXL communication is disabled for debug purposes");
    }

    if (can_enabled) {
        canComm.reset(new CanCommunication());
    }

    if (dxl_enabled) {
        dxlComm.reset(new DxlCommunication());
    }

    new_calibration_requested = false;
    niryo_one_comm_ok = false;
    can_comm_ok = false;
    dxl_comm_ok = false;
}

int NiryoOneCommunication::init()
{
    int result = 0;
    
    if (can_enabled) {
        result = canComm->init(hardware_version,node);
        if (result != 0) {
            return result;
        }
    }
    if (dxl_enabled) {
        result =  dxlComm->init(hardware_version,node);
        if (result != 0) {
            return result;
        }
    }
    
    return result;
}

bool NiryoOneCommunication::scanAndCheckMotors()
{
    bool result = true;
    if (can_enabled) {
        result = ((canComm->scanAndCheck() == CAN_SCAN_OK) && result); 
    }
    if (dxl_enabled) {
        result = ((dxlComm->scanAndCheck() == DXL_SCAN_OK) && result);
    }
    niryo_one_comm_ok = result;
    return result;
}

bool NiryoOneCommunication::isConnectionOk()
{
    bool result = true;
    if (can_enabled) {
        result = ((canComm->isConnectionOk()) && result);
    }
    if (dxl_enabled) {
        result = ((dxlComm->isConnectionOk()) && result);
    }
    niryo_one_comm_ok = result;
    return result;
}

void NiryoOneCommunication::manageCanConnectionLoop()
{
    if (!can_enabled) { 
        return; 
    }

    rclcpp::Rate check_connection_rate(niryo_one_hw_check_connection_frequency);
    bool motors_ok = false;

    while (rclcpp::ok()) {
        if (!canComm->isConnectionOk() || new_calibration_requested) {
            new_calibration_requested = false;
            RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Stop Can hw control");
            canComm->stopHardwareControlLoop();
            //ros::Duration(0.1).sleep();
            sleep_for(0.1);
           
            while (canComm->scanAndCheck() != CAN_SCAN_OK) { // wait for connection to be up
                RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Scan to find stepper motors...");
                sleep_for(0.25);
                //ros::Duration(0.25).sleep();
            }
            
            // once connected, set calibration flag
            RCLCPP_INFO(rclcpp::get_logger("NiryoOneCommunication"),"Set calibration flag");
            canComm->setCalibrationFlag(true);
                
            // deactivate motors (?)
            canComm->setTorqueOn(false);
    
            canComm->startHardwareControlLoop(true); // limited mode
            motors_ok = false;

            while (!motors_ok) {
                int calibration_step1_result = CAN_STEPPERS_CALIBRATION_FAIL;
                int calibration_step2_result = CAN_STEPPERS_CALIBRATION_FAIL;
                
                calibration_step1_result = canComm->calibrateMotors(1);
                if (calibration_step1_result == CAN_STEPPERS_CALIBRATION_OK) {
                    if (dxl_enabled) {
                        if (canComm->getCalibrationMode() == CAN_STEPPERS_CALIBRATION_MODE_AUTO) {
                            RCLCPP_INFO(rclcpp::get_logger("NiryoOneCommunication"),"Asking Dynamixel motors to go to home position");
                            dxlComm->moveAllMotorsToHomePosition();
                        }
                    }
                    calibration_step2_result = canComm->calibrateMotors(2);
                }

                if ((calibration_step1_result == CAN_STEPPERS_CALIBRATION_OK) 
                        && (calibration_step2_result == CAN_STEPPERS_CALIBRATION_OK)) {
                    motors_ok = true;
                    new_calibration_requested = false;
                    activateLearningMode(true);
                }
                else { // if calibration is not ok, wait and retry 
                    // check if connection is still ok
                    if (!canComm->isConnectionOk()) {
                        while (canComm->scanAndCheck() != CAN_SCAN_OK) { // wait for connection to be up
                            RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Scan to find stepper motors...");
                            //ros::Duration(0.25).sleep();
                            sleep_for(0.25);
                        }
                    }

                    // last calibration has failed, reset flag
                    if (calibration_step1_result != CAN_STEPPERS_CALIBRATION_WAITING_USER_INPUT) {
                        canComm->setCalibrationFlag(true);
                        // go back to limited mode (during calibration, hw control loop is stopped)
                        canComm->startHardwareControlLoop(true); 
                    }
                    sleep_for(0.25);
                    //ros::Duration(0.25).sleep();
                }
            }

            RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Resume can hw control");
            activateLearningMode(true);
            if (dxl_enabled) {
                canComm->startHardwareControlLoop(!dxlComm->isConnectionOk());
            }
            else {
                canComm->startHardwareControlLoop(false);
            }
        }
        else { // can connection ok + calibrated
            if (dxl_enabled && !dxlComm->isConnectionOk()) {
                if (!canComm->isOnLimitedMode()) {
                    canComm->startHardwareControlLoop(true);
                }
            }
            else {
                if (canComm->isOnLimitedMode()) {
                    canComm->setTorqueOn(false);
                    canComm->startHardwareControlLoop(false);
                }
            }
        }
        check_connection_rate.sleep();
    }
}



// stepper niryo one conveyor belt test is here
int NiryoOneCommunication::pingAndSetConveyor(uint8_t id, bool activate, std::string &message)
{
    if (can_enabled) {
        int res = canComm->setConveyor(id, activate); 
        if (res == CONVEYOR_STATE_SET_OK)
            message = " Set conveyor OK ";
        else 
            message = "Set conveyor ERROR"; 
        return res;
    }
    message = " Set conveyor ERROR ";
    return CONVEYOR_STATE_SET_ERROR;
}

// stepper niryo one conveyor belt test is here
int NiryoOneCommunication::moveConveyor(uint8_t id, bool activate, int16_t speed, int8_t direction, std::string &message)
{
    if (can_enabled) {
        int res = canComm->conveyorOn(id, activate, speed, direction);
        if (res == CONVEYOR_CONTROL_OK){
            message = "Command conveyor OK";
        }
        else 
            message = "Command conveyor ERROR";

        return res; 
    }
    message = "Can problem";
    return CONVEYOR_CONTROL_ERROR;
}
int NiryoOneCommunication::updateIdConveyor(uint8_t old_id, uint8_t new_id, std::string &message)
{
    if (can_enabled) {
        int res = canComm->updateConveyorId(old_id, new_id);
        if (res == CONVEYOR_UPDATE_ID_OK){
                message = " Update conveyor  OK ";
                return res; 
            }
        else{
                message = " Update conveyor  ERROR ";
                return res; 
            }
    }
    message = "Can problem";
    return CONVEYOR_UPDATE_ID_ERROR;
}


 void NiryoOneCommunication::getConveyorFeedBack(uint8_t conveyor_id, bool* connection_state, bool* running, int16_t* speed, int8_t* direction)
 {
     if (can_enabled) {
        canComm->getConveyorFeedBack(conveyor_id, connection_state, running, speed, direction);
    }
 }
void NiryoOneCommunication::checkHardwareVersionFromDxlMotors()
{
    // Check if hardware_version is compatible
    // The purpose here is retro-compatibility with version 1.
    // Version 2 is the default
    // If the robot is still V1 (old version) we can detect it from
    // Dynamixel motors setup, and automatically change the version
    // used, without any user input
    int detected_version = -1;
    detected_version = dxlComm->detectVersion();
    while (detected_version < 0) {
        RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Scan to find Dxl motors + Check hardware version");
        detected_version = dxlComm->detectVersion();
        sleep_for(0.25);
        //ros::Duration(0.25).sleep();
    }

    RCLCPP_INFO(rclcpp::get_logger("NiryoOneCommunication"),"Detected version from hardware : %d", detected_version);

    if (detected_version == 0) {
        // version could not be detected from current hardware setup
        // it seems that some motors have been disabled for debug purposes
        // --> continue like nothing happened
    }
    else if (  (detected_version == 1 && hardware_version == 2)
            || (detected_version == 2 && hardware_version == 1)) {
        // change version (V1->V2 or V2->V1) and reboot
        RCLCPP_ERROR(rclcpp::get_logger("NiryoOneCommunication"),"WRONG HARDWARE VERSION");

    }
}

void NiryoOneCommunication::manageDxlConnectionLoop()
{
    if (!dxl_enabled) {
        return;
    }

    checkHardwareVersionFromDxlMotors();

    rclcpp::Rate check_connection_rate(niryo_one_hw_check_connection_frequency);

    while (rclcpp::ok()) {
        if (!dxlComm->isConnectionOk()) {
            RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Stop Dxl hw control");
            dxlComm->stopHardwareControlLoop();
            //ros::Duration(0.1).sleep();
            sleep_for(0.1);

            while (dxlComm->scanAndCheck() != DXL_SCAN_OK) { // wait for connection to be up
                RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Scan to find Dxl motors");
                //ros::Duration(0.25).sleep();
                sleep_for(0.25);
            }

            RCLCPP_WARN(rclcpp::get_logger("NiryoOneCommunication"),"Resume Dxl hw control");
            dxlComm->setTorqueOn(false);
            activateLearningMode(true);
            if (can_enabled) {
                dxlComm->startHardwareControlLoop(!canComm->isConnectionOk());
            }
            else {
                dxlComm->startHardwareControlLoop(false);
            }
        }
        else { // dxl connection ok
            if (can_enabled && !canComm->isConnectionOk()) {
                if (!dxlComm->isOnLimitedMode()) {
                    dxlComm->startHardwareControlLoop(true);
                }
            }
            else {
                if (dxlComm->isOnLimitedMode()) {
                    dxlComm->setTorqueOn(false);
                    dxlComm->startHardwareControlLoop(false);
                }
            }
        }
        
        check_connection_rate.sleep();
    }
}

void NiryoOneCommunication::manageHardwareConnection()
{
    can_connection_loop_thread.reset(new std::thread(std::bind(&NiryoOneCommunication::manageCanConnectionLoop, this)));
    dxl_connection_loop_thread.reset(new std::thread(std::bind(&NiryoOneCommunication::manageDxlConnectionLoop, this)));
}

void NiryoOneCommunication::startHardwareControlLoop()
{
    if (can_enabled) { canComm->startHardwareControlLoop(false); }
    if (dxl_enabled) { dxlComm->startHardwareControlLoop(false); }
}

void NiryoOneCommunication::stopHardwareControlLoop()
{
    if (can_enabled) { canComm->stopHardwareControlLoop(); }
    if (dxl_enabled) { dxlComm->stopHardwareControlLoop(); }
}

void NiryoOneCommunication::resumeHardwareControlLoop()
{
    if (can_enabled) { canComm->startHardwareControlLoop(false); }
    if (dxl_enabled) { dxlComm->startHardwareControlLoop(false); }
}

void NiryoOneCommunication::synchronizeMotors(bool begin_traj)
{
    if (can_enabled) {
        canComm->synchronizeSteppers(begin_traj);
    }
}

int NiryoOneCommunication::allowMotorsCalibrationToStart(int mode, std::string &result_message)
{
    if (can_enabled) {
        if (mode == CAN_STEPPERS_CALIBRATION_MODE_MANUAL) {
            if (!canComm->canProcessManualCalibration(result_message)) {
                return 400;
            }
        }
        canComm->validateMotorsCalibrationFromUserInput(mode);
    }
    if (dxl_enabled) {
        // todo check dxl in bounds
    }

    result_message = "Calibration is starting";
    return 200;
}

void NiryoOneCommunication::requestNewCalibration()
{
    new_calibration_requested = true;
}

bool NiryoOneCommunication::isCalibrationInProgress()
{
    if (can_enabled) {
        return canComm->isCalibrationInProgress();
    }
    return false;
}

void NiryoOneCommunication::getHardwareStatus(bool *is_connection_ok, std::string &error_message,
        int *calibration_needed, bool *calibration_in_progress,
        std::vector<std::string> &motor_names, std::vector<std::string> &motor_types,
        std::vector<int32_t> &temperatures, std::vector<double> &voltages,
        std::vector<int32_t> &hw_errors)
{
    bool can_connection_ok = !can_enabled; // if CAN disabled, declare connection ok
    int can_calibration_needed = 0;
    bool can_calibration_in_progress = false;
    std::string can_error_message = "";
    std::vector<std::string> can_motor_names;
    std::vector<std::string> can_motor_types;
    std::vector<int32_t> can_temperatures;
    std::vector<double> can_voltages;
    std::vector<int32_t> can_hw_errors;

    bool dxl_connection_ok = !dxl_enabled; // if Dxl disabled, declare connection ok
    int dxl_calibration_needed = 0;
    bool dxl_calibration_in_progress = false;
    std::string dxl_error_message = "";
    std::vector<std::string> dxl_motor_names;
    std::vector<std::string> dxl_motor_types;
    std::vector<int32_t> dxl_temperatures;
    std::vector<double> dxl_voltages;
    std::vector<int32_t> dxl_hw_errors;

    if (can_enabled) {
        canComm->getHardwareStatus(&can_connection_ok, can_error_message, &can_calibration_needed,
                &can_calibration_in_progress, can_motor_names, can_motor_types, can_temperatures, can_voltages, can_hw_errors);
    }
    if (dxl_enabled) {
        dxlComm->getHardwareStatus(&dxl_connection_ok, dxl_error_message, &dxl_calibration_needed,
                &dxl_calibration_in_progress, dxl_motor_names, dxl_motor_types, dxl_temperatures, dxl_voltages, dxl_hw_errors);
    }

    motor_names.clear();
    motor_types.clear();
    temperatures.clear();
    voltages.clear();
    hw_errors.clear();

    motor_names.insert(motor_names.end(), can_motor_names.begin(), can_motor_names.end());
    motor_names.insert(motor_names.end(), dxl_motor_names.begin(), dxl_motor_names.end());
    motor_types.insert(motor_types.end(), can_motor_types.begin(), can_motor_types.end());
    motor_types.insert(motor_types.end(), dxl_motor_types.begin(), dxl_motor_types.end());
    temperatures.insert(temperatures.end(), can_temperatures.begin(), can_temperatures.end());
    temperatures.insert(temperatures.end(), dxl_temperatures.begin(), dxl_temperatures.end());
    voltages.insert(voltages.end(), can_voltages.begin(), can_voltages.end());
    voltages.insert(voltages.end(), dxl_voltages.begin(), dxl_voltages.end());
    hw_errors.insert(hw_errors.end(), can_hw_errors.begin(), can_hw_errors.end());
    hw_errors.insert(hw_errors.end(), dxl_hw_errors.begin(), dxl_hw_errors.end());

    *(is_connection_ok) = (can_connection_ok && dxl_connection_ok);
    *(calibration_needed) = (can_calibration_needed || dxl_calibration_needed);
    *(calibration_in_progress) = (can_calibration_in_progress || dxl_calibration_in_progress);
    error_message = "";
    error_message += can_error_message;
    if (dxl_error_message != "") {
        error_message += "\n";
    }
    error_message += dxl_error_message;
}

void NiryoOneCommunication::getFirmwareVersions(std::vector<std::string> &motor_names,
        std::vector<std::string> &firmware_versions)
{
    std::vector<std::string> can_firmware_versions;
    std::vector<std::string> can_motor_names;

    if (can_enabled) {
        canComm->getFirmwareVersions(can_motor_names, can_firmware_versions);
    }

    motor_names.clear();
    firmware_versions.clear();

    firmware_versions.insert(firmware_versions.end(), can_firmware_versions.begin(), can_firmware_versions.end());
    motor_names.insert(motor_names.end(), can_motor_names.begin(), can_motor_names.end());
}

void NiryoOneCommunication::getCurrentPosition(double pos[6])
{
    if (hardware_version == 1) {
        if (can_enabled) { canComm->getCurrentPositionV1(&pos[0], &pos[1], &pos[2], &pos[3]); }
        if (dxl_enabled) { dxlComm->getCurrentPositionV1(&pos[4], &pos[5]); }

        // if disabled (debug purposes)
        if (!can_enabled) {
            pos[0] = pos_can_disabled_v1[0];
            pos[1] = pos_can_disabled_v1[1];
            pos[2] = pos_can_disabled_v1[2];
            pos[3] = pos_can_disabled_v1[3];
        }

        if (!dxl_enabled) {
            pos[4] = pos_dxl_disabled_v1[0];
            pos[5] = pos_dxl_disabled_v1[1];
        }
    }
    else if (hardware_version == 2) {
        if (can_enabled) { canComm->getCurrentPositionV2(&pos[0], &pos[1], &pos[2]); }
        if (dxl_enabled) { dxlComm->getCurrentPositionV2(&pos[3], &pos[4], &pos[5]); }

        // if disabled (debug purposes)
        if (!can_enabled) {
            pos[0] = pos_can_disabled_v2[0];
            pos[1] = pos_can_disabled_v2[1];
            pos[2] = pos_can_disabled_v2[2];
        }

        if (!dxl_enabled) {
            pos[3] = pos_dxl_disabled_v2[0];
            pos[4] = pos_dxl_disabled_v2[1];
            pos[5] = pos_dxl_disabled_v2[2];
        }
    }
}

void NiryoOneCommunication::getCurrentGripperPosition(double& pos)
{
    if (dxl_enabled){
        dxlComm->getCurrentGripperPosition(pos);
    }
}

void NiryoOneCommunication::getCurrentGripperEffort(double& eff)
{
    if (dxl_enabled){
        dxlComm->getCurrentGripperEffort(eff);
    }
}

void NiryoOneCommunication::sendPositionToRobot(const double cmd[6])
{
    bool is_calibration_in_progress = false;
    if (can_enabled) {
        is_calibration_in_progress = canComm->isCalibrationInProgress();
    }

    // don't send position command when calibrating motors
    if (!is_calibration_in_progress) {
        if (hardware_version == 1) {
            if (can_enabled) { canComm->setGoalPositionV1(cmd[0], cmd[1], cmd[2], cmd[3]); }
            if (dxl_enabled) { dxlComm->setGoalPositionV1(cmd[4], cmd[5]); }

            // if disabled (debug purposes)
            if (!can_enabled) {
                pos_can_disabled_v1[0] = cmd[0];
                pos_can_disabled_v1[1] = cmd[1];
                pos_can_disabled_v1[2] = cmd[2];
                pos_can_disabled_v1[3] = cmd[3];
            }

            if (!dxl_enabled) {
                pos_dxl_disabled_v1[0] = cmd[4];
                pos_dxl_disabled_v1[1] = cmd[5];
            }
        }
        else if (hardware_version == 2) {
            if (can_enabled) { canComm->setGoalPositionV2(cmd[0], cmd[1], cmd[2]); }
            if (dxl_enabled) { dxlComm->setGoalPositionV2(cmd[3], cmd[4], cmd[5]); }

            // if disabled (debug purposes)
            if (!can_enabled) {
                pos_can_disabled_v2[0] = cmd[0];
                pos_can_disabled_v2[1] = cmd[1];
                pos_can_disabled_v2[2] = cmd[2];
            }

            if (!dxl_enabled) {
                pos_dxl_disabled_v2[0] = cmd[3];
                pos_dxl_disabled_v2[1] = cmd[4];
                pos_dxl_disabled_v2[2] = cmd[5];
            }
        }
    }
}

void NiryoOneCommunication::addCustomDxlCommand(int motor_type, uint8_t id, uint32_t value,
        uint32_t reg_address, uint32_t byte_number)
{
    if (dxl_enabled) {
        dxlComm->addCustomDxlCommand(motor_type, id, value, reg_address, byte_number);
    }
}

void NiryoOneCommunication::rebootMotors()
{
    // Only useful for Dynamixel motors
    if (dxl_enabled) { dxlComm->rebootMotors(); }
}

void NiryoOneCommunication::activateLearningMode(bool activate)
{
    if (can_enabled) { canComm->setTorqueOn(!activate); }
    if (dxl_enabled) { dxlComm->setTorqueOn(!activate); }
}

bool NiryoOneCommunication::setLeds(std::vector<int> &leds, std::string &message)
{
    if (leds.size() != 4) {
        message = "Led array must have 4 values";
        return false;
    }

    if (dxl_enabled) {
        dxlComm->setLeds(leds);
    }

    message = "Set LED ok";
    return true;
}

int NiryoOneCommunication::pullAirVacuumPump(uint8_t id, uint16_t pull_air_position, uint16_t pull_air_hold_torque)
{
    if (dxl_enabled) {
        return dxlComm->pullAirVacuumPump(id, pull_air_position, pull_air_hold_torque);
    }
    return VACUUM_PUMP_STATE_PULLED;
}

int NiryoOneCommunication::pushAirVacuumPump(uint8_t id, uint16_t push_air_position)
{
    if (dxl_enabled) {
        return dxlComm->pushAirVacuumPump(id, push_air_position);
    }
    return VACUUM_PUMP_STATE_PUSHED;
}

int NiryoOneCommunication::pingAndSetDxlTool(uint8_t id, std::string name)
{
    if (dxl_enabled) {
        return dxlComm->pingAndSetTool(id, name);
    }
    return TOOL_STATE_PING_OK;
}

int NiryoOneCommunication::openGripper(uint8_t id, uint16_t open_position, uint16_t open_speed, uint16_t open_hold_torque)
{
    if (dxl_enabled) {
        return dxlComm->openGripper(id, open_position, open_speed, open_hold_torque);
    }
    return GRIPPER_STATE_OPEN;
}

int NiryoOneCommunication::closeGripper(uint8_t id, uint16_t close_position, uint16_t close_speed, uint16_t close_hold_torque, uint16_t close_max_torque)
{
    if (dxl_enabled) {
        return dxlComm->closeGripper(id, close_position, close_speed, close_hold_torque, close_max_torque);
    }
    return GRIPPER_STATE_CLOSE;
}