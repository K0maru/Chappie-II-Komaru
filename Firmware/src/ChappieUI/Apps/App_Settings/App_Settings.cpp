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
#define NTP1  "ntp1.aliyun.com"
#define NTP2  "ntp2.aliyun.com"
#define NTP3  "ntp3.aliyun.com"
static I2C_BM8563_TimeTypeDef rtc_time;
static I2C_BM8563_DateTypeDef rtc_date;
static std::string app_name = "Settings";
static CHAPPIE* device;


LV_IMG_DECLARE(ui_img_icon_setting_png);


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
    
    typedef enum {
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
    } lv_menu_builder_variant_t;

    // static void back_event_handler(lv_event_t * e);
    // static void switch_handler(lv_event_t * e);
    lv_obj_t * root_page;

    void set_sidebar_width(lv_obj_t* menu, lv_coord_t width);

    static lv_obj_t * create_text(lv_obj_t * parent, const char * icon, const char * txt,lv_menu_builder_variant_t builder_variant);
    static lv_obj_t * create_slider(lv_obj_t * parent,const char * icon, const char * txt, int32_t min, int32_t max, int32_t val);
    static lv_obj_t * create_switch(lv_obj_t * parent,const char * icon, const char * txt, bool chk);
    static lv_obj_t * create_WiFiInfo_table(lv_obj_t * parent, const char * ssid,const char * ip_adress,const char * wifi_status);
    /**
     * @brief 设置界面的多级菜单
     * @author K0maru3
     */
    void App_Settings_menu(void)
    {
        lv_obj_t * menu = lv_menu_create(lv_scr_act());
        lv_color_t bg_color = lv_obj_get_style_bg_color(menu, 0);
        if(lv_color_brightness(bg_color) > 127) {
            lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 10), 0);
        }
        else {
            lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 50), 0);
        }

        lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
        lv_obj_center(menu);

        lv_obj_t * cont;
        lv_obj_t * section;

        /*Create sub pages*/
        lv_obj_t * sub_WIFI_page = lv_menu_page_create(menu, NULL);
        lv_obj_set_style_pad_hor(sub_WIFI_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        section = lv_menu_section_create(sub_WIFI_page);
        //cont = create_WiFiInfo_table(section,WiFi.SSID().c_str(),WiFi.localIP().toString().c_str(),(const char *)WiFi.status());
        cont = create_WiFiInfo_table(section,"123","123","123");
        
        lv_menu_set_load_page_event(menu, cont, sub_WIFI_page);

        lv_obj_t * sub_BLE_page = lv_menu_page_create(menu, NULL);
        lv_obj_set_style_pad_hor(sub_BLE_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        lv_menu_separator_create(sub_BLE_page);
        section = lv_menu_section_create(sub_BLE_page);

        // create_switch(section, LV_SYMBOL_BLUETOOTH, "BLE", false);

        // lv_obj_t * sub_display_page = lv_menu_page_create(menu, NULL);
        // lv_obj_set_style_pad_hor(sub_display_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        // lv_menu_separator_create(sub_display_page);
        // section = lv_menu_section_create(sub_display_page);
        // brightness_slider = create_slider(section, LV_SYMBOL_SETTINGS, "Brightness", 0, 150, 100);
        // lv_obj_add_event_cb(brightness_slider,slider_control_event_cb,LV_EVENT_VALUE_CHANGED,NULL);

        lv_obj_t * sub_software_info_page = lv_menu_page_create(menu, NULL);
        lv_obj_set_style_pad_hor(sub_software_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        section = lv_menu_section_create(sub_software_info_page);
        create_text(section, NULL, "Version 1.1", LV_MENU_ITEM_BUILDER_VARIANT_1);

        lv_obj_t * sub_legal_info_page = lv_menu_page_create(menu, NULL);
        lv_obj_set_style_pad_hor(sub_legal_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        section = lv_menu_section_create(sub_legal_info_page);
        create_text(section, NULL,"Chappie-II modified by K0maru3",LV_MENU_ITEM_BUILDER_VARIANT_1);

        lv_obj_t * sub_about_page = lv_menu_page_create(menu, NULL);
        lv_obj_set_style_pad_hor(sub_about_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        lv_menu_separator_create(sub_about_page);
        section = lv_menu_section_create(sub_about_page);
        cont = create_text(section, NULL, "Software information", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(menu, cont, sub_software_info_page);
        cont = create_text(section, NULL, "Legal information", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(menu, cont, sub_legal_info_page);


        /*Create a root page*/
        root_page = lv_menu_page_create(menu, "Settings");
        lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
        section = lv_menu_section_create(root_page);

        /*WIFI页面计划用于显示Wifi连接情况*/
        cont = create_text(section, LV_SYMBOL_WIFI, "WIFI", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(menu, cont, sub_WIFI_page);
        
        /*BLE页面计划用于显示蓝牙连接情况*/
        cont = create_text(section, LV_SYMBOL_BLUETOOTH, "BLE", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(menu, cont, sub_BLE_page);

        // /*亮度调节和自动息屏*/
        // cont = create_text(section, LV_SYMBOL_SETTINGS, "Display", LV_MENU_ITEM_BUILDER_VARIANT_1);
        // lv_menu_set_load_page_event(menu, cont, sub_display_page);

        create_text(root_page, NULL, "Others", LV_MENU_ITEM_BUILDER_VARIANT_1);
        section = lv_menu_section_create(root_page);
        cont = create_text(section, NULL, "About", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(menu, cont, sub_about_page);
        // cont = create_text(section, LV_SYMBOL_SETTINGS, "Menu mode", LV_MENU_ITEM_BUILDER_VARIANT_1);
        // lv_menu_set_load_page_event(menu, cont, sub_menu_mode_page);
        lv_menu_set_sidebar_page(menu, root_page);
        set_sidebar_width(menu, 100);

        lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_event_send(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
                        NULL);

        
    }

    static lv_obj_t * create_text(lv_obj_t * parent, const char * icon, const char * txt,
                                lv_menu_builder_variant_t builder_variant)
    {
        lv_obj_t * obj = lv_menu_cont_create(parent);

        lv_obj_t * img = NULL;
        lv_obj_t * label = NULL;

        if(icon) {
            img = lv_img_create(obj);
            lv_img_set_src(img, icon);
        }

        if(txt) {
            label = lv_label_create(obj);
            lv_label_set_text(label, txt);
            lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
            //lv_obj_set_flex_grow(label, 2);
        }

        if(builder_variant == LV_MENU_ITEM_BUILDER_VARIANT_2 && icon && txt) {
            lv_obj_add_flag(img, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
            lv_obj_swap(img, label);
        }
        return obj;
    }
    /**
     * @brief 获取显示WIFI信息
     * @param  parent           
     * @param  ssid             
     * @param  ip_adress        
     * @param  wifi_state       
     * @return lv_obj_t* 
     */
    static lv_obj_t * create_WiFiInfo_table(lv_obj_t * parent, const char * ssid,const char * ip_adress,const char * wifi_status)
    {
        lv_obj_t * obj = lv_menu_cont_create(parent);
        
        lv_obj_t * table = lv_table_create(obj);
        lv_table_set_col_cnt(table, 2);
        /*Fill the first column*/
        lv_table_set_cell_value(table, 0, 0, "Type");
        lv_table_set_cell_value(table, 1, 0, "Status");
        lv_table_set_cell_value(table, 2, 0, "SSID");
        lv_table_set_cell_value(table, 3, 0, "IP");

        /*Fill the second column*/
        lv_table_set_cell_value(table, 0, 1, "Strings");
        lv_table_set_cell_value(table, 1, 1, wifi_status);
        lv_table_set_cell_value(table, 2, 1, ssid);
        lv_table_set_cell_value(table, 3, 1, ip_adress);

        lv_obj_set_size(table, 140, 160);
        return obj;
    }

    static lv_obj_t * create_switch(lv_obj_t * parent, const char * icon, const char * txt, bool chk)
    {
        lv_obj_t * obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1);

        lv_obj_t * sw = lv_switch_create(obj);
        lv_obj_add_state(sw, chk ? LV_STATE_CHECKED : 0);

        return obj;
    }
    /**
     * @brief Set the sidebar width object
     * @param  menu             
     * @param  width            
     */
    void set_sidebar_width(lv_obj_t* menu, lv_coord_t width) 
    {
        lv_menu_t* obj = (lv_menu_t*)menu;
        lv_obj_set_width(obj->sidebar, width);
    }

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

    /**
     * @brief 依靠settings app的创建来触发一次时间的同步
     * 
     */
    void App_Settings_onCreate()
    {

        UI_LOG("[%s] onCreate1\n", App_Settings_appName().c_str());
        UI_LOG("[WiFi] try to sync time\n");
        http.begin("http://quan.suning.com/getSysTime.do"); //HTTP begin
        int httpCode = http.GET();
            
        if (httpCode > 0)
        {
            // httpCode will be negative on error
            if (httpCode == HTTP_CODE_OK) // 收到正确的内容
            {
            UI_LOG("[HTTP] Connected\n");
            String resBuff = http.getString();
            UI_LOG("[HTTP] Getting string\n");
            char *pEnd;
            char charArray[100];
            char str [30];
            resBuff.toCharArray(charArray, 61);
            for(uint8_t i = 0;i<=15;i++){str[i]=charArray[i+46];}
            UI_LOG("[HTTP] string to charArray\n");
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
            UI_LOG("[SYS] rtc_date struct already\n");
            
            UI_LOG("[SYS] waiting applauncher restart\n");
            }
        }
        http.end();
        UI_LOG("[HTTP] disConnected\n");
        device->Rtc.setDate(&rtc_date);
        device->Rtc.setTime(&rtc_time);
        UI_LOG("[SYS] time sync done\n");
        App_Settings_menu();
        
        // xTaskCreatePinnedToCore(xTaskOne, "TaskOne", 4096*10, NULL, 1, NULL, 0);

        
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


