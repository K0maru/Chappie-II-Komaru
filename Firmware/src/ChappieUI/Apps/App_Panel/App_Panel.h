#if 1
#pragma once
#include "../AppTypedef.h"
#include "../../ChappieUIConfigs.h"

/**
 * @brief Create an App in name space 
 * 
 */
namespace App {

    std::string App_Panel_appName();
    void* App_Panel_appIcon();
    void App_Panel_onCreate();
    void App_Panel_onLoop();
    void App_Panel_onDestroy();
    void App_Panel_getBsp(void* bsp);

    /**
     * @brief Declace your App like this 
     * 
     */
    App_Declare(Panel);
}

#endif
