/**
 * @file App_Panel.cpp
 * @brief 整合三大功能，完善更新机制
 * @author K0maru (k0maru3@foxmail.com)
 * @version 3.0
 * @date 2025-04-16
 * 
 */
#if 1
#include "App_Panel.h"
#include "../../../ChappieBsp/Chappie.h"
#include "math.h"
#include "Panel_ui/ui.h"
#include <nvs.h>
#include <nvs_flash.h>

typedef enum{
    PFD_unconfiged = 0,
    PFD_configed = 0xAA,
}PFD_info_str_t;



extern lv_coord_t ui_FallcountChart_series_1_array[];
extern lv_coord_t ui_ChartInactivity_series_1_array[];
extern lv_coord_t ui_StepcountChart_series_1_array[];
volatile bool GRAVITY_LOST = false;
volatile bool GRAVITY_OVER = false;
volatile bool FALL_DOWN = false;
volatile bool MOTION_LESS = false;

lv_obj_t* lv_mpu;

QueueHandle_t mpu_queue;
TaskHandle_t task_mpu;
static QueueHandle_t Filter_queue;
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
struct MPU6050_data_Filter_t {
    float accelS;
};
static MPU6050_data_t MPU6050_data;
static MPU6050_data_t MPU6050_data_receiver;
static MPU6050_data_Filter_t Filter_sender;
static MPU6050_data_Filter_t Filter_receiver;

TaskHandle_t task_detect = NULL;
TaskHandle_t task_UI = NULL;
TaskHandle_t task_update = NULL;
TaskHandle_t task_pedometer_handler = NULL;
TaskHandle_t task_filter = NULL;
TaskHandle_t task_InactivityDetect_handler = NULL;
TaskHandle_t Fallwarning_speaker = NULL;
TaskHandle_t Inactivitywarning_speaker = NULL;
static std::string app_name = "Panel";
static CHAPPIE* device;
static LGFX_Sprite* _screen;
//bool DataCollectionEnable = true;
static bool DetectionEnable = true;
static bool PedometerEnable = true;
static bool InactivityDetectEnable = true;
lv_obj_t * sw_det = NULL;
lv_timer_t * det_timer = NULL;
lv_obj_t * lv_PM = NULL;
lv_obj_t * sw_PM = NULL;
lv_timer_t * PM_timer = NULL;

lv_obj_t * Fall_msgbox = NULL;
lv_obj_t * Fall_msgbox_btn = NULL;
lv_obj_t * Inactivity_msgbox = NULL;
lv_obj_t * Inactivity_msgbox_btn = NULL;

#define QUEUE_LENGTH 5
#define FILTER_WINDOW 5
#define GRAVITY_LOST_THRESHOLD 0.6
#define GRAVITY_OVER_THRESHOLD 2
#define PITCH_THRESHOLD 45
#define ROLL_THRESHOLD 45
#define ACCEL_BUFFER_SIZE 200  // 加速度数据的缓存大小（用于计算自相关）
#define T_MAX 100  // 最大时间窗口
#define T_MIN 25   // 最小时间窗口

#define scr_act_height() lv_obj_get_height(lv_scr_act())
#define scr_act_width() lv_obj_get_width(lv_scr_act())

#define ACCEL_THRESHOLD 0.05   // 加速度阈值，低于这个值视为静止
#define INACTIVITY_THRESHOLD 1800000  // 久坐阈值，单位：毫秒（30分钟）
#define CHECK_INTERVAL 1000  // 每秒检查一次静止状态

float accelS[ACCEL_BUFFER_SIZE];  // 存储加速度强度数据
float autocorr[ACCEL_BUFFER_SIZE];  // 自相关值

int current_index = 0;  // 当前加速度数据索引
int threshold = 50;  // 自相关值的阈值，用于判断步伐周期

static int looptimes = 0;
static uint8_t QueueIndex = 0;
static float DataBuffer[FILTER_WINDOW];

unsigned long lastActivityTime = 0;  // 最后一次活动的时间
unsigned long inactivityTime = 0;  // 累计的静止时间
bool isIdle = false;  // 标记是否静止

struct PFD_data_t {
    uint8_t date = 0;
    uint16_t steps = 0;
    uint8_t fall_count = 0;
    uint8_t inactivity_count = 0;
};
static PFD_data_t PFD_data;
#define PFD_LEN 5

