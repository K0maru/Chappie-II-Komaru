/**
 * @file App_FallDetection.cpp
 * @brief 
 * @author K0maru (k0maru3@foxmail.com)
 * @version 0.1
 * @date 2024-12-25
 * 
 * 
 * @par 修改日志:
 *  <Date>     | <Version> | <Author>       | <Description>                   
 * ----------------------------------------------------------------------------------
 * 2024.12.28  |    0.1    | K0maru         | 新增mpu数据获取(已测试)、跌倒检测(未测试)
 */
#if 1
#include "App_FallDetection.h"
#include "../../../ChappieBsp/Chappie.h"
#include "math.h"

volatile bool GRAVITY_LOST = false;
volatile bool GRAVITY_OVER = false;
volatile bool FALL_DOWN = false;
volatile bool MOTION_LESS = false;

QueueHandle_t mpu_queue = NULL;

struct MPU6050_data_t {
    float Yaw;
    float Pitch;
    float Roll;
    float accelX;
    float accelY;
    float accelZ;
    float accelS;
};
static MPU6050_data_t MPU6050_data;
static MPU6050_data_t MPU6050_data_receiver;
TaskHandle_t task_mpu = NULL;
TaskHandle_t task_detect = NULL;
TaskHandle_t task_UI = NULL;

static std::string app_name = "FallDetection";
static CHAPPIE* device;
static LGFX_Sprite* _screen;
static bool DataCollectionEnable = true;
static bool DetectionEnable = true;

#define GRAVITY_LOST_THRESHOLD 0.6
#define GRAVITY_OVER_THRESHOLD 2
#define PITCH_THRESHOLD 45
#define ROLL_THRESHOLD 45

static int looptimes = 0;

namespace App {

    /**
     * @brief Return the App name laucnher, which will be show on launcher App list
     * 
     * @return std::string 
     */
    std::string App_FallDetection_appName()
    {
        return app_name;
    }


