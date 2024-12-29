/**
 * @file App_Pedometer.cpp
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
#include "App_Pedometer.h"
#include "../../../ChappieBsp/Chappie.h"
#include "math.h"

static std::string app_name = "Pedometer";
static CHAPPIE* device;
static LGFX_Sprite* _screen;

extern TaskHandle_t task_mpu;
extern QueueHandle_t mpu_queue;
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
extern bool DataCollectionEnable;

namespace App {

    /**
     * @brief Return the App name laucnher, which will be show on launcher App list
     * 
     * @return std::string 
     */
    std::string App_Pedometer_appName()
    {
        return app_name;
    }


    /**
     * @brief Return the App Icon laucnher, NULL for default
     * 
     * @return void* 
     */
    void* App_Pedometer_appIcon()
    {
        return NULL;
    }
    /**
     * @brief 需要被保持的线程，用于持续获取MPU6050的数据。通过创建一个深度为5的消息队列来方便数据处理和保持。
     * 
     */
    static void task_mpu6050_data(void* param)
    {
        mpu_queue = xQueueCreate(5,sizeof(MPU6050_data));
        UI_LOG("[%s] mpu_queue created\n", App_Pedometer_appName().c_str());
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
     * @brief Called when App is on create
     * 
     */
    void App_Pedometer_onCreate()
    {
        UI_LOG("[%s] onCreate\n", App_Pedometer_appName().c_str());

        static bool imu_inited = false;
        static bool falldown = false;
        testscreen_init();
        if (!imu_inited) {
            device->Lcd.printf("Init IMU...\n");
            UI_LOG("[%s] Imu not init\n", App_Pedometer_appName().c_str());
            imu_inited = true;
            device->Imu.init();
        }
        else{
            UI_LOG("[%s] Imu already inited\n", App_Pedometer_appName().c_str());
        }
        /*数据收集线程未启动且设置要求启动时，创建数据收集线程*/
        if(task_mpu == NULL && DataCollectionEnable){
            UI_LOG("[%s] try to create MPUtask\n", App_Pedometer_appName().c_str());

            xTaskCreate(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu);

            device->Lcd.printf("Data collection task has been created\n");
            UI_LOG("[%s] Data collection task has been created\n", App_Pedometer_appName().c_str());
        }        
    }


    /**
     * @brief Called repeatedly, end this function ASAP! or the App management will be affected
     * If the thing you want to do takes time, try create a taak or lvgl timer to handle them.
     * Try use millis() instead of delay() here
     * 
     */
    void App_Pedometer_onLoop()
    {
    }


    /**
     * @brief Called when App is about to be destroy
     * Please remember to release the resourse like lvgl timers in this function
     * 
     */
    void App_Pedometer_onDestroy()
    {
        UI_LOG("[%s] onDestroy\n", App_Pedometer_appName().c_str());
    }


    /**
     * @brief Launcher will pass the BSP pointer through this function before onCreate
     * 
     */
    void App_Pedometer_getBsp(void* bsp)
    {
        device = (CHAPPIE*)bsp;
    }
    
}

#endif
