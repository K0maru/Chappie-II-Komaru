/**
 * @file App_Settings.cpp
 * @brief 
 * @author K0maru (k0maru3@foxmail.com)
 * @version 1.0
 * @date 2024-10-24
 * 
 * 
 * @par 修改日志:
 *  <Date>     | <Version> | <Author>       | <Description>                   
 * ----------------------------------------------------------------------------
 * 2024.10.24  | 1.0       |K0maru          |新增多级菜单Demo
 */
#if 1
#include "App_Settings.h"
#include "../../../ChappieBsp/Chappie.h"
#include <WiFi.h>
#include <HTTPClient.h>
/**
 * @brief 新增多级菜单必要组件，测试完后整理合并
 */
#include "../../../lib/lvgl/examples/lv_examples.h"
#include "../../../lib/lvgl/src/widgets/lv_btn.h"
#include "../../../lib/lvgl/src/widgets/lv_img.h"
#include "../../../lib/lvgl/src/extra/widgets/menu/lv_menu.h"
#include "../../../lib/lvgl/src/extra/widgets/msgbox/lv_msgbox.h"
#include "../../../lib/lvgl/src/core/lv_disp.h"
#include "../../../lib/lvgl/src/core/lv_obj.h"
#include "../../../lib/lvgl/src/core/lv_event.h"
#include "../../../lib/lvgl/src/extra/others/msg/lv_msg.h"

HTTPClient http;
static I2C_BM8563_TimeTypeDef rtc_time;
static I2C_BM8563_DateTypeDef rtc_date;
static std::string app_name = "Settings";
static CHAPPIE* device;

LV_IMG_DECLARE(ui_img_icon_setting_png);


static int extract_ints(char* str, int* nums, int max_count) {
    int len = strlen(str);
    int count = 0;
    int num = 0;
    for (int i = 0; i < len && count < max_count; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10 || i == 12) {
            nums[count++] = num;
            num = 0;
        }
        num = num * 10 + (str[i] - '0');
    }
    if (count < max_count) {
        nums[count++] = num;
    }
    return count;
}
static void xTaskOne(void *xTask1)
{
    while (1)
    {
        uint8_t i = 0;
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin();
        while (WiFi.status() != WL_CONNECTED)
	    { //这里是阻塞程序，直到连接成功
            vTaskDelay(100);
            i++;  
            if(i == 100)
            {
                //vTaskDelete(NULL);
                break;//等待10秒，若未连接，则退出联网
            }
        }
        i = 0;
        if(WiFi.status() != WL_CONNECTED)
        {       
            WiFi.beginSmartConfig();
        }
        if(WiFi.status() != WL_CONNECTED)
        {
            while (!WiFi.smartConfigDone()) 
            { 
                delay(500);
                i++;  
                if(i == 200)
                {
                    WiFi.disconnect();
                    vTaskDelete(NULL);//等待25秒，若仍未配网，则注销线程
                }
            }
            i = 0;
            vTaskDelay(100);
            while (WiFi.status() != WL_CONNECTED) 
            { 
                vTaskDelay(100);
                i++;  
                if(i == 100)
                {
                    WiFi.disconnect();
                    vTaskDelete(NULL);
                } 
            }
        }
    	http.begin("http://quan.suning.com/getSysTime.do"); //HTTP begin
    	int httpCode = http.GET();
        if (httpCode > 0)
        {
            // httpCode will be negative on error
            if (httpCode == HTTP_CODE_OK) // 收到正确的内容
            {
            String resBuff = http.getString();
            char *pEnd;
            char charArray[100];
            char str [30];
            resBuff.toCharArray(charArray, 61);
            for(i=0;i<=15;i++){str[i]=charArray[i+46];}

            int nums[6];
            extract_ints(str, nums, 6);

            rtc_date.year = nums[0];
            rtc_date.month = nums[1];
            rtc_date.date = nums[2];
            rtc_time.hours = nums[3];
            rtc_time.minutes = nums[4];
            rtc_time.seconds = nums[5]+1;//补偿数据处理的时间            
            uint16_t y = nums[0];
            uint8_t m = nums[1];
            uint8_t d = nums[2];
            if(m<3)
            {
                m+=12;
                y-=1;
            }
            rtc_date.weekDay=(d+2*m+3*(m+1)/5+y+y/4-y/100+y/400+1)%7;
            device->Rtc.setDate(&rtc_date);
            device->Rtc.setTime(&rtc_time);
            }
        }
        http.end();
        WiFi.disconnect();
        vTaskDelete(NULL);
    }
}


