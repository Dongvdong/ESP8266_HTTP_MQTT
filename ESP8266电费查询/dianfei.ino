/**

  说明：
    程序会先尝试连接路由表中保存的wifi(自动向路由表中增加2条wifi)，
    不成功则进入smartconfig模式，等待微信airkiss的连接信息;
    连接到i-HDU或i_HDU则自动进行认证

    连接wifi后，会从相应的网址读取费用查询网址；
    获取失败则使用默认网址（这个网址应该要改成备用的。。）
    如果网址返回的数据第一行为字符updata，则进入升级模式
      先向反馈地址发送反馈，以更新index.php

    查询到网址后，每一段时间解析一次信息
    程序能解析出截止时间，楼，寝室号，剩余电费
    显示寝室号和剩余电费

  功耗测试： 5V时0.07A左右

  bug：  开机花屏，初步推测是在esp8266上电时GPIO0或1会产生某种信号，被同样在启动过程的屏幕接收到后引起数据错乱。目前先给屏幕供电、再给esp8266供电可以缓解

    Created on: 01.05.2018

*/
#define DELAYTIME 10000
//出错重试时间
#define DELAYTIMEERR 2000

#ifdef ESP8266
extern "C" {
#include "user_interface.h"   //含有system_get_chip_id（）的库
}
#endif
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <string.h>

#include <Adafruit_ssd1306syp.h>
#define SDA_PIN 2
#define SCL_PIN 0
Adafruit_ssd1306syp display(SDA_PIN, SCL_PIN);

//储存电费查询网址
String dat1;
String dat2;

//储存每次查询间隔时间
int delaytime = DELAYTIME;

//储存SN号
String SN;

ESP8266WiFiMulti WiFiMulti;

struct priceinf {
  float price;
  char timestr[20 * 7];
  int year;
  int month;
  int day;
  int hh;
  int mm;
  int ss;
  char lou[20];
  char qinshi[10];

};

void wifi_Init();
priceinf getPrice(String s);
void oled_Display (priceinf);
int hdulogin();
int site_Get();
//void WifiScan();

void setup() {
  int i;
  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  //获取SN号
  Serial.print("SN:");
  SN = (String )system_get_chip_id();
  Serial.println(SN);

  //等待初始化完成
  WiFi.begin();
  for (uint8_t t = 3; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  display.initialize();//屏幕初始化
  wifi_Init();    //连接wifi


  //获取计费网址和升级信息
  if (site_Get() != 0) {
    display.clear();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Unable to connect to the server!. Please try to reboot");
    display.println("Connect a fake WiFi?");
    display.update();

    WiFi.disconnect();
    delay(3000);
  }
}

void loop() {
  //等待WiFi连接
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    http.begin(dat1, 80, dat2); //HTTP

    int httpCode = http.GET();
    if (httpCode) {
      // 打印返回代码
      Serial.printf("code=%d\n", httpCode);

      // 数据正常返回
      if (httpCode == 200) {
        String payload = http.getString();
        oled_Display(getPrice(payload));
      }
      else {//服务器返回的代码不是200
        display.clear();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("Network error:");
        display.print("code:");
        display.println(httpCode);
        display.update();

        delaytime = DELAYTIMEERR;
      }
    }
    //服务器无返回信息
    else {
      Serial.print("GET... failed\n");
      display.clear();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.println("Network error:");
      display.print("Can't connect to the balance server");
      display.update();

      delaytime = DELAYTIMEERR;
    }
  }
  delay(delaytime);
  site_Get();
}

