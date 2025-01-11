/**
 * @file ChappieUI.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-03-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "ChappieUI.h"
#include <nvs_flash.h>

int CHAPPIEUI::begin()
{
    if (_inited) {
        UI_LOG("[ChappieUI] already inited\n");
        return -1;
    }
    
    /* Create bsp */
    _device = new CHAPPIE;
    if (_device == NULL) {
        UI_LOG("[ChappieUI] bsp create failed\n");
        return -1;
    }
    
    /* Create launcher */
    _launcher = new App::App_Launcher(_device);
    if (_device == NULL) {
        UI_LOG("[ChappieUI] Launcher create failed\n");
        return -1;
    }

    _inited = true;
    
    /* Init device */
    _device->init();

    /* Init lvgl */
    _device->lvgl.init(&_device->Lcd, &_device->Tp);
    esp_err_t err = nvs_flash_init();
    
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 如果 NVS 空间不足或存在新版本的 NVS 数据，需要格式化 NVS
        ESP_ERROR_CHECK(nvs_flash_erase());  // 格式化 NVS
        err = nvs_flash_init();  // 再次初始化
    }
    /* Start launcher */
    _launcher->onCreate();
    
    return 0;
}


void CHAPPIEUI::update()
{
    _launcher->onLoop();
}