namespace App {

    /**
     * @brief Return the App name laucnher, which will be show on launcher App list
     * 
     * @return std::string 
     */
    std::string App_Settings_appName()
    {
        return app_name;
    }


    /**
     * @brief Return the App Icon laucnher, NULL for default
     * 
     * @return void* 
     */
    void* App_Settings_appIcon()
    {
        // return NULL;
        return (void*)&ui_img_icon_setting_png;
    }
    /**
     * @brief 设置界面的多级菜单
     * @author K0maru3
     */
    void App_Settings_menu(void)
    {
        /*Create a menu object*/
        lv_obj_t * menu = lv_menu_create(lv_scr_act());
        lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
        lv_obj_center(menu);

        lv_obj_t * cont;
        lv_obj_t * label;

        /*Create a sub page*/
        lv_obj_t * sub_page = lv_menu_page_create(menu, NULL);

        cont = lv_menu_cont_create(sub_page);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Hello, I am hiding here");

        /*Create a main page*/
        lv_obj_t * main_page = lv_menu_page_create(menu, NULL);

        cont = lv_menu_cont_create(main_page);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Item 1");

        cont = lv_menu_cont_create(main_page);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Item 2");

        cont = lv_menu_cont_create(main_page);
        label = lv_label_create(cont);
        lv_label_set_text(label, "Item 3 (Click me!)");
        lv_menu_set_load_page_event(menu, cont, sub_page);

        lv_menu_set_page(menu, main_page);
    }

    /**
     * @brief Called when App is on create
     * 
     */
    void App_Settings_onCreate()
    {
        UI_LOG("[%s] onCreate1\n", App_Settings_appName().c_str());
        App_Settings_menu();

        // /*Create an Arc*/
        // lv_obj_t * arc = lv_arc_create(lv_scr_act());
        // lv_obj_set_size(arc, 150, 150);
        // lv_arc_set_rotation(arc, 135);
        // lv_arc_set_bg_angles(arc, 0, 270);
        // lv_arc_set_value(arc, 40);
        // lv_obj_center(arc);
        // xTaskCreatePinnedToCore(xTaskOne, "TaskOne", 4096*10, NULL, 1, NULL, 0);

        
          /*rtc_date.year = 2023;
            rtc_date.month = 3;
            rtc_date.weekDay = 2;
            rtc_date.date = 11;
            rtc_time.hours = 9;
            rtc_time.minutes = 0;
            rtc_time.seconds = 3;
            device->Rtc.setDate(&rtc_date);
            device->Rtc.setTime(&rtc_time);*/
    }


    /**
     * @brief Called repeatedly, end this function ASAP! or the App management will be affected
     * If the thing you want to do takes time, try create a taak or lvgl timer to handle them.
     * Try use millis() instead of delay() here
     * 
     */
    void App_Settings_onLoop()
    {

    }


    /**
     * @brief Called when App is about to be destroy
     * Please remember to release the resourse like lvgl timers in this function
     * 
     */
    void App_Settings_onDestroy()
    {
        UI_LOG("[%s] onDestroy\n", App_Settings_appName().c_str());
    }


    /**
     * @brief Launcher will pass the BSP pointer through this function before onCreate
     * 
     */
    void App_Settings_getBsp(void* bsp)
    {
        device = (CHAPPIE*)bsp;
    }
    
}

#endif


