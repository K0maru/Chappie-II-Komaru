/**
 * @file App_Panel.cpp
 * @brief 
 * @author K0maru (k0maru3@foxmail.com)
 * @version 1.0
 * @date 2025-01-22
 * 
 * 
 * @par 修改日志:
 *  <Date>     | <Version> | <Author>       | <Description>                   
 * ----------------------------------------------------------------------------
 */
#if 1
#include "App_Panel.h"
#include "../../../ChappieBsp/Chappie.h"
#include "math.h"

volatile bool GRAVITY_LOST = false;
volatile bool GRAVITY_OVER = false;
volatile bool FALL_DOWN = false;
volatile bool MOTION_LESS = false;

lv_obj_t* lv_mpu;

extern QueueHandle_t mpu_queue;
extern TaskHandle_t task_mpu;
struct MPU6050_data_t {
    float Yaw;
    float Pitch;
    float Roll;
    float accelX;
    float accelY;
    float accelZ;
    float accelS;
    float accelXZ;
};
static MPU6050_data_t MPU6050_data;
static MPU6050_data_t MPU6050_data_receiver;

TaskHandle_t task_detect = NULL;
TaskHandle_t task_UI = NULL;
TaskHandle_t task_update = NULL;
static std::string app_name = "Panel";
static CHAPPIE* device;
static LGFX_Sprite* _screen;
//bool DataCollectionEnable = true;
static bool DetectionEnable = true;

//lv_obj_t * sw_data = NULL;
lv_obj_t * sw_det = NULL;
lv_timer_t * det_timer = NULL;
#define GRAVITY_LOST_THRESHOLD 0.6
#define GRAVITY_OVER_THRESHOLD 2
#define PITCH_THRESHOLD 45
#define ROLL_THRESHOLD 45
#define scr_act_height() lv_obj_get_height(lv_scr_act())
#define scr_act_width() lv_obj_get_width(lv_scr_act())
static int looptimes = 0;

namespace App {

    /**
     * @brief Return the App name laucnher, which will be show on launcher App list
     * 
     * @return std::string 
     */
    std::string App_Panel_appName()
    {
        return app_name;
    }

/*-----------------------------------------------EVENT&TASK--------------------------------------------------------------------------*/
    /**
     * @brief Return the App Icon laucnher, NULL for default
     * 
     * @return void* 
     */
    void* App_Panel_appIcon()
    {
        return NULL;
    }
    void IRAM_ATTR GravityLostInterrupt() {
        GRAVITY_LOST = true;
        UI_LOG("[%s] GRAVITY_LOST\n", App_Panel_appName().c_str());
    }
    void IRAM_ATTR GravityOverInterrupt() {
        GRAVITY_OVER = true;
        UI_LOG("[%s] GRAVITY_OVER\n", App_Panel_appName().c_str());
    }
    void IRAM_ATTR MotionLessInterrupt() {
        MOTION_LESS = true;
        UI_LOG("[%s] MOTION_LESS\n", App_Panel_appName().c_str());
    } 
    void IRAM_ATTR FallDownInterrupt(){
        FALL_DOWN = true;
        UI_LOG("[%s] FALL_DOWN\n", App_Panel_appName().c_str());
        while(FALL_DOWN){
            device->Speaker.tone(9000, 300);
            
            if(device->Button.B.pressed()){
                GRAVITY_LOST = false;
                GRAVITY_OVER = false;
                MOTION_LESS = false;
                FALL_DOWN = false;
            }
            delay(10);
        }
        
        
    }
    
