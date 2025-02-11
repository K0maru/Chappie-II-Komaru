// /**
//  * @file App_Pedometer.cpp
//  * @brief 
//  * @author K0maru (k0maru3@foxmail.com)
//  * @version 1.0
//  * @date 2024-12-29
//  * 
//  * 
//  * @par 修改日志:
//  *  <Date>     | <Version> | <Author>       | <Description>                   
//  * ----------------------------------------------------------------------------
//  * 2024.12.29  |    0.1    |    K0maru      |构建App框架，添加功能组件
//  * 
//  */
// #if 1
// #include "App_Pedometer.h"
// #include "../../../ChappieBsp/Chappie.h"
// #include "math.h"

// #define QUEUE_LENGTH 5
// #define FILTER_WINDOW 5
// #define scr_act_height() lv_obj_get_height(lv_scr_act())
// #define scr_act_width() lv_obj_get_width(lv_scr_act())

// static uint8_t QueueIndex = 0;
// static float DataBuffer[FILTER_WINDOW]; 

// static std::string app_name = "Pedometer";
// static CHAPPIE* device;
// static LGFX_Sprite* _screen;

// extern TaskHandle_t task_mpu;
// TaskHandle_t task_pedometer_handler = NULL;
// TaskHandle_t task_filter = NULL;
// extern QueueHandle_t mpu_queue;
// static QueueHandle_t Filter_queue;
// struct MPU6050_data_t {
//     float Yaw;
//     float Pitch;
//     float Roll;
//     float accelX;
//     float accelY;
//     float accelZ;
//     float accelS;
//     float accelXZ;
// };
// struct MPU6050_data_Filter_t {
//     float accelS;
// };
// static MPU6050_data_t MPU6050_data;
// static MPU6050_data_t MPU6050_data_receiver;
// static MPU6050_data_Filter_t Filter_sender;
// static MPU6050_data_Filter_t Filter_receiver;
// extern bool DataCollectionEnable;
// static bool PedometerEnable = true;
// struct StepCount_t {
//     uint16_t steps = 0;
//     uint8_t date = -1;
// };
// static I2C_BM8563_DateTypeDef rtc_date;
// static StepCount_t StepCount;

// lv_obj_t * lv_PM = NULL;
// //lv_obj_t * sw_data = NULL;
// lv_obj_t * sw_PM = NULL;
// lv_timer_t * PM_timer = NULL;

// #define ACCEL_BUFFER_SIZE 100  // 加速度数据的缓存大小（用于计算自相关）

// #define T_MAX 1100  // 最大时间窗口
// #define T_MIN 750   // 最小时间窗口

// float accelS[ACCEL_BUFFER_SIZE];  // 存储加速度强度数据
// float autocorr[ACCEL_BUFFER_SIZE];  // 自相关值

// int current_index = 0;  // 当前加速度数据索引
// int threshold = 50;  // 自相关值的阈值，用于判断步伐周期

// namespace App {

//     /**
//      * @brief Return the App name laucnher, which will be show on launcher App list
//      * 
//      * @return std::string 
//      */
//     std::string App_Pedometer_appName()
//     {
//         return app_name;
//     }
//     /**
//      * @brief Return the App Icon laucnher, NULL for default
//      * 
//      * @return void* 
//      */
//     void* App_Pedometer_appIcon()
//     {
//         return NULL;
//     }

//     float calculate_mean(float data[], int length) {
//         float sum = 0;
//         for (int i = 0; i < length; i++) {
//             sum += data[i];
//         }
//         return sum / length;
//     }

//     float calculate_std(float data[], int length, float mean) {
//         float sum = 0;
//         for (int i = 0; i < length; i++) {
//             sum += (data[i] - mean) * (data[i] - mean);
//         }
//         return sqrt(sum / length);
//     }
//     /**
//      * @brief 需要被保持的线程，用于持续获取MPU6050的数据。通过创建一个深度为5的消息队列来方便数据处理和保持。
//      * 
//      */
//     static void task_mpu6050_data(void* param)
//     {
//         mpu_queue = xQueueCreate(QUEUE_LENGTH,sizeof(MPU6050_data));
//         UI_LOG("[%s] mpu_queue created\n", App_Pedometer_appName().c_str());
//         while(1){
//             device->Imu.getYawPitchRoll(MPU6050_data.Yaw,MPU6050_data.Pitch,MPU6050_data.Roll);
//             device->Imu.getAcceler(MPU6050_data.accelX,MPU6050_data.accelY,MPU6050_data.accelZ);
//             MPU6050_data.accelS = sqrt(MPU6050_data.accelX*MPU6050_data.accelX+
//                                        MPU6050_data.accelY*MPU6050_data.accelY+
//                                        MPU6050_data.accelZ*MPU6050_data.accelZ);
//             MPU6050_data.accelXZ = sqrt(MPU6050_data.accelX*MPU6050_data.accelX+MPU6050_data.accelZ*MPU6050_data.accelZ);
//             xQueueSend(mpu_queue, &MPU6050_data, portMAX_DELAY);
//             vTaskDelay(10); //100hz
//         }
        