void wifi_Init() {
  WiFi.begin();
  int i = 0;
  //增加的默认WiFi，在这里增加自己的WIFI信息
  WiFiMulti.addAP("i-HDU", "");
  WiFiMulti.addAP("AndroidAP", "");
  display.clear();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 48);
  display.print("SN:");
  display.println(SN);
  display.setCursor(0, 56);
  display.println("Powered by zyzand^-^");
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("WiFi Connecting...");
  display.update();
  delay(200);
  i = 0;
  while ((WiFiMulti.run() != WL_CONNECTED) && i < 75) { //等待15秒
    delay(200);
    display.print(".");
    display.update();
    i++;
  }
  if (i == 75) { //15秒没连接上,进入SmartConfig模式
    display.clear();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 56);
    display.println("Powered by zyzand^-^");
    display.setCursor(0, 0);
    display.println("Connect fail.");
    display.println("Waiting for SmartConfig...");
    display.println("(please use you phone to Config)");
    display.update();
    Serial.println("\r\nWait for Smartconfig");

    WiFi.mode(WIFI_STA);
    WiFi.beginSmartConfig();

    while (!WiFi.smartConfigDone()) {
      delay(500);
    }

    Serial.println("SmartConfig Success");
    Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
    Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.clear();
  display.setCursor(0, 56);
  display.println("Powered by zyz^-^");
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.print("ssid:");
  display.println(WiFi.SSID().c_str());
  display.print("IP:");
  display.println(WiFi.localIP());
  display.update();
  delay(1500);

  //判断是否进行i-HDU认证
  if (strcmp(WiFi.SSID().c_str(), "i-HDU") == 0 || strcmp(WiFi.SSID().c_str(), "i_HDU") == 0) {
    display.clear();
    display.setCursor(0, 0);
    display.println("Start i-HDU login");
    Serial.println("Start i-HDU login");
    display.update();
    if (hdulogin() == 0) {
      display.println("Login Success!");
      Serial.println("iHDU Login Success!");
    }
    else {
      display.println("Login Fail!");
      Serial.println("iHDU Login Fail!");
    }
    display.update();
    delay(500);
  }
  return;
}

void oled_Display(priceinf dat) {
  delaytime = DELAYTIME;
  display.clear();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Date:");
  if (strlen(dat.timestr) == 0) {
    display.println("No Data");
    delaytime = DELAYTIMEERR;
  }
  else {
    display.println(dat.timestr);
  }
  
  display.println("");
  display.print("room ");
  display.print(dat.qinshi);
  display.println(" Balance:");
  display.setTextSize(2);
  display.print("  ");
  if (dat.price <= 5) {
    display.setTextColor(BLACK, WHITE);
  }
  display.print(dat.price);
  display.update();
}

priceinf getPrice(String s) {
  priceinf dat;
  int datStart = 0, datEnd = 0;
  String datstr;
  char buf[50];

  char datsign[] = "<span class=\"price\"";
  datStart = s.indexOf(datsign) + strlen(datsign) + 23;
  if (datStart == strlen(datsign) + 23 - 1) { //没有找到price
    memset(&dat, 0, sizeof(dat));
    return dat;
  }
  datEnd = s.indexOf("</span>", datStart) - 2;   //减2是为了减去字符“元”
  datstr = s.substring(datStart, datEnd);
  dat.price = datstr.toFloat();


  char timesign[] = "<font style=\"color:#2d9fd3\"><b>";
  datStart = s.indexOf(timesign) + strlen(timesign);
  datEnd = s.indexOf("</b></font>", datStart);   //结尾
  datstr = s.substring(datStart, datEnd);
  datstr.toCharArray(dat.timestr, 20);
  dat.timestr[19] = 0;

  dat.year = datstr.substring(0, 4).toInt();
  dat.month = datstr.substring(5, 7).toInt();
  dat.day = datstr.substring(8, 10).toInt();
  dat.hh = datstr.substring(11, 13).toInt();
  dat.mm = datstr.substring(14, 16).toInt();
  dat.ss = datstr.substring(17, 19).toInt();

  char lousign[] = "楼幢：";
  datStart = s.indexOf(lousign) + strlen(lousign);
  datEnd = s.indexOf("</p>", datStart);   //结尾
  datstr = s.substring(datStart, datEnd);
  datstr.toCharArray(dat.lou, 20);
  dat.lou[19] = 0;

  char qinshisign[] = "寝室号：";
  datStart = s.indexOf(qinshisign) + strlen(qinshisign);
  datEnd = s.indexOf("</p>", datStart);   //结尾
  datstr = s.substring(datStart, datEnd);
  datstr.toCharArray(dat.qinshi, 10);
  dat.qinshi[9] = 0;


  //Serial.println(s);
  Serial.print("time=");
  Serial.println(dat.timestr);
  Serial.print("price=");
  Serial.println(dat.price);
  Serial.print("lou=");
  Serial.println(dat.lou);
  Serial.print("qinshi=");
  Serial.println(dat.qinshi);
  Serial.println();
  return dat;
}

