/**
 * @file main.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-03-02
 * 
 * @copyright Copyright (c) 2023
 * 
 * @par 修改日志
 *  <Date>     | <Version> | <Author>       | <Description>                   *
 *----------------------------------------------------------------------------*
 *  2024/10/24 | 0.0.0.2   | K0maru         | 整理代码注释规范                 *      
 */
#include "Arduino.h"
#include "ChappieUI/ChappieUI.h"

CHAPPIEUI ChappieUI;
/**
 * @brief 系统初始化并启动App_Launcher 
 */
void setup()
{
    ChappieUI.begin();
}
/**
 * @brief 刷新App_Launcher以获得实时信息
 */
void loop()
{
    ChappieUI.update();
    delay(5);
}
