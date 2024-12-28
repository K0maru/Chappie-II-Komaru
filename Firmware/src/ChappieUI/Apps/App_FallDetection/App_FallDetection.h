/**
 * @file App_FallDetection.h
 * @brief 
 * @author K0maru (k0maru3@foxmail.com)
 * @version 1.0
 * @date 2024-12-25
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

    std::string App_FallDetection_appName();
    void* App_FallDetection_appIcon();
    void App_FallDetection_onCreate();
    void App_FallDetection_onLoop();
    void App_FallDetection_onDestroy();
    void App_FallDetection_getBsp(void* bsp);

    /**
     * @brief Declace your App like this 
     * 
     */
    App_Declare(FallDetection);
}

#endif