    /**
     * @brief  通过UI_LOG输出任务状态
     * @param  task_handler     任务句柄
     */
    void App_Panel_TaskStateCheck(TaskHandle_t task_handler){
        static eTaskState TaskState;
        TaskState = eTaskStateGet(task_handler);
        switch (TaskState) {
            case eRunning:
                UI_LOG("[%s] Task is Running\n", App_Panel_appName().c_str());
                break;
            case eReady:
                UI_LOG("[%s] Task is Ready\n", App_Panel_appName().c_str());
                break;
            case eBlocked:
                UI_LOG("[%s] Task is Blocked\n", App_Panel_appName().c_str());
                break;
            case eSuspended:
                UI_LOG("[%s] Task is Suspended\n", App_Panel_appName().c_str());
                break;
            case eDeleted:
                UI_LOG("[%s] Task is Deleted\n", App_Panel_appName().c_str());
                break;
            default:
                UI_LOG("[%s] Invalid State\n", App_Panel_appName().c_str());
                break;
        }
    }
    /**
     * @brief 需要被保持的线程，用于持续获取MPU6050的数据。通过创建一个深度为5的消息队列来方便数据处理和保持。
     * 
     */
    static void task_mpu6050_data(void* param)
    {
        mpu_queue = xQueueCreate(5,sizeof(MPU6050_data));
        ESP_LOGI(App_Panel_appName().c_str(),"mpu_queue created");
        //UI_LOG("[%s] mpu_queue created\n", App_Panel_appName().c_str());
        while(1){
            device->Imu.getYawPitchRoll(MPU6050_data.Yaw,MPU6050_data.Pitch,MPU6050_data.Roll);
            device->Imu.getAcceler(MPU6050_data.accelX,MPU6050_data.accelY,MPU6050_data.accelZ);
            MPU6050_data.accelS = sqrt(MPU6050_data.accelX*MPU6050_data.accelX+
                                       MPU6050_data.accelY*MPU6050_data.accelY+
                                       MPU6050_data.accelZ*MPU6050_data.accelZ);
            MPU6050_data.accelXZ = sqrt(MPU6050_data.accelX*MPU6050_data.accelX+MPU6050_data.accelZ*MPU6050_data.accelZ);
            xQueueSend(mpu_queue, &MPU6050_data, portMAX_DELAY);
            vTaskDelay(10); //100hz
        }
        
    }
    