//     }
//     /**
//      * @brief   五点滤波，每10ms采集数据，每20ms滤波
//      */
//     static void task_5Point_Filter(void* param)
//     {
//         Filter_queue = xQueueCreate(QUEUE_LENGTH,sizeof(Filter_sender));
//         ESP_LOGI(App_Pedometer_appName().c_str(),"Filter_queue created");
//         //UI_LOG("[%s] Filter_queue created\n", App_Pedometer_appName().c_str());
//         while(1){
//             if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
//                 //模拟队列
//                 if (QueueIndex < FILTER_WINDOW) {
//                     DataBuffer[QueueIndex] = MPU6050_data_receiver.accelXZ;
//                     QueueIndex++;
//                 }
//                 else {
//                     // 队列已满，覆盖最旧的数据
//                     for (int i = 0; i < FILTER_WINDOW - 1; i++) {
//                         DataBuffer[i] = DataBuffer[i + 1];
//                     }
//                     DataBuffer[FILTER_WINDOW - 1] = MPU6050_data_receiver.accelS;
//                 }
//             }
//             //20ms滤波一次
//             // 计算五点滤波后的数据（即数据队列的平均值）
//             float sum = 0.0f;
//             for (int i = 0; i < FILTER_WINDOW; i++) {
//                 sum += DataBuffer[i];
//             }
//             Filter_sender.accelS = sum / FILTER_WINDOW;
//             accelS[current_index] = Filter_sender.accelS;
//             current_index = (current_index + 1) % ACCEL_BUFFER_SIZE;
//             //xQueueSend(Filter_queue,&Filter_sender,portMAX_DELAY);
//             vTaskDelay(20);
//         }
//     }
//     float calculate_autocorrelation(int T, float mean) {
//         float numerator = 0.0;
//         float denominator1 = 0.0;
//         float denominator2 = 0.0;

//         for (int t = 0; t < T; t++) {
//             numerator += (accelS[t] - mean) * (accelS[t + T] - mean);
//             denominator1 += (accelS[t] - mean) * (accelS[t] - mean);
//             denominator2 += (accelS[t + T] - mean) * (accelS[t + T] - mean);
//         }

//         // 计算自相关系数
//         return numerator / (sqrt(denominator1 * denominator2));
//     }
//     static void task_pedometer(void* param)
//     {
//         if(StepCount.date != rtc_date.date){
//             StepCount.date = rtc_date.date;
//             StepCount.steps = 0;
//             ESP_LOGI(App_Pedometer_appName().c_str(),"Init steps for new day");
//             //UI_LOG("[%s] Init StepCount\n", App_Pedometer_appName().c_str());
//         }
//         while(1){
//             // 计算自相关系数
//             //float autocorr = calculate_autocorrelation(T, mean);
//             float best_autocorr = 0;
//             int best_T = T_MIN;

//             // 在 T_min 到 T_max 范围内查找最佳的 T
//             for (int T = T_MIN; T <= T_MAX; T++) {
//                 float mean = calculate_mean(accelS, T);
//                 float std = calculate_std(accelS, T, mean);
//                 float autocorr = calculate_autocorrelation(T, mean);
//                 if (autocorr > best_autocorr) {
//                     best_autocorr = autocorr;
//                     best_T = T;
//                 }
//             }
//             if (best_autocorr > 0.93) {
//                     StepCount.steps++;  // 步数加一
//                     ESP_LOGI(App_Pedometer_appName().c_str(),"Step detected! Total steps: %d",StepCount.steps);
//             }
//             vTaskDelay(best_T+10);
//         }
        
//     }
//         /**
//      * @brief  通过UI_LOG输出任务状态
//      * @param  task_handler     任务句柄
//      */
//     void App_Pedometer_TaskStateCheck(TaskHandle_t task_handler){
//         static eTaskState TaskState;
//         TaskState = eTaskStateGet(task_handler);

