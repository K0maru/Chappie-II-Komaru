/**
 * @file App_SedentaryDetection.h
 * @brief 
 * @author K0maru (k0maru3@foxmail.com)
 * @version 1.0
 * @date 2025-01-02
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

    std::string App_SedentaryDetection_appName();
    void* App_SedentaryDetection_appIcon();
    void App_SedentaryDetection_onCreate();
    void App_SedentaryDetection_onLoop();
    void App_SedentaryDetection_onDestroy();
    void App_SedentaryDetection_getBsp(void* bsp);

    /**
     * @brief Declace your App like this 
     * 
     */
    App_Declare(SedentaryDetection);
}

#endif