    /**
     * @brief Return the App Icon laucnher, NULL for default
     * 
     * @return void* 
     */
    void* App_FallDetection_appIcon()
    {
        return NULL;
    }
    void IRAM_ATTR GravityLostInterrupt() {
        GRAVITY_LOST = true;
        UI_LOG("[%s] GRAVITY_LOST\n", App_FallDetection_appName().c_str());
    }
    void IRAM_ATTR GravityOverInterrupt() {
        GRAVITY_OVER = true;
        UI_LOG("[%s] GRAVITY_OVER\n", App_FallDetection_appName().c_str());
    }
    void IRAM_ATTR MotionLessInterrupt() {
        MOTION_LESS = true;
        UI_LOG("[%s] MOTION_LESS\n", App_FallDetection_appName().c_str());
    } 
    void IRAM_ATTR FallDownInterrupt(){
        FALL_DOWN = true;
        UI_LOG("[%s] FALL_DOWN\n", App_FallDetection_appName().c_str());
    }
    /**
     * @brief 需要被保持的线程，用于持续获取MPU6050的数据。通过创建一个深度为5的消息队列来方便数据处理和保持。
     * 
     */
    static void task_mpu6050_data(void* param)
    {
        mpu_queue = xQueueCreate(5,sizeof(MPU6050_data));
        UI_LOG("[%s] mpu_queue created\n", App_FallDetection_appName().c_str());
        while(1){
            device->Imu.getYawPitchRoll(MPU6050_data.Yaw,MPU6050_data.Pitch,MPU6050_data.Roll);
            device->Imu.getAcceler(MPU6050_data.accelX,MPU6050_data.accelY,MPU6050_data.accelZ);
            MPU6050_data.accelS = sqrt(MPU6050_data.accelX*MPU6050_data.accelX+
                                       MPU6050_data.accelY*MPU6050_data.accelY+
                                       MPU6050_data.accelZ*MPU6050_data.accelZ);
            // MPU6050_data.accelS = 1;
            xQueueSend(mpu_queue, &MPU6050_data, portMAX_DELAY);
            vTaskDelay(10); //100hz
        }
        
    }
    static void task_falldetect(void* param)
    {
        
        while(1){
            TickType_t startTime = 0; // 记录开始时间
            if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
                //UI_LOG("[%s] NOW the sum of Gravity is %f\n", App_FallDetection_appName().c_str(),MPU6050_data_receiver.accelS);
                if(GRAVITY_LOST){
                    if(GRAVITY_OVER){
                        TickType_t MotionlessStart = 0;
                        if(MOTION_LESS){
                            if(abs(MPU6050_data_receiver.Pitch) > 45 || abs(MPU6050_data_receiver.Roll) > 45){
                                FallDownInterrupt();
                            }
                            else{
                                UI_LOG("[%s] Error detection\n", App_FallDetection_appName().c_str());
                                GRAVITY_LOST = false;
                                GRAVITY_OVER = false;
                                MOTION_LESS = false;
                            }
                        }
                        if(MotionlessStart == 0&&MOTION_LESS == false) MotionlessStart = xTaskGetTickCount();
                        if((xTaskGetTickCount() - MotionlessStart) > pdMS_TO_TICKS(1500)&&MPU6050_data_receiver.accelS == 1){
                            MotionLessInterrupt();
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
                        GRAVITY_LOST = false;
                    }
                }
                else if(MPU6050_data_receiver.accelS < GRAVITY_LOST_THRESHOLD && GRAVITY_LOST == false){
                    GravityLostInterrupt();
                }
            }
            else{
                UI_LOG("[%s] Waitting for data\n", App_FallDetection_appName().c_str());
            }
            vTaskDelay(20);
        }
    }
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
    static void task_UI_loop()
    {
        //UI_LOG("[%s] in loop for %d times\n", App_FallDetection_appName().c_str(),looptimes++);
        if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
            //UI_LOG("[%s] got data\n", App_FallDetection_appName().c_str());
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
            UI_LOG("[%s] no data\n", App_FallDetection_appName().c_str());
            _screen->fillScreen(TFT_BLACK);
            _screen->setTextSize(2);
            _screen->setTextColor(TFT_ORANGE);
            _screen->setCursor(0, 30);
            _screen->printf(" > Error\n");
            _screen->pushSprite(0, 0);
        }
        // if(FALL_DOWN){
        //     _screen->fillScreen(TFT_BLACK);
        //     _screen->setTextSize(2);
        //     _screen->setTextColor(TFT_ORANGE);
        //     _screen->setCursor(0, 30);
        //     _screen->printf(" > Are you OK?\n");
        //     _screen->pushSprite(0, 0);
        // }
        delay(10);
    }
    /**
     * @brief Called when App is on create
     * 
     */
    void App_FallDetection_onCreate()
    {
        UI_LOG("[%s] onCreate\n", App_FallDetection_appName().c_str());
        static bool imu_inited = false;
        static bool falldown = false;
        testscreen_init();
        if (!imu_inited) {
            device->Lcd.printf("Init IMU...\n");
            UI_LOG("[%s] Imu not init\n", App_FallDetection_appName().c_str());
            imu_inited = true;
            device->Imu.init();
        }
        else{
            UI_LOG("[%s] Imu already inited\n", App_FallDetection_appName().c_str());
        }

        /*数据收集线程未启动且设置要求启动时，创建数据收集线程*/
        if(task_mpu == NULL && DataCollectionEnable){
            UI_LOG("[%s] try to create MPUtask\n", App_FallDetection_appName().c_str());

            xTaskCreate(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu);

            device->Lcd.printf("Data collection task has been created\n");
            UI_LOG("[%s] Data collection task has been created\n", App_FallDetection_appName().c_str());
        }
        /*检测线程未启动且要求启动时，创建检测线程*/
        if(task_detect == NULL && DetectionEnable){
            UI_LOG("[%s] try to create Detectiontask\n", App_FallDetection_appName().c_str());

            xTaskCreate(task_falldetect, "MPU6050_DET", 1024*16, NULL, 4, &task_detect);

            device->Lcd.printf("Detection task has been created\n");
            UI_LOG("[%s] Detection task has been created\n", App_FallDetection_appName().c_str());
        }
        while (1) {
            task_UI_loop();
            if (device->Button.B.pressed()){
                UI_LOG("[%s] Button has been pressed\n", App_FallDetection_appName().c_str());
                //vTaskDelete(task_UI);
                GRAVITY_LOST = false;
                GRAVITY_OVER = false;
                MOTION_LESS = false;
                FALL_DOWN = false;
                break;
            }
            //delay(10);
        }
        testscreen_deinit();

        lv_obj_t * label = lv_label_create(lv_scr_act());
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(label, "Press B again to quit");
    }


    /**
     * @brief Called repeatedly, end this function ASAP! or the App management will be affected
     * If the thing you want to do takes time, try create a taak or lvgl timer to handle them.
     * Try use millis() instead of delay() here
     * 
     */
    void App_FallDetection_onLoop()
    {
    }


    /**
     * @brief Called when App is about to be destroy
     * Please remember to release the resourse like lvgl timers in this function
     * 
     */
    void App_FallDetection_onDestroy()
    {
        /*在选择关闭检测功能的情况下再删除task，不关闭的情况下要保持该task*/
        if(DetectionEnable == false){
            vTaskDelete(task_mpu);
            task_mpu = NULL;
            UI_LOG("[%s] Data collection task has been destoryed\n", App_FallDetection_appName().c_str());
        }
        UI_LOG("[%s] onDestroy\n", App_FallDetection_appName().c_str());
    }


    /**
     * @brief Launcher will pass the BSP pointer through this function before onCreate
     * 
     */
    void App_FallDetection_getBsp(void* bsp)
    {
        device = (CHAPPIE*)bsp;
    }
    
    
}

#endif
