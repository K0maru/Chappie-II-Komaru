/**
 * @file App_Pedometer.h
 * @brief 
 * @author K0maru (k0maru3@foxmail.com)
 * @version 1.0
 * @date 2024-12-29
 * 
 * 
 * @par 修改日志:
 *  <Date>     | <Version> | <Author>       | <Description>                   
 * ----------------------------------------------------------------------------
 */
#if 1
#pragma once
#include "../AppTypedef.h"
#include "../../ChappieUIConfigs.h"

/**
 * @brief Create an App in name space 
 * 
 */
namespace App {

    std::string App_Pedometer_appName();
    void* App_Pedometer_appIcon();
    void App_Pedometer_onCreate();
    void App_Pedometer_onLoop();
    void App_Pedometer_onDestroy();
    void App_Pedometer_getBsp(void* bsp);

    /**
     * @brief Declace your App like this 
     * 
     */
    App_Declare(Pedometer);
}

#endif
