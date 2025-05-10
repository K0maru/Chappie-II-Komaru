/**
 * @file ChappieSD.hpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-03-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <iostream>
#include <string>
#include <vector>
#include <FS.h>
#include <SD.h>

#define CHAPPIE_SD_PIN_MISO     42
#define CHAPPIE_SD_PIN_MOSI     41
#define CHAPPIE_SD_PIN_CLK      40
#define CHAPPIE_SD_PIN_CS       48

class ChappieSD {
    private:
        bool _inited;

    public:
        inline ChappieSD() : _inited(false) {}
        inline ~ChappieSD() {}

        /**
         * @brief 
         * 
         * @return true Success
         * @return false Failed
         */
        inline bool init()
        {
            /* Init spi */
            SPIClass* sd_spi = new SPIClass(HSPI);
            sd_spi->begin(CHAPPIE_SD_PIN_CLK, CHAPPIE_SD_PIN_MISO, CHAPPIE_SD_PIN_MOSI, CHAPPIE_SD_PIN_CS);
            bool ret = SD.begin(CHAPPIE_SD_PIN_CS, *sd_spi, 400000);
            
            if (!ret) {
                ESP_LOGI("SD","init failed");
                //printf("[SD] init failed\n");
                return ret;
            }
            //printf("[SD] init success, ");

            /* Get SD card info */
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_MMC){
                ESP_LOGI("SD","init success,MMC");
                //printf("MMC");
            }  
            else if(cardType == CARD_SD){
                ESP_LOGI("SD","init success,SDSC");
                //printf("SDSC");
            }
            else if(cardType == CARD_SDHC){
                ESP_LOGI("SD","init success,SDHC");
                //printf("SDHC");
            }
            ESP_LOGI("SD","%dGB",(SD.cardSize() / 1073741824));
            //printf(" %dGB\n", (SD.cardSize() / 1073741824));

            _inited = true;
            return ret;
        }

        inline bool isInited() { return _inited; }

        inline bool readFile(const char* path, lv_coord_t* array,...) {
            if(!_inited){
                ESP_LOGI("SD","Not initialized");
                //printf("[SD] Not initialized\n");
                return false;
            }
            ESP_LOGI("SD","now size : %dGB",(SD.cardSize() / 1073741824));
            //printf("[SD] now size : %dGB\n",(SD.cardSize() / 1073741824));

            File file = SD.open(path, FILE_READ);

            if (!file) {
                ESP_LOGI("SD","Failed to open file for writing: %s",path);
                //printf("[SD] Failed to open file for writing: %s\n", path);
                return false;
            }
            ESP_LOGI("SD","Reading file: %s",path);
            //printf("[SD] Reading file: %s\n", path);
            int index = 0;
            while (file.available() && index < 7) {
                String line = file.readStringUntil('\n');
                line.trim();  // 去除空格和换行
                if (line.length() == 0) continue;
        
                int sepIndex = line.indexOf(' ');
                if (sepIndex == -1) continue;
        
                String cntStr = line.substring(sepIndex + 1);
                int Count = cntStr.toInt();
        
                array[index++] = Count;
            }
            file.close();
            ESP_LOGI("SD","Read complete");
            //printf("\n[SD] Read complete\n");

            return true;
        }

        inline bool writeFile(const char* path, const char* format,...) {
            if(!_inited){
                ESP_LOGI("SD","Not initialized");
                //printf("[SD] Not initialized\n");
                return false;
            }
            ESP_LOGI("SD","now size : %dGB\n",(SD.cardSize() / 1073741824));
            //printf("[SD] now size : %dGB\n",(SD.cardSize() / 1073741824));
            va_list args;
            va_start(args, format);
            int len = vsnprintf(nullptr, 0, format, args);  // 获取所需长度
            va_end(args);
            if (len <= 0)
                return false;
        
            char* buffer = (char*)pvPortMalloc(len + 1);  // +1 for null terminator
            if (!buffer)
                return false;
        
            va_start(args, format);
            vsnprintf(buffer, len + 1, format, args);
            va_end(args);
            ESP_LOGI("test","139");
            File file = SD.open(path, FILE_READ);

            if (!file) {
                ESP_LOGI("SD","Failed to open file for writing: %s",path);
                //printf("[SD] Failed to open file for writing: %s\n", path);
                vPortFree(buffer);
                return false;
            }
            ESP_LOGI("test","148");
            std::vector<String> lines;
            while (file.available()) {
                String line = file.readStringUntil('\n');
                line.trim();  // 去除结尾换行
                if (line.length() > 0)
                    lines.push_back(line);
            }
            ESP_LOGI("test","156");
            file.close();
            ESP_LOGI("test","158");
            // 如果已有7行，移除第一行
            if (lines.size() >= 7)
            lines.erase(lines.begin());
            ESP_LOGI("test","162");
            // 添加新数据
            lines.push_back(String(buffer));
            delay(20);
            // 重新写入文件（覆盖）
            ESP_LOGI("test","167");
            file = SD.open(path, FILE_WRITE);
            ESP_LOGI("test","169");
            if (!file) {
                ESP_LOGI("SD","Failed to open file for writing: %s",path);
                //printf("[SD] Failed to open file for writing: %s\n", path);
                vPortFree(buffer);
                return false;
            }
            
            for (auto& l : lines) {
                file.println(l);
            }
            ESP_LOGI("test","180");
            file.close();
            ESP_LOGI("SD","Write success: %s : %s",path, buffer);
            //printf("[SD] Write success: %s : %s\n", path, buffer);
            vPortFree(buffer);

            return true;
        }
};

