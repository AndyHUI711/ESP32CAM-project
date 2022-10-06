# ESP32CAM-project
It is a project that using ESP32CAM and object detection


**Create a new env**\
`conda create -n esp32 python==3.7`\
`conda activate esp32\`

**Git clone**\
`git clone https://github.com/AndyHUI711/ESP32CAM-project.git`

****Arduino set up**\**
1. download your Arduino IDE 2.0.0 from:
https://www.arduino.cc/en/software
2. install esp32 package\
![img.png](img.png)\
Go to _File_ -> _Preferences_ -> enter: \
![img_1.png](img_1.png)\
Add the Url:\
`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`\
3. install esp32 board lib \
![img_2.png](img_2.png)
_Tools -> Board Manager -> search esp32 -> download the last version_\


# **How to use esp32**

1. 设备(电脑等)连接 2.4G WIFI
2. WIFI: MIX4
3. PW: andyhui8
4. 进去网站： http://192.168.134.229/
5. 选择需要的资源： 如：
http://192.168.134.229/1600x1200.mjpeg
6. 有图片，视频，不同size 可选！
7. main.py 调用有bug待解决
8. 可以尝试YOLOV5 等直接获取连接URL 数据进行分析