/*
	通过i-HDU认证，请自己修改postDate中的学号和密码
*/
int hdulogin() {
  const char * host = "2.2.2.2";
  const int httpPort = 80;

  WiFiClient client;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return 1;
  }
  delay(10);
  String postDate = "opr=pwdLogin&userName=你的学号&pwd=学号对应的密码&rememberPwd=1";//将从串口接收的数据发送到服务器，readLine()方法可以自行设计
  if (postDate.length() && postDate != "0") {
    String data = (String)postDate;
    int length = data.length();

    String postRequest = (String)("POST ") + "/ac_portal/login.php HTTP/1.1\r\n" +
                         "Host: " + host + "\r\n" +
                         "Connection: Keep Alive\r\n" +
                         "Content-Length: " + length + "\r\n" +
                         "Accept: */*\r\n" +
                         "Origin: http://2.2.2.2\r\n" +
                         "Content-Type: application/x-www-form-urlencoded; charset=UTF-8" + "\r\n" +
                         "User-Agent: zyzandESP8266\r\n" +
                         "\r\n" +
                         data + "\r\n";
    //Serial.println(postRequest);
    Serial.println();
    client.print(postRequest);
    delay(600);
    //处理返回信息
    String line = client.readStringUntil('\n');
    while (client.available() > 0) {
      line += client.readStringUntil('\n');
    }
    //  Serial.println(line);
    client.stop();
    if (line.indexOf("logon success") != -1 || line.indexOf("不需要") != -1) { //认证成功
      return 0;
    }
    else {
      return 2;
    }

  }
  client.stop();
  return 2;
}

int site_Get() {
  int ret = 0;
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;

    Serial.printf("[www.zyzand.com]Connect to www.zyzand.com...\n");
    http.begin("www.zyzand.com", 80, (String)"/IoT/clients/" + SN + "/index.php"); //HTTP
    int httpCode = http.GET();
    int i = 5;//重试次数
    while (i-- > 0 && httpCode != 200) {
      Serial.printf("[www.zyzand.com]code: %d Try again...", httpCode);
      delay(1000);
      httpCode = http.GET();
    }
    //从www.zyzand.com获取信息失败
    if (httpCode != 200) {
      Serial.printf("[www.zyzand.com] GET Fail!");
      ret = 2;
    }
    //成功则进行数据解析
    else  {
      Serial.println("[www.zyzand.com]GET data:");
      String payload = http.getString();
      Serial.println(payload);
      int nflag = payload.indexOf("\n");
      Serial.println(nflag);
      Serial.println(payload.indexOf("\n", nflag));
      dat1 = payload.substring(0, nflag);
      dat2 = payload.substring(nflag + 1, payload.indexOf("\n", nflag + 1));

      Serial.print("[www.zyzand.com]dat1 = ");
      Serial.println(dat1);
      Serial.print("[www.zyzand.com]dat2 = ");
      Serial.println(dat2);

      //OTA
      if (strcmp(dat1.c_str(), "updata") == 0) {
        OTA();
      }
    }
  }
  else {//WiFi连接失败
    Serial.print("[www.zyzand.com]WiFi failed\n");
    ret = 1;
  }

  //失败时的处理
  if (ret != 0) {
    dat1 = "zyzand.com";
    dat2 = (String)"/IoT/clients/" + SN + "/error/index.php";
  }
  return ret;
}

void OTA() {
  HTTPClient http;
  http.begin("www.zyzand.com", 80, (String)"/IoT/clients/" + SN + "/updata_fadeback.php?code=0&error=Start%20updata"); //HTTP
  http.GET();

  t_httpUpdate_return ret = ESPhttpUpdate.update((String)"http://www.zyzand.com/IoT/clients/" + SN + "/updata.bin");

  String fadeback = (String)"/IoT/clients/" + SN + "/updata_fadeback.php?code=";
  int i;
  char c;
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());

      //反馈升级错误信息
      fadeback += ((String)ESPhttpUpdate.getLastError() + "&error=");
      for (i = 0; i < strlen(ESPhttpUpdate.getLastErrorString().c_str()); i++) {
        c = ESPhttpUpdate.getLastErrorString().c_str()[i];
        if (c == ' ') {
          fadeback += "%20";
        }
        else if (c == '[') {
          fadeback += "%5B";
        }
        else if (c == ']') {
          fadeback += "%5D";
        }
        else if (c == ':') {
          fadeback += "%3A";
        }
        else {
          fadeback += c;
        }
      }
      http.begin("www.zyzand.com", 80, fadeback); //HTTP
      http.GET();

      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      //反馈升级信息

      http.begin("www.zyzand.com", 80, (String)"/IoT/clients/" + SN + "/updata_fadeback.php?code=0&error=HTTP_UPDATE_NO_UPDATES"); //HTTP
      http.GET();
      break;

    case HTTP_UPDATE_OK:
      //经测试，升级成功后自动复位，不会执行
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
  while (1) {} //升级失败，停止喂狗，使看门狗复位
}