    static void task_falldetect(void* param)
    {
        TickType_t startTime = 0; // 记录开始时间
        while(1){
            if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
                if(GRAVITY_LOST){
                    if(GRAVITY_OVER){
                        TickType_t MotionlessStart = 0;
                        if(MOTION_LESS){
                            if(abs(MPU6050_data_receiver.Pitch) > 45 || abs(MPU6050_data_receiver.Roll) > 45){
                                FallDownInterrupt();
                            }
                            else{
                                ESP_LOGI(App_Panel_appName().c_str(),"Error detection");
                                GRAVITY_LOST = false;
                                GRAVITY_OVER = false;
                                MOTION_LESS = false;
                            }
                        }
                        if(MotionlessStart == 0&&MOTION_LESS == false) MotionlessStart = xTaskGetTickCount();
                        if(((xTaskGetTickCount() - MotionlessStart) >= pdMS_TO_TICKS(1500)||(xTaskGetTickCount() - MotionlessStart) <= pdMS_TO_TICKS(2500))&&
                           (MPU6050_data_receiver.accelS >= 0.9||MPU6050_data_receiver.accelS<=1.1)){
                            MotionLessInterrupt();
                        }
                        else{
                            GRAVITY_OVER = false;
                            GRAVITY_LOST = false;
                        }
                        

                    }
                    /*失重后，0.5s内检测是否进入超重状态（跌倒）*/
                    if(startTime == 0&&GRAVITY_OVER == false) startTime = xTaskGetTickCount();
                    if((xTaskGetTickCount() - startTime) < pdMS_TO_TICKS(500) && GRAVITY_OVER == false){
                        if(MPU6050_data_receiver.accelS > GRAVITY_OVER_THRESHOLD){
                            GravityOverInterrupt();
                        }
                    }
                    else if(GRAVITY_OVER == false){
                        startTime = 0;
                        GRAVITY_LOST = false;
                    }
                }
                else if(MPU6050_data_receiver.accelS < GRAVITY_LOST_THRESHOLD && GRAVITY_LOST == false){
                    GravityLostInterrupt();
                }
            }
            else{
                UI_LOG("[%s] Waitting for data\n", App_Panel_appName().c_str());
            }
            vTaskDelay(20);
        }
    }

    void mpu_value_update(lv_timer_t* timer){

        MPU6050_data_t* data = (MPU6050_data_t*)timer->user_data;
        ESP_LOGI("DEBUG","Yaw: %.2f\nPitch: %.2f\nRoll: %.2f\nAS: %.2f", data->Yaw,data->Pitch,data->Roll,data->accelS);
        lv_obj_set_size(lv_mpu, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_text_fmt(lv_mpu, "Yaw: %.2f\nPitch: %.2f\nRoll: %.2f\nAS: %.2f", data->Yaw,data->Pitch,data->Roll,data->accelS);//格式化显示输出
        lv_obj_align(lv_mpu, LV_ALIGN_LEFT_MID, 0, 0);     //显示坐标设置
        lv_obj_invalidate(lv_mpu); 
    }
    /**
     * @brief detect和collect功能开关的handler
     * @param  e                事件
     */
    static void lv_sw_handler(lv_event_t *e){
        lv_obj_t * obj = lv_event_get_target(e);
        if(obj == sw_det){
            ESP_LOGI(App_Panel_appName().c_str(),"SW_detection handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
                if(task_detect == NULL){
                    xTaskCreate(task_falldetect, "MPU6050_DET", 1024*16, NULL, 4, &task_detect);
                    ESP_LOGI(App_Panel_appName().c_str(),"task_falldetect has been created");
                    det_timer = lv_timer_create(mpu_value_update,20,&MPU6050_data_receiver);
                    ESP_LOGI(App_Panel_appName().c_str(),"det_timer has been created");
                }
                DetectionEnable = true;
            }
            else{
                DetectionEnable = false;
                lv_timer_del(det_timer);
                det_timer = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"det_timer has been detected");
                vTaskDelete(task_detect);
                ESP_LOGI(App_Panel_appName().c_str(),"task_detect has been detected");
                task_detect = NULL;
            }
        }
        // else if(obj == sw_data){
        //     ESP_LOGI(App_Panel_appName().c_str(),"SW_datacollection handler called back");
        //     if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
        //         if(task_mpu == NULL){
        //             xTaskCreatePinnedToCore(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu, 0);
        //             //xTaskCreate(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu);
        //         }
        //         DetectionEnable = true;
        //         ESP_LOGI(App_Panel_appName().c_str(),"DetectionEnable set true");
        //     }
        //     else{
        //         DetectionEnable = false;
        //         ESP_LOGI(App_Panel_appName().c_str(),"DetectionEnable set false");
        //         // vTaskDelete(task_mpu);
        //         // ESP_LOGI(App_Panel_appName().c_str(),"task_mpu has been detected");
        //         task_mpu = NULL;
        //     }
        // }
    }