//         switch (TaskState) {
//             case eRunning:
//                 //ESP_LOGI(App_Pedometer_appName().c_str(),"Task is Running");
//                 UI_LOG("[%s] Task is Running\n", App_Pedometer_appName().c_str());
//                 break;
//             case eReady:
//                 //ESP_LOGI(App_Pedometer_appName().c_str(),"Task is Ready");
//                 UI_LOG("[%s] Task is Ready\n", App_Pedometer_appName().c_str());
//                 break;
//             case eBlocked:
//                 //ESP_LOGI(App_Pedometer_appName().c_str(),"Task is Blocked");
//                 UI_LOG("[%s] Task is Blocked\n", App_Pedometer_appName().c_str());
//                 break;
//             case eSuspended:
//                 //ESP_LOGI(App_Pedometer_appName().c_str(),"Task is Suspended");
//                 UI_LOG("[%s] Task is Suspended\n", App_Pedometer_appName().c_str());
//                 break;
//             case eDeleted:
//                 //ESP_LOGI(App_Pedometer_appName().c_str(),"Task is Deleted");
//                 UI_LOG("[%s] Task is Deleted\n", App_Pedometer_appName().c_str());
//                 break;
//             default:
//                 //ESP_LOGI(App_Pedometer_appName().c_str(),"Invalid State");
//                 UI_LOG("[%s] Invalid State\n", App_Pedometer_appName().c_str());
//                 break;
//         }
//     }
//     void PM_value_update(lv_timer_t* timer){

//         StepCount_t* data = (StepCount_t*)timer->user_data;
//         //ESP_LOGD("%s","Now,Yaw: %.1f",App_FallDetection_appName(),data->Yaw);
//         lv_obj_set_size(lv_PM, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
//         lv_label_set_text_fmt(lv_PM, "Steps: %d", data->steps);//格式化显示输出
//         lv_obj_align(lv_PM, LV_ALIGN_LEFT_MID, 0, 0);     //显示坐标设置
//         lv_obj_invalidate(lv_PM); 
//     }
//     static void lv_sw_handler(lv_event_t *e){
//         lv_obj_t * obj = lv_event_get_target(e);
//         if(obj == sw_PM){
//             ESP_LOGI(App_Pedometer_appName().c_str(),"sw_PM handler called back");
//             if (lv_obj_get_state(obj) == (LV_STATE_CHECKED | LV_STATE_FOCUSED)) {
//                 if(task_filter == NULL){
//                     ESP_LOGI(App_Pedometer_appName().c_str(),"Try to create Filtertask");
//                     xTaskCreatePinnedToCore(task_5Point_Filter,"Filter",5000,NULL,3,&task_filter, 0);
//                     App_Pedometer_TaskStateCheck(task_filter);
//                 }
//                 if(task_pedometer_handler == NULL){
//                     ESP_LOGI(App_Pedometer_appName().c_str(),"Try to create Pedometertask");
//                     xTaskCreate(task_pedometer, "Pedometer", 5000, NULL, 2, &task_pedometer_handler);
//                     App_Pedometer_TaskStateCheck(task_pedometer_handler);
//                 }
//                 if(PM_timer == NULL){
//                     PM_timer = lv_timer_create(PM_value_update,1000,&StepCount);
//                 }
//                 PedometerEnable = true;
//             }
//             else{
//                 PedometerEnable = false;
//                 lv_timer_del(PM_timer);
//                 PM_timer = NULL;
//                 ESP_LOGI(App_Pedometer_appName().c_str(),"PM_timer has been detected");
//                 vTaskDelete(task_pedometer_handler);
//                 task_pedometer_handler = NULL;
//                 ESP_LOGI(App_Pedometer_appName().c_str(),"task_pedometer has been detected");
//                 vTaskDelete(task_filter);
//                 task_filter = NULL;
//                 ESP_LOGI(App_Pedometer_appName().c_str(),"task_filter has been detected");
//             }
//         }
//     }
//     /**
//      * @brief init screen
//      */
//     static void testscreen_init()
//     {
//         _screen = new LGFX_Sprite(&device->Lcd);
//         _screen->setPsram(true);
//         _screen->createSprite(device->Lcd.width(), device->Lcd.height());
//         _screen->setTextScroll(true);
//         _screen->setTextSize(1.2);

//         device->Lcd.fillScreen(TFT_BLACK);
//         device->Lcd.setCursor(0, 0);
//         device->Lcd.setTextSize(1.5);
//         device->Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
//     }

//     static void testscreen_deinit()
//     {
//         _screen->deleteSprite();
//         delete _screen;
//     }    