volatile I2C_BM8563_DateTypeDef rtc_date;

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

    float calculate_mean(float data[], int length) {
        float sum = 0;
        for (int i = 0; i < length; i++) {
            sum += data[i];
        }
        return sum / length;
    }

    float calculate_std(float data[], int length, float mean) {
        float sum = 0;
        for (int i = 0; i < length; i++) {
            sum += (data[i] - mean) * (data[i] - mean);
        }
        return sqrt(sum / length);
    }

    static void task_Fallwarning_speaker(void* param) {
        // 在 GUI 线程创建弹窗
        lv_async_call([](void*) {
            static const char * btns[] = {"Cancel", ""};
            Fall_msgbox = lv_msgbox_create(lv_scr_act(), "Detect Falldown!", "Do you want to stop the beep?", btns, false);
            lv_obj_set_width(Fall_msgbox, 220);
            lv_obj_align(Fall_msgbox, LV_ALIGN_CENTER, 0, 0);
            // 不直接关闭，在任务中关闭
            lv_obj_add_event_cb(Fall_msgbox, [](lv_event_t* e) {
                    //msgbox_close_requested = true;  // 标记请求关闭
                    ESP_LOGI("test","167");
                    lv_msgbox_close(Fall_msgbox);
                    ESP_LOGI("test","169");
                    GRAVITY_LOST = false;
                    GRAVITY_OVER = false;
                    FALL_DOWN = false;
                    MOTION_LESS = false;
                    Fall_msgbox = NULL;
                    vTaskDelete(Fallwarning_speaker);
                    Fallwarning_speaker = NULL;
                    ESP_LOGI("test","176");
                
            }, LV_EVENT_VALUE_CHANGED, NULL);
        }, NULL);
        ESP_LOGI("test","181");
        // 蜂鸣器响铃循环
        while (FALL_DOWN) {
            device->Speaker.tone(9000, 200);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    static void task_Inactivity_speaker(void* param) {
        // 在 GUI 线程创建弹窗
        lv_async_call([](void*) {
            static const char * btns[] = {"Cancel", ""};
            Inactivity_msgbox = lv_msgbox_create(lv_scr_act(), "Detect Inacticity", "Do you want to stop the beep?", btns, false);
            lv_obj_set_width(Inactivity_msgbox, 220);
            lv_obj_align(Inactivity_msgbox, LV_ALIGN_CENTER, 0, 0);
            // 不直接关闭，在任务中关闭
            lv_obj_add_event_cb(Inactivity_msgbox, [](lv_event_t* e) {
                    //msgbox_close_requested = true;  // 标记请求关闭
                    lv_msgbox_close(Inactivity_msgbox);
                    Inactivity_msgbox = NULL;
                    isIdle = false;
                    vTaskDelete(Inactivitywarning_speaker);
                    Inactivitywarning_speaker = NULL;               
            }, LV_EVENT_VALUE_CHANGED, NULL);
        }, NULL);
        // 蜂鸣器响铃循环
        while (isIdle) {
            device->Speaker.tone(9000, 200);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
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
        if(Fallwarning_speaker == NULL){
            PFD_data.fall_count++;
            xTaskCreate(task_Fallwarning_speaker,"Fallwarning",2000,NULL,2,&Fallwarning_speaker);
        }
        // while(FALL_DOWN){
        //     device->Speaker.tone(9000, 300);
            
        //     if(device->Button.B.pressed()){
        //         GRAVITY_LOST = false;
        //         GRAVITY_OVER = false;
        //         MOTION_LESS = false;
        //         FALL_DOWN = false;
        //     }
        //     delay(10);
        // }
        
        
    }
    //从nvs中读取wifi配置到给定的sta_config结构体变量
    static esp_err_t readPFDConfig(PFD_data_t *PFD_config)
    {
        nvs_handle nvs;
        unsigned char PFDConfigVal;
        //  0.打开
        nvs_open("PFD_CONFIG", NVS_READWRITE, &nvs); 
        //  1.读取标志位，并判断
        nvs_get_u8(nvs, "PFDConfigFlag", &PFDConfigVal);
        if(PFDConfigVal != PFD_configed){
            // 1.1 没有PFD数据，关闭nvs，返回错误码
            ESP_LOGE("PFD", "no PFD config,read fail!");
            nvs_close(nvs); 
            return ESP_FAIL;
        }else{      
            //  1.2 进入下个步骤
            ESP_LOGI("PFD", "PFD configed,read ok!");    
        }
        //  2.读取上一次配网的ID，password
        uint32_t len = PFD_LEN;
        esp_err_t err = nvs_get_blob(nvs, "PFD_config", PFD_config, &len);

        ESP_LOGI("PFD", "readout  date:%d", PFD_config->date);
        ESP_LOGI("PFD", "readout  steps:%d", PFD_config->steps);
        ESP_LOGI("PFD", "readout  fall_count:%d", PFD_config->fall_count);
        ESP_LOGI("PFD", "readout  inactivity_count:%d", PFD_config->inactivity_count);
        // 3.关闭nvs退出
        nvs_close(nvs);
        return err;
    }

    /**
     * @brief  保存PFD配置参数结构体变量PFD_config到nvs
     * @param  PFD_config      wifi配置参数
     */
    static void savePFDConfig(PFD_data_t *PFD_config)
    {
        nvs_handle nvs;
        //  0.打开
        nvs_open("PFD_CONFIG", NVS_READWRITE, &nvs); 
        //  1.写入标记 0xaa,表示已经存储
        nvs_set_u8(nvs, "PFDConfigFlag", PFD_configed);
        //  2.写入PFD数据
        ESP_ERROR_CHECK(nvs_set_blob(nvs, "PFD_config", PFD_config, PFD_LEN));
        //  3.提交 并保存表的内容
        ESP_ERROR_CHECK(nvs_commit(nvs)); 
        //  4.关闭nvs退出
        nvs_close(nvs);                   
    }

    void clearPFDConfigFlag(void){
        nvs_handle nvs;
        //  0.打开
        nvs_open("PFD_CONFIG", NVS_READWRITE, &nvs); 
        //  1.写入标记 0x00,清除配网标记
        nvs_set_u8(nvs, "PFDConfigFlag", PFD_unconfiged);
        //  2.提交 并保存表的内容
        ESP_ERROR_CHECK(nvs_commit(nvs)); 
        //  3.关闭nvs退出
        nvs_close(nvs); 
    }

    /**
     * @brief   五点滤波，每10ms采集数据，每20ms滤波
     */
    static void task_5Point_Filter(void* param)
    {
        Filter_queue = xQueueCreate(QUEUE_LENGTH,sizeof(Filter_sender));
        ESP_LOGI(App_Panel_appName().c_str(),"Filter_queue created");
        //UI_LOG("[%s] Filter_queue created\n", App_Panel_appName().c_str());
        while(1){
            if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
                //模拟队列
                if (QueueIndex < FILTER_WINDOW) {
                    DataBuffer[QueueIndex] = MPU6050_data_receiver.accelS;
                    QueueIndex++;
                }
                else {
                    // 队列已满，覆盖最旧的数据
                    for (int i = 0; i < FILTER_WINDOW - 1; i++) {
                        DataBuffer[i] = DataBuffer[i + 1];
                    }
                    DataBuffer[FILTER_WINDOW - 1] = MPU6050_data_receiver.accelS;
                }
            }
            //20ms滤波一次
            // 计算五点滤波后的数据（即数据队列的平均值）
            float sum = 0.0f;
            for (int i = 0; i < FILTER_WINDOW; i++) {
                sum += DataBuffer[i];
            }
            Filter_sender.accelS = sum / FILTER_WINDOW;
            accelS[current_index] = Filter_sender.accelS;
            current_index = (current_index + 1) % ACCEL_BUFFER_SIZE;
            xQueueSend(Filter_queue,&Filter_sender,portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    float calculate_autocorrelation(int T, float mean) {
        float numerator = 0.0;
        float denominator1 = 0.0;
        float denominator2 = 0.0;

        for (int t = 0; t < T; t++) {
            numerator += (accelS[t] - mean) * (accelS[t + T] - mean);
            denominator1 += (accelS[t] - mean) * (accelS[t] - mean);
            denominator2 += (accelS[t + T] - mean) * (accelS[t + T] - mean);
        }

        // 计算自相关系数
        float deno = sqrt(denominator1 * denominator2);
        if(deno == 0.0) return 0.0;
        return numerator / deno;
    }

    /**
     * @brief                   sync chart
     * @param  op               op can receive 'p':Pedometer;'f':falldown;'i':inactivity
     * @param  date             date of data
     * @param  data             data
     */
    void chart_sync(const char &op){
        ESP_LOGI("SD","Start chart sync");
        lv_coord_t array[7] = {0};
        //lv_chart_series_t * series;
        lv_coord_t* target_array = nullptr;
        lv_obj_t* chart = nullptr;
        int index = 0;
        const char* path;
        switch (op)
        {
        case 'p':{
            target_array = ui_StepcountChart_series_1_array;
            chart = ui_StepcountChart;
            path = "/Pedometer.txt";
            break;
            }
        case 'f':{
            target_array = ui_FallcountChart_series_1_array;
            chart = ui_FallcountChart;
            path = "/Falldown.txt";
            break;
            }
        case 'i':{
            target_array = ui_ChartInactivity_series_1_array;
            chart = ui_ChartInactivity;
            path = "/Inactivity.txt";;
            break;
            }
        default:
            break;
        }
        device->Sd.readFile(path,array);
        for(index = 0;index < 7;++index){
            target_array[index] = array[index];
            //series->y_points[index] = array[index];
        }
        lv_chart_refresh(chart);
        ESP_LOGI("SD","chart sync done");
    }

    void chart_save(const char &op,const uint8_t &date,const uint16_t &data){
        ESP_LOGI("SD","Try to write");
        switch (op)
        {
        case 'p':
            device->Sd.writeFile("/Pedometer.txt","%d %d\n",date,data);
            break;
        case 'f':
            device->Sd.writeFile("/Falldown.txt","%d %d\n",date,data);
            break;
        case 'i':
            device->Sd.writeFile("/Inactivity.txt","%d %d\n",date,data);
            break;
        default:
            break;
        }
        ESP_LOGI("SD","Done");
    }
    static void task_pedometer(void* param)
    {
        while(1){
            // 计算自相关系数
            //float autocorr = calculate_autocorrelation(T, mean);
            float best_autocorr = 0;
            static int best_T = T_MIN;

            // 在 T_min 到 T_max 范围内查找最佳的 T
            for (int T = T_MIN; T <= T_MAX; T++) {
                float mean = calculate_mean(accelS, T);
                float std = calculate_std(accelS, T, mean);
                if (std < 0.01f) continue;
                float autocorr = calculate_autocorrelation(T, mean);
                if (autocorr > best_autocorr) {
                    best_autocorr = autocorr;
                    best_T = T;
                }
            }
            if (best_autocorr > 0.93) {
                    PFD_data.steps++;  // 步数加一
                    ESP_LOGI(App_Panel_appName().c_str(),"Step detected! Total steps: %d",PFD_data.steps);
            }
            vTaskDelay(pdMS_TO_TICKS(best_T + 10));
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
        if(PFD_data.date != rtc_date.date){
            if(PFD_data.date == 0){
                PFD_data.date = rtc_date.date;
                //chart_save('p',PFD_data.date,PFD_data.steps);
            }
            else{
                ESP_LOGI("MPU","Init steps for new day");
                chart_save('p',PFD_data.date,PFD_data.steps);
                //chart_sync('p');
                chart_save('f',PFD_data.date,PFD_data.fall_count);
                //chart_sync('f');
                chart_save('i',PFD_data.date,PFD_data.inactivity_count);
                //chart_sync('i');

                PFD_data.date = rtc_date.date;
                PFD_data.steps = 0;
                PFD_data.fall_count = 0;
                PFD_data.inactivity_count = 0;
            }
            ESP_LOGI(App_Panel_appName().c_str(),"Init steps for new day");
        }
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
            vTaskDelay(pdMS_TO_TICKS(10)); //100hz
        }
        
    }
    
    static void task_falldetect(void* param)
    {
        TickType_t startTime = 0; // 记录开始时间
        while(1){
            if(Fallwarning_speaker == NULL){
                if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
                    if(GRAVITY_LOST){
                        if(GRAVITY_OVER){
                            TickType_t MotionlessStart = 0;
                            if(MOTION_LESS){
                                if(abs(MPU6050_data_receiver.Pitch) > 45 || abs(MPU6050_data_receiver.Roll) > 45){
                                    FallDownInterrupt();
                                }
                                else if(FALL_DOWN == false){
                                    ESP_LOGI(App_Panel_appName().c_str(),"Error detection");
                                    ESP_LOGI("MPU","Yaw:%.2f Pitch:%.2f Roll:%.2f",MPU6050_data_receiver.Yaw,MPU6050_data_receiver.Pitch,MPU6050_data_receiver.Roll);
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
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    bool isUserInactive() {
        return abs(MPU6050_data_receiver.accelS-1) < ACCEL_THRESHOLD;  // 如果加速度强度小于阈值，认为用户静止
    }

    void triggerInactivityReminder() {
        // 显示久坐提醒
        PFD_data.inactivity_count++;
        xTaskCreate(task_Inactivity_speaker,"Inactivitywarning",2000,NULL,2,&Inactivitywarning_speaker);
        // 如果有振动功能，也可以触发振动提醒
        // device->vibrate();  // 假设设备有振动功能
    }

    void checkInactivity() {
        if (isUserInactive()) {
            inactivityTime += CHECK_INTERVAL;  // 累计静止时间
            if (inactivityTime >= INACTIVITY_THRESHOLD) {
                if (!isIdle) {
                    isIdle = true;
                    triggerInactivityReminder();  // 超过阈值，触发提醒
                }
            }
        } else {
            inactivityTime = 0;  // 如果用户活动，重置静止时间
            isIdle = false;
        }
    }

    static void task_InactivityDetect(void* param){
        while(1){
            checkInactivity();
            vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL));
        }
    }

    void mpu_value_update(lv_timer_t* timer){

        MPU6050_data_t* data = (MPU6050_data_t*)timer->user_data;
        //ESP_LOGI("DEBUG","Yaw: %.2f\nPitch: %.2f\nRoll: %.2f\nAS: %.2f", data->Yaw,data->Pitch,data->Roll,data->accelS);
        //lv_obj_set_size(lv_mpu, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        //lv_label_set_text_fmt(lv_mpu, "Yaw: %.2f\nPitch: %.2f\nRoll: %.2f\nAS: %.2f", data->Yaw,data->Pitch,data->Roll,data->accelS);//格式化显示输出
        //lv_obj_align(lv_mpu, LV_ALIGN_LEFT_MID, 0, 0);     //显示坐标设置
        //lv_obj_invalidate(lv_mpu); 
        lv_label_set_text_fmt(ui_LabelMPUYAW,"Yaw: %.2f",data->Yaw);
        //lv_obj_invalidate(ui_LabelMPUYAW);
        lv_label_set_text_fmt(ui_LabelMPURoll,"Roll: %.2f",data->Roll);
        lv_label_set_text_fmt(ui_LabelMPUPitch,"Pitch: %.2f",data->Pitch);
        lv_label_set_text_fmt(ui_LabelMPUAS,"AS: %.2f",data->accelS);

    }
    void PM_value_update(lv_timer_t* timer){

        PFD_data_t* data = (PFD_data_t*)timer->user_data;
        //ESP_LOGI("test","%d",data->fall_count);
        lv_label_set_text_fmt(ui_LabelStepcount,"%d",data->steps);//格式化显示输出
        lv_arc_set_value(ui_ArcPedometer,data->steps);
        lv_arc_set_value(ui_ArcSteps,data->steps);
        //chart_sync('p',data->date,data->steps);

        lv_label_set_text_fmt(ui_LabelInactivitycount,"%d",data->inactivity_count);//格式化显示输出
        lv_arc_set_value(ui_ArcInactivity,data->inactivity_count);
        lv_arc_set_value(ui_ArcInactivitycount,data->inactivity_count);

        lv_label_set_text_fmt(ui_LabelFallcount,"%d",data->fall_count);//格式化显示输出
        lv_arc_set_value(ui_ArcFall,data->fall_count);
        lv_arc_set_value(ui_ArcFalldetection,data->fall_count);
    }
    /**
     * @brief 计步/跌倒检测/久坐检测 功能启用/关闭
     */
    void FallDetectionEnable(){
        /*检测线程未启动且要求启动时，创建检测线程*/
        if(task_detect == NULL && DetectionEnable){
            UI_LOG("[%s] try to create Detectiontask\n", App_Panel_appName().c_str());
            xTaskCreate(task_falldetect, "MPU6050_DET", 1024*16, NULL, 4, &task_detect);
            App_Panel_TaskStateCheck(task_detect);
            //device->Lcd.printf("Detection task has been created\n");
            UI_LOG("[%s] Detection task has been created\n", App_Panel_appName().c_str());
        }
    }
    void FallDetectionDisable(){
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
    }
    void PedometertaskEnable(){
        if(task_pedometer_handler == NULL && PedometerEnable){
            ESP_LOGI(App_Panel_appName().c_str(),"Try to create Filtertask");
            //UI_LOG("[%s] Try to create Pedometertask\n", App_Panel_appName().c_str());
            //xTaskCreate(task_5Point_Filter,"Filter",5000,NULL,3,&task_filter);
            xTaskCreatePinnedToCore(task_5Point_Filter,"Filter",5000,NULL,3,&task_filter, 0);
            App_Panel_TaskStateCheck(task_filter);
            ESP_LOGI(App_Panel_appName().c_str(),"Try to create Pedometertask");
            xTaskCreate(task_pedometer, "Pedometer", 5000, NULL, 2, &task_pedometer_handler);
            App_Panel_TaskStateCheck(task_pedometer_handler);
            //device->Lcd.printf("Pedometer task has been created");
        }
    }
    void PedometertaskDisable(){
        if(PedometerEnable == false){
            if(PM_timer!=NULL){
                lv_timer_del(PM_timer);
                PM_timer =NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"PM_timer has been destoryed");
            }
            if(task_pedometer_handler!=NULL){
                vTaskDelete(task_pedometer_handler);
                task_pedometer_handler = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"Pedometer task has been destoryed");
                vTaskDelete(task_filter);
                task_filter = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"Filter task has been destoryed");
            }
            //UI_LOG("[%s] Pedometer task has been destoryed\n", App_Panel_appName().c_str());
        }
    }
    void InactivityDetectiontaskEnable(){
        if(task_InactivityDetect_handler == NULL && InactivityDetectEnable){
            ESP_LOGI(App_Panel_appName().c_str(),"Try to create Inactivity Detection task");
            xTaskCreate(task_InactivityDetect, "Inactivity Detection", 2048, NULL, 2, &task_InactivityDetect_handler);
            App_Panel_TaskStateCheck(task_InactivityDetect_handler);
            //device->Lcd.printf("Inactivity Detection task has been created");
        }
    }
    void InactivityDetectiontaskDisable(){
        if(InactivityDetectEnable == false){
            vTaskDelete(task_InactivityDetect_handler);
            task_InactivityDetect_handler = NULL;
            inactivityTime = 0;
            isIdle = false;
            ESP_LOGI(App_Panel_appName().c_str(),"InactivityDetect task has been destoryed");
        }
    }
    /**
     * @brief  三个功能按钮和开关的回调，以及对应按钮和开关的互锁
     * @param  e ui触发事件
     */
    static void panel_event_cb(lv_event_t *e){
        lv_obj_t * obj = lv_event_get_target(e);
        if(obj == ui_ButtonFall){
            ESP_LOGI(App_Panel_appName().c_str(),"ButtonFall handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
                DetectionEnable = true;
                lv_obj_add_state(ui_SwitchFallDetection,LV_STATE_CHECKED);
                FallDetectionEnable();
            }
            else{
                DetectionEnable = false;
                if(lv_obj_has_state(ui_SwitchFallDetection,LV_STATE_CHECKED)) lv_obj_clear_state(ui_SwitchFallDetection,LV_STATE_CHECKED);
                FallDetectionDisable();
            }
        }
        if(obj == ui_ButtonSteps){
            ESP_LOGI(App_Panel_appName().c_str(),"ButtonSteps handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
                PedometerEnable = true;
                lv_obj_add_state(ui_SwitchPedometer,LV_STATE_CHECKED);
                PedometertaskEnable();
            }
            else{
                PedometerEnable = false;
                if(lv_obj_has_state(ui_SwitchPedometer,LV_STATE_CHECKED)) lv_obj_clear_state(ui_SwitchPedometer,LV_STATE_CHECKED);
                PedometertaskDisable();
            }
        }
        if(obj == ui_ButtonInactivity){
            ESP_LOGI(App_Panel_appName().c_str(),"ButtonInactivity handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
                InactivityDetectEnable = true;
                lv_obj_add_state(ui_SwitchInactivity,LV_STATE_CHECKED);
                InactivityDetectiontaskEnable();
            }
            else{
                InactivityDetectEnable = false;
                if(lv_obj_has_state(ui_SwitchInactivity,LV_STATE_CHECKED)) lv_obj_clear_state(ui_SwitchInactivity,LV_STATE_CHECKED);
                InactivityDetectiontaskDisable();
            }
        }
        if(obj == ui_SwitchFallDetection){
            ESP_LOGI(App_Panel_appName().c_str(),"SwitchFall handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED)) {
                DetectionEnable = true;
                lv_obj_add_state(ui_ButtonFall,(LV_STATE_CHECKED | LV_STATE_FOCUSED));
                FallDetectionEnable();
            }
            else{
                DetectionEnable = false;
                if(lv_obj_has_state(ui_ButtonFall,(LV_STATE_CHECKED | LV_STATE_FOCUSED))) lv_obj_clear_state(ui_ButtonFall,(LV_STATE_CHECKED | LV_STATE_FOCUSED));
                FallDetectionDisable();
            }
        }
        if(obj == ui_SwitchInactivity){
            ESP_LOGI(App_Panel_appName().c_str(),"SwitchInactivity handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED)) {
                InactivityDetectEnable = true;
                lv_obj_add_state(ui_ButtonInactivity,(LV_STATE_CHECKED | LV_STATE_FOCUSED));
                InactivityDetectiontaskEnable();
            }
            else{
                InactivityDetectEnable = false;
                if(lv_obj_has_state(ui_ButtonInactivity,(LV_STATE_CHECKED | LV_STATE_FOCUSED))) lv_obj_clear_state(ui_ButtonInactivity,(LV_STATE_CHECKED | LV_STATE_FOCUSED));
                InactivityDetectiontaskDisable();
            }
        }
        if(obj == ui_SwitchPedometer){
            ESP_LOGI(App_Panel_appName().c_str(),"SwitchPedometer handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED)) {
                PedometerEnable = true;
                lv_obj_add_state(ui_ButtonSteps,(LV_STATE_CHECKED | LV_STATE_FOCUSED));
                PedometertaskEnable();
            }
            else{
                PedometerEnable = true;
                if(lv_obj_has_state(ui_ButtonSteps,(LV_STATE_CHECKED | LV_STATE_FOCUSED))) lv_obj_clear_state(ui_ButtonSteps,(LV_STATE_CHECKED | LV_STATE_FOCUSED));
                PedometertaskEnable();
            }
        }
        if(obj == Fall_msgbox_btn){
            ESP_LOGI(App_Panel_appName().c_str(),"FallDetect msgbox Button handler called back");
            vTaskDelete(Fallwarning_speaker);
            Fallwarning_speaker = NULL;
            device->Speaker.stop();
        }
    }
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
        if(obj == sw_PM){
            ESP_LOGI(App_Panel_appName().c_str(),"sw_PM handler called back");
            if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
                if(task_filter == NULL){
                    ESP_LOGI(App_Panel_appName().c_str(),"Try to create Filtertask");
                    xTaskCreatePinnedToCore(task_5Point_Filter,"Filter",5000,NULL,3,&task_filter, 0);
                    App_Panel_TaskStateCheck(task_filter);
                }
                if(task_pedometer_handler == NULL){
                    ESP_LOGI(App_Panel_appName().c_str(),"Try to create Pedometertask");
                    xTaskCreate(task_pedometer, "Pedometer", 5000, NULL, 2, &task_pedometer_handler);
                    App_Panel_TaskStateCheck(task_pedometer_handler);
                }
                if(PM_timer == NULL){
                    PM_timer = lv_timer_create(PM_value_update,1000,&PFD_data);
                }
                PedometerEnable = true;
            }
            else{
                PedometerEnable = false;
                lv_timer_del(PM_timer);
                PM_timer = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"PM_timer has been detected");
                vTaskDelete(task_pedometer_handler);
                task_pedometer_handler = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"task_pedometer has been detected");
                vTaskDelete(task_filter);
                task_filter = NULL;
                ESP_LOGI(App_Panel_appName().c_str(),"task_filter has been detected");
            }
        }
    }

/*----------------------------------------------------UI&DEVICE-------------------------------------------------------------------*/
    /**
     * @brief test init screen
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
     * @brief Called when App is on create
     * 
     */
    void App_Panel_onCreate()
    {
        UI_LOG("[%s] onCreate\n", App_Panel_appName().c_str());
        if(device->Sd.isInited()){
            ESP_LOGI("SD","SD already inited");
        }
        else{
            ESP_LOGI("SD","Try SD init");
            device->Sd.init();
        }
        //static bool falldown = false;
        testscreen_init();
        /*从NVS内读取保存的PFD数据*/
        if(readPFDConfig(&PFD_data) != ESP_OK){
            PFD_data.date = 0;
            PFD_data.steps = 0;
            PFD_data.fall_count = 0;
            PFD_data.inactivity_count = 0;
        }
        /*数据收集线程未启动且设置要求启动时，创建数据收集线程*/
        if(task_mpu == NULL){
            ESP_LOGI(App_Panel_appName().c_str(),"Try to create MPUtask");
            //UI_LOG("[%s] Try to create MPUtask\n", App_Panel_appName().c_str());
            xTaskCreatePinnedToCore(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu, 0);
            //xTaskCreate(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu);
            App_Panel_TaskStateCheck(task_mpu);
            device->Lcd.printf("Data collection task has been created\n");
            delay(500);
            //UI_LOG("[%s] Data collection task has been created\n", App_Panel_appName().c_str());
        }
        /*检测线程未启动且要求启动时，创建检测线程*/
        FallDetectionEnable();
        device->Lcd.printf("Detection task has been created\n");
        PedometertaskEnable();
        device->Lcd.printf("Pedometer task has been created");
        InactivityDetectiontaskEnable();
        device->Lcd.printf("Inactivity Detection task has been created");

        device->lvgl.disable();

        /* Reinit lvgl to free resources */
        lv_deinit();
        device->lvgl.init(&device->Lcd, &device->Tp);

        /* Init launcher UI */
        Panel_ui_init();
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x4D5B74), LV_PART_MAIN | LV_STATE_DEFAULT);
        /*初始化标签数据*/
        lv_label_set_text_fmt(ui_LabelStepcount,"%d",0);
        lv_label_set_text_fmt(ui_LabelFallcount,"%d",0);
        lv_label_set_text_fmt(ui_LabelInactivitycount,"%d",0);
        /*初始化圆弧*/
        lv_arc_set_value(ui_ArcPedometer,0);
        lv_arc_set_value(ui_ArcSteps,0);
        lv_arc_set_value(ui_ArcFall,0);
        lv_arc_set_value(ui_ArcFalldetection,0);
        lv_arc_set_value(ui_ArcInactivity,0);
        lv_arc_set_value(ui_ArcInactivitycount,0);
        //lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, true);
        /* Desktop init */
        lv_obj_set_scroll_snap_y(ui_AppDesktop, LV_SCROLL_SNAP_CENTER);
        //lv_obj_update_snap(ui_AppDesktop, LV_ANIM_ON);
        /* Add ui event call back */
        lv_obj_add_event_cb(ui_ButtonFall, panel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_ButtonSteps, panel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_ButtonInactivity, panel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_SwitchFallDetection, panel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_SwitchInactivity, panel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_SwitchPedometer, panel_event_cb, LV_EVENT_CLICKED, NULL);
        
        det_timer = lv_timer_create(mpu_value_update,20,&MPU6050_data_receiver);
        PM_timer = lv_timer_create(PM_value_update,30,&PFD_data); 
        
        if (DetectionEnable) {
            lv_obj_add_state(ui_ButtonFall, (LV_STATE_CHECKED | LV_STATE_FOCUSED));
            lv_obj_add_state(ui_SwitchFallDetection,(LV_STATE_CHECKED));
        }
        if (PedometerEnable) {
            lv_obj_add_state(ui_ButtonSteps, (LV_STATE_CHECKED | LV_STATE_FOCUSED));
            lv_obj_add_state(ui_SwitchPedometer,(LV_STATE_CHECKED));
        }
        if (InactivityDetectEnable) {
            lv_obj_add_state(ui_ButtonInactivity, (LV_STATE_CHECKED | LV_STATE_FOCUSED));
            lv_obj_add_state(ui_SwitchInactivity,(LV_STATE_CHECKED));
        }
        chart_sync('p');
        chart_sync('f');
        chart_sync('i');
        device->lvgl.enable();
        //App_Panel_tileview();

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
        FallDetectionDisable();
        PedometertaskDisable();
        InactivityDetectiontaskDisable();
        testscreen_deinit();
        clearPFDConfigFlag();
        savePFDConfig(&PFD_data);
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