/*----------------------------------------------------UI&DEVICE-------------------------------------------------------------------*/
    /**
     * @brief init screen
     */
    static void testscreen_init()
    {
        _screen = new LGFX_Sprite(&device->Lcd);
        _screen->setPsram(true);
        _screen->createSprite(device->Lcd.width(), device->Lcd.height());
        _screen->setTextScroll(true);
        _screen->setTextSize(1.2);

        device->Lcd.fillScreen(TFT_BLACK);
        device->Lcd.setCursor(0, 0);
        device->Lcd.setTextSize(1.5);
        device->Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    static void testscreen_deinit()
    {
        _screen->deleteSprite();
        delete _screen;
    }
    /**
     * @brief 简易测试界面（不启用）
     */
    static void task_UI_loop()
    {
        //UI_LOG("[%s] in loop for %d times\n", App_Panel_appName().c_str(),looptimes++);
        if(FALL_DOWN){
            _screen->fillScreen(TFT_BLACK);
            _screen->setTextSize(2);
            _screen->setTextColor(TFT_ORANGE);
            _screen->setCursor(0, 30);
            _screen->printf(" > if ok, press B\n");
            _screen->pushSprite(0, 0);
        }
        else if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
            //UI_LOG("[%s] got data\n", App_Panel_appName().c_str());
            _screen->fillScreen(TFT_BLACK);
            _screen->setTextSize(2);
            _screen->setTextColor(TFT_ORANGE);
            _screen->setCursor(0, 30);
            _screen->printf(" > Yaw: %.1f\n > Pitch: %.1f\n > Row: %.1f\n", MPU6050_data_receiver.Yaw, 
                                                                        MPU6050_data_receiver.Pitch, 
                                                                        MPU6050_data_receiver.Roll);
            _screen->printf(" > AX:  %.1f\n > AY:    %.1f\n > AZ:  %.1f\n", MPU6050_data_receiver.accelX,
                                                                        MPU6050_data_receiver.accelY,
                                                                        MPU6050_data_receiver.accelZ);
            _screen->printf(" > AS:  %.1f\n",MPU6050_data_receiver.accelS);
            _screen->printf(" > try to detect,may be\n");
            _screen->pushSprite(0, 0);
        }
        else{
            UI_LOG("[%s] no data\n", App_Panel_appName().c_str());
            _screen->fillScreen(TFT_BLACK);
            _screen->setTextSize(2);
            _screen->setTextColor(TFT_ORANGE);
            _screen->setCursor(0, 30);
            _screen->printf(" > Error\n");
            _screen->pushSprite(0, 0);
        }
        delay(10);
    }
    /**
     * @brief App界面
     */
    static void App_Panel_tileview(void)
    {
        lv_obj_t * tv = lv_tileview_create(lv_scr_act());

        /*Tile1: 显示跌倒次数和开关*/
        lv_obj_t * tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_BOTTOM);
        //sw_data = lv_switch_create(tile1);
        
        //sw_det = lv_switch_create(tile1);
        
        lv_obj_t * label = NULL;

        /* 基础对象（矩形背景） */
        lv_obj_t *obj_det = lv_obj_create(tile1);                               /* 创建基础对象 */
        lv_obj_set_size(obj_det,scr_act_height() / 2, scr_act_height() / 2 );          /* 设置大小 */
        lv_obj_align(obj_det, LV_ALIGN_CENTER, -scr_act_width() / 4, 0 );              /* 设置位置 */


        /* 开关标签 */
        lv_obj_t *label_det = lv_label_create(obj_det);                               /* 创建标签 */
        lv_label_set_text(label_det, "Detection");                                          /* 设置文本内容 */
        lv_obj_align(label_det, LV_ALIGN_CENTER, 0, -scr_act_height() / 16 );          /* 设置位置 */

        /* 开关 */
        sw_det = lv_switch_create(obj_det);                                       /* 创建开关 */
        lv_obj_set_size(sw_det,scr_act_height() / 6, scr_act_height() / 12 );      /* 设置大小 */
        lv_obj_align(sw_det, LV_ALIGN_CENTER, 0, scr_act_height() / 16 );          /* 设置位置 */
        if(DetectionEnable){lv_obj_add_state(sw_det,LV_STATE_CHECKED);}
        lv_obj_add_event_cb(sw_det, lv_sw_handler, LV_EVENT_VALUE_CHANGED, NULL);/* 添加事件 */

        /* 基础对象（矩形背景） */
        // lv_obj_t *obj_data = lv_obj_create(tile1);
        // lv_obj_set_size(obj_data,scr_act_height() / 2, scr_act_height() / 2 );
        // lv_obj_align(obj_data, LV_ALIGN_CENTER, scr_act_width() / 4, 0 );

        /* 开关标签 */
        // lv_obj_t *label_data = lv_label_create(obj_data);
        // lv_label_set_text(label_data, "Collection");
        // lv_obj_align(label_data, LV_ALIGN_CENTER, 0, -scr_act_height() / 16 );


        /* 开关 */
        // sw_data = lv_switch_create(obj_data);
        // lv_obj_set_size(sw_data ,scr_act_height() / 6, scr_act_height() / 12 );
        // lv_obj_align(sw_data , LV_ALIGN_CENTER, 0, scr_act_height() / 16 );
        // //if(DataCollectionEnable){lv_obj_add_state(sw_data,LV_STATE_CHECKED);}
        // lv_obj_add_event_cb(sw_data, lv_sw_handler, LV_EVENT_VALUE_CHANGED, NULL);/* 添加事件 */

        //lv_obj_center(sw_det);
        /*Tile2：实时显示调试信息*/
        lv_obj_t * tile2 = lv_tileview_add_tile(tv, 0, 1, LV_DIR_TOP);
        lv_mpu = lv_label_create(tile2);
        
        det_timer = lv_timer_create(mpu_value_update,20,&MPU6050_data_receiver);
    }
    /**
     * @brief Called when App is on create
     * 
     */
    void App_Panel_onCreate()
    {
        UI_LOG("[%s] onCreate\n", App_Panel_appName().c_str());
        
        static bool falldown = false;
        testscreen_init();
        /*数据收集线程未启动且设置要求启动时，创建数据收集线程*/
        if(task_mpu == NULL){
            ESP_LOGI(App_Panel_appName().c_str(),"Try to create MPUtask");
            //UI_LOG("[%s] Try to create MPUtask\n", App_Pedometer_appName().c_str());
            xTaskCreatePinnedToCore(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu, 0);
            //xTaskCreate(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu);
            App_Panel_TaskStateCheck(task_mpu);
            device->Lcd.printf("Data collection task has been created\n");
            //UI_LOG("[%s] Data collection task has been created\n", App_Pedometer_appName().c_str());
        }
        /*检测线程未启动且要求启动时，创建检测线程*/
        if(task_detect == NULL && DetectionEnable){
            UI_LOG("[%s] try to create Detectiontask\n", App_Panel_appName().c_str());

            xTaskCreate(task_falldetect, "MPU6050_DET", 1024*16, NULL, 4, &task_detect);
            App_Panel_TaskStateCheck(task_detect);
            device->Lcd.printf("Detection task has been created\n");
            UI_LOG("[%s] Detection task has been created\n", App_Panel_appName().c_str());
        }

        
        App_Panel_tileview();
        // while (1) {
        //     task_UI_loop();
        //     if (device->Button.B.pressed()){
        //         UI_LOG("[%s] Button has been pressed\n", App_Panel_appName().c_str());
        //         //vTaskDelete(task_UI);
                
        //         break;
        //     }
        //     //delay(10);
        // }
        // testscreen_deinit();

        // lv_obj_t * label = lv_label_create(lv_scr_act());
        // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        // lv_label_set_text(label, "Press B again to quit");
    }


    /**
     * @brief Called repeatedly, end this function ASAP! or the App management will be affected
     * If the thing you want to do takes time, try create a taak or lvgl timer to handle them.
     * Try use millis() instead of delay() here
     * 
     */
    void App_Panel_onLoop()
    {
    }


    /**
     * @brief Called when App is about to be destroy
     * Please remember to release the resourse like lvgl timers in this function
     * 
     */
    void App_Panel_onDestroy()
    {
        /*在选择关闭检测功能的情况下再删除task，不关闭的情况下要保持该task*/
        if(DetectionEnable == false){
            if(det_timer != NULL){
                lv_timer_del(det_timer);
                det_timer = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"det_timer Delete");
            }
            if(task_detect != NULL){
                vTaskDelete(task_detect);
                task_detect = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"Detection task has been destoryed");
            }
            
            //UI_LOG("[%s] Detection task has been destoryed\n", App_Panel_appName().c_str());
        }
        testscreen_deinit();
        UI_LOG("[%s] onDestroy\n", App_Panel_appName().c_str());
    }

    
    /**
     * @brief Launcher will pass the BSP pointer through this function before onCreate
     * 
     */
    void App_Panel_getBsp(void* bsp)
    {
        device = (CHAPPIE*)bsp;
    }
    
    
}

#endif