//     static void task_UI_loop()
//     {
//         if(xQueueReceive(mpu_queue,&MPU6050_data_receiver,portMAX_DELAY) == true){
//             _screen->fillScreen(TFT_BLACK);
//             _screen->setTextSize(2);
//             _screen->setTextColor(TFT_ORANGE);
//             _screen->setCursor(0, 30);
//             _screen->printf(" > Yaw: %.1f\n > Pitch: %.1f\n > Row: %.1f\n", MPU6050_data_receiver.Yaw, 
//                                                                         MPU6050_data_receiver.Pitch, 
//                                                                         MPU6050_data_receiver.Roll);
//             _screen->printf(" > AX:  %.1f\n > AY:    %.1f\n > AZ:  %.1f\n", MPU6050_data_receiver.accelX,
//                                                                         MPU6050_data_receiver.accelY,
//                                                                         MPU6050_data_receiver.accelZ);
//             _screen->printf(" > AS:  %.1f\n",MPU6050_data_receiver.accelS);
//             _screen->printf(" > Steps:  %d\n",StepCount.steps);
//             _screen->printf(" > try to count,may be\n");
//             _screen->pushSprite(0, 0);
//         }
//         else{
//             UI_LOG("[%s] no data\n", App_Pedometer_appName().c_str());
//             _screen->fillScreen(TFT_BLACK);
//             _screen->setTextSize(2);
//             _screen->setTextColor(TFT_ORANGE);
//             _screen->setCursor(0, 30);
//             _screen->printf(" > Error\n");
//             _screen->pushSprite(0, 0);
//         }
//         delay(10);
//     }
//     static void App_Pedometer_tileview(void)
//     {
//         lv_obj_t * tv = lv_tileview_create(lv_scr_act());

//         /*Tile1: 显示跌倒次数和开关*/
//         lv_obj_t * tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_BOTTOM);
//         //sw_data = lv_switch_create(tile1);
        
//         //sw_PM = lv_switch_create(tile1);
        
//         lv_obj_t * label = NULL;

//         /* 基础对象（矩形背景） */
//         lv_obj_t *obj_PM = lv_obj_create(tile1);                               /* 创建基础对象 */
//         lv_obj_set_size(obj_PM,scr_act_height() / 2, scr_act_height() / 2 );          /* 设置大小 */
//         lv_obj_align(obj_PM, LV_ALIGN_CENTER, -scr_act_width() / 4, 0 );              /* 设置位置 */


//         /* 开关标签 */
//         lv_obj_t *label_det = lv_label_create(obj_PM);                               /* 创建标签 */
//         lv_label_set_text(label_det, "Detection");                                          /* 设置文本内容 */
//         lv_obj_align(label_det, LV_ALIGN_CENTER, 0, -scr_act_height() / 16 );          /* 设置位置 */

//         /* 开关 */
//         sw_PM = lv_switch_create(obj_PM);                                       /* 创建开关 */
//         lv_obj_set_size(sw_PM,scr_act_height() / 6, scr_act_height() / 12 );      /* 设置大小 */
//         lv_obj_align(sw_PM, LV_ALIGN_CENTER, 0, scr_act_height() / 16 );          /* 设置位置 */
//         if(PedometerEnable){lv_obj_add_state(sw_PM,LV_STATE_CHECKED);}
//         lv_obj_add_event_cb(sw_PM, lv_sw_handler, LV_EVENT_VALUE_CHANGED, NULL);/* 添加事件 */

//         /* 基础对象（矩形背景） */
//         lv_obj_t *obj_data = lv_obj_create(tile1);
//         lv_obj_set_size(obj_data,scr_act_height() / 2, scr_act_height() / 2 );
//         lv_obj_align(obj_data, LV_ALIGN_CENTER, scr_act_width() / 4, 0 );

//         //lv_obj_center(sw_PM);
//         /*Tile2：实时显示调试信息*/
//         lv_obj_t * tile2 = lv_tileview_add_tile(tv, 0, 1, LV_DIR_TOP);
//         lv_PM = lv_label_create(tile2);
        
//         PM_timer = lv_timer_create(PM_value_update,20,&StepCount);
//     }
//     /**
//      * @brief Called when App is on create
//      * 
//      */
//     void App_Pedometer_onCreate()
//     {
//         UI_LOG("[%s] onCreate\n", App_Pedometer_appName().c_str());
        
//         static bool imu_inited = false;
//         static bool falldown = false;
//         device->Rtc.getDate(&rtc_date);
//         testscreen_init();
//         /*数据收集线程未启动且设置要求启动时，创建数据收集线程*/
//         if(task_mpu == NULL){
//             ESP_LOGI(App_Pedometer_appName().c_str(),"Try to create MPUtask");
//             //UI_LOG("[%s] Try to create MPUtask\n", App_Pedometer_appName().c_str());
//             xTaskCreatePinnedToCore(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu, 0);
//             //xTaskCreate(task_mpu6050_data, "MPU6050_DATA", 5000, NULL, 3, &task_mpu);
//             App_Pedometer_TaskStateCheck(task_mpu);
//             device->Lcd.printf("Data collection task has been created\n");
//             //UI_LOG("[%s] Data collection task has been created\n", App_Pedometer_appName().c_str());
//         }
//         if(task_pedometer_handler == NULL && PedometerEnable){
//             ESP_LOGI(App_Pedometer_appName().c_str(),"Try to create Filtertask");
//             //UI_LOG("[%s] Try to create Pedometertask\n", App_Pedometer_appName().c_str());
//             //xTaskCreate(task_5Point_Filter,"Filter",5000,NULL,3,&task_filter);
//             xTaskCreatePinnedToCore(task_5Point_Filter,"Filter",5000,NULL,3,&task_filter, 0);
//             App_Pedometer_TaskStateCheck(task_filter);
//             ESP_LOGI(App_Pedometer_appName().c_str(),"Try to create Pedometertask");
//             xTaskCreate(task_pedometer, "Pedometer", 5000, NULL, 2, &task_pedometer_handler);
//             App_Pedometer_TaskStateCheck(task_pedometer_handler);
//             device->Lcd.printf("Pedometer task has been created");
//         }
        
//         App_Pedometer_tileview();
//         // while (1) {
//         //     task_UI_loop();
//         //     //ESP_LOGI("UI","is looping");
//         //     if (device->Button.B.pressed()){
//         //         UI_LOG("[%s] Button has been pressed\n", App_Pedometer_appName().c_str());
//         //         break;
//         //     }
//         //     //delay(10);
//         // }
//         // testscreen_deinit();

//         // lv_obj_t * label = lv_label_create(lv_scr_act());
//         // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
//         // lv_label_set_text(label, "Press B again to quit");      
//     }


//     /**
//      * @brief Called repeatedly, end this function ASAP! or the App management will be affected
//      * If the thing you want to do takes time, try create a taak or lvgl timer to handle them.
//      * Try use millis() instead of delay() here
//      * 
//      */
//     void App_Pedometer_onLoop()
//     {
//     }


//     /**
//      * @brief Called when App is about to be destroy
//      * Please remember to release the resourse like lvgl timers in this function
//      * 
//      */
//     void App_Pedometer_onDestroy()
//     {
        
        
//         /*在选择关闭检测功能的情况下再删除task，不关闭的情况下要保持该task*/
//         if(PedometerEnable == false){
//             if(PM_timer!=NULL){
//                 lv_timer_del(PM_timer);
//                 PM_timer =NULL;
//                 ESP_LOGI(App_Pedometer_appName().c_str(),"PM_timer has been destoryed");
//             }
//             if(task_pedometer_handler!=NULL){
//                 vTaskDelete(task_pedometer_handler);
//                 task_pedometer_handler = NULL;
//                 ESP_LOGI(App_Pedometer_appName().c_str(),"Pedometer task has been destoryed");
//                 vTaskDelete(task_filter);
//                 task_filter = NULL;
//                 ESP_LOGI(App_Pedometer_appName().c_str(),"Filter task has been destoryed");
//             }
//             //UI_LOG("[%s] Pedometer task has been destoryed\n", App_Pedometer_appName().c_str());
//         }
//         // vTaskDelete(task_pedometer_handler);
//         // task_pedometer_handler = NULL;
//         // ESP_LOGI(App_Pedometer_appName().c_str(),"Pedometer task has been destoryed");
//         // vTaskDelete(task_filter);
//         // task_filter = NULL;
//         // ESP_LOGI(App_Pedometer_appName().c_str(),"Filter task has been destoryed");
//         // testscreen_deinit();
//         // ESP_LOGI(App_Pedometer_appName().c_str(),"onDestroy");
//         testscreen_deinit();
//         UI_LOG("[%s] onDestroy\n", App_Pedometer_appName().c_str());
//     }


//     /**
//      * @brief Launcher will pass the BSP pointer through this function before onCreate
//      * 
//      */
//     void App_Pedometer_getBsp(void* bsp)
//     {
//         device = (CHAPPIE*)bsp;
//     }
    
// }

// #endif
