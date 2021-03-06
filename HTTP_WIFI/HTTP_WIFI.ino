/*


1 上电从rom读取上次WIFI地址
  成功-1网页交互  2MQTT通信
  失败-进入网页模式，等待新的密码


2 
  添加上电检测重连wifi, 如果是普通直连，如果是网页认证，直连+网页请求
*/


//------是否开启打印-----------------
#define Use_Serial Serial


//-----------------------------A-1 库引用开始-----------------//
#include <string.h>
#include <math.h>  // 数学工具库
#include <EEPROM.h>// eeprom  存储库

// WIFI库
#include <ESP8266WiFi.h>
//-------客户端---------
#include <ESP8266HTTPClient.h>
//---------服务器---------
#include <ESP8266WebServer.h>  
#include <FS.h> 

//Mqtt
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


//ESP获取自身ID
#ifdef ESP8266
extern "C" {
#include "user_interface.h"   //含有system_get_chip_id（）的库
}
#endif

//-----------------------------A-1 库引用结束-----------------//




//-----------------------------A-2 变量声明开始-----------------//
int PIN_Led = D4;
bool PIN_Led_State=0;
ESP8266WebServer server ( 80 );  
//储存SN号
String SN;

/*----------------WIFI账号和密码--------------*/
 char ssid[50] = "";    // Enter SSID here
 char password[50] = "";  //Enter Password here
 
#define DEFAULT_STASSID "lovelamp"
#define DEFAULT_STAPSW  "lovelamp" 

//下图说明了Arduino环境中使用的Flash布局：

//|--------------|-------|---------------|--|--|--|--|--|
//^              ^       ^               ^     ^
//Sketch    OTA update   File system   EEPROM  WiFi config (SDK)


// 用于存上次的WIFI和密码
#define MAGIC_NUMBER 0xAA
struct config_type
{
  char stassid[50];
  char stapsw[50];
  uint8_t magic;
};
config_type config_wifi;

//------mqtt头定义-----------------
#define AIO_SERVER      "www.dongvdong.top"  //mqtt服务器
#define AIO_SERVERPORT  1883  //端口
#define AIO_USERNAME    "dongdong"
#define AIO_KEY         "dongdong"
WiFiClient mqttclient;

//------mqtt通信-----------------
Adafruit_MQTT_Client mqtt(&mqttclient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/test", MQTT_QOS_1);



 
//--------------HTTP请求------------------
struct http_request {  
  String  Referer;
  char host[20];
  int httpPort=80;
  String host_ur ;
  
  String usr_name;//账号
  String usr_pwd;//密码
 
  String postDate;

  };

String http_html;
//-----------------------------A-2 变量声明结束-----------------//


//-----------------------------A-3 函数声明开始-----------------//
void wifi_Init();
void saveConfig();
void loadConfig();
int hdulogin(struct http_request ruqest) ;
void http_wifi_xidian();
String getContentType(String filename);
void handleNotFound();
void handleMain();
void handlePin();
void handleWifi();
void http_jiexi(http_request ruqest);
void handleWifi_wangye();
void handleTest();
void MQTT_connect();
void onoffcallback(char *data, uint16_t len) ;
//---------------------------A-3 函数声明结束------------------------//


//-----------------------------A-3 函数-----------------//
//3-1管脚初始化
void get_espid(){
   SN = (String )system_get_chip_id();
  Use_Serial.println(SN);
  
  }
void LED_Int(){
  
   pinMode(D4, OUTPUT); 
  
  }
//3-2WIFI初始化
void wifi_Init(){

  
  Use_Serial.println("Connecting to ");
  Use_Serial.println(config_wifi.stassid);
  WiFi.begin(config_wifi.stassid, config_wifi.stapsw);
 //int retries=10;
  while (WiFi.status() != WL_CONNECTED) {
    
  server.handleClient(); 
// retries--;
 //if (retries == 0) {
 // Use_Serial.println("WIFI connect 3 times fail!");
 //  return;
 //}
       
  ESP.wdtFeed();
  delay(500);
  server.handleClient(); 
  
  Use_Serial.print(".");
  PIN_Led_State = !PIN_Led_State;
  digitalWrite(PIN_Led, PIN_Led_State);
 
  }

  Use_Serial.println("--------------WIFI CONNECT!-------------  ");
  Use_Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
  Use_Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
  Use_Serial.println("----------------------------------------  ");
    
 
  }



/*
 *3-/2 保存参数到EEPROM
*/
void saveConfig()
{
  Serial.println("Save config!");
  Serial.print("stassid:");
  Serial.println(config_wifi.stassid);
  Serial.print("stapsw:");
  Serial.println(config_wifi.stapsw);
  EEPROM.begin(1024);
  uint8_t *p = (uint8_t*)(&config_wifi);
  for (int i = 0; i < sizeof(config_wifi); i++)
  {
    EEPROM.write(i, *(p + i));
  }
  EEPROM.commit();
}
/*
 * 从EEPROM加载参数
*/
void loadConfig()
{
  EEPROM.begin(1024);
  uint8_t *p = (uint8_t*)(&config_wifi);
  for (int i = 0; i < sizeof(config_wifi); i++)
  {
    *(p + i) = EEPROM.read(i);
  }
  EEPROM.commit();
  //出厂自带
  if (config_wifi.magic != MAGIC_NUMBER)
  {
    strcpy(config_wifi.stassid, DEFAULT_STASSID);
    strcpy(config_wifi.stapsw, DEFAULT_STAPSW);
    config_wifi.magic = MAGIC_NUMBER;
    saveConfig();
    Serial.println("Restore config!");
  }
  Serial.println(" ");
  Serial.println("-----Read config-----");
  Serial.print("stassid:");
  Serial.println(config_wifi.stassid);
  Serial.print("stapsw:");
  Serial.println(config_wifi.stapsw);
  Serial.println("-------------------");

}
 
  
//3-3ESP8266建立无线热点
void SET_AP(){
  
  // 设置内网
  IPAddress softLocal(192,168,4,1);   // 1 设置内网WIFI IP地址
  IPAddress softGateway(192,168,4,1);
  IPAddress softSubnet(255,255,255,0);
  WiFi.softAPConfig(softLocal, softGateway, softSubnet);
   
  String apName = ("ESP8266_"+(String)ESP.getChipId());  // 2 设置WIFI名称
  const char *softAPName = apName.c_str();
   
  WiFi.softAP(softAPName, "admin");      // 3创建wifi  名称 +密码 adminadmin

  Use_Serial.print("softAPName: ");  // 5输出WIFI 名称
  Use_Serial.println(apName);

  IPAddress myIP = WiFi.softAPIP();  // 4输出创建的WIFI IP地址
  Use_Serial.print("AP IP address: ");     
  Use_Serial.println(myIP);
  
  }
//3-4ESP建立网页服务器
void Server_int(){
  
   server.on ("/", handleMain); // 绑定‘/’地址到handleMain方法处理 ----  返回主页面 一键配网页面 
  server.on ("/pin", HTTP_GET, handlePin); // 绑定‘/pin’地址到handlePin方法处理  ---- 开关灯请求 
   server.on ("/wifi", HTTP_GET, handleWifi); // 绑定‘/wifi’地址到handlePWIFI方法处理  --- 重新配网请求
    server.on ("/wifi_wangye", HTTP_GET, handleWifi_wangye);
    server.on ("/test", HTTP_GET, handleTest);
    server.onNotFound ( handleNotFound ); // NotFound处理
   server.begin(); 
   
   Use_Serial.println ( "HTTP server started" );
  
  }
//3-5-1 网页服务器主页
void handleMain() {  

  /* 返回信息给浏览器（状态码，Content-type， 内容） 

   * 这里是访问当前设备ip直接返回一个String 

   */  

  Serial.print("handleMain");  

//打开一个文件。path应该是以斜线开头的绝对路径（例如/dir/filename.txt）。mode是一个指定访问模式的字符串。
//它可以是“r”，“w”，“a”，“r +”，“w +”，“a +”之一。这些模式的含义与fopenC功能相同
  File file = SPIFFS.open("/index.html", "r");  

  size_t sent = server.streamFile(file, "text/html");  

  file.close();  

  return;  

}  
//3-5-2 网页控制引脚
/* 引脚更改处理 

 * 访问地址为htp://192.162.xxx.xxx/pin?a=XXX的时候根据a的值来进行对应的处理 

 */  

void handlePin() {  

  if(server.hasArg("a")) { // 请求中是否包含有a的参数  

    String action = server.arg("a"); // 获得a参数的值  

    if(action == "on") { // a=on  

      digitalWrite(2, LOW); // 点亮8266上的蓝色led，led是低电平驱动，需要拉低才能亮  

      server.send ( 200, "text/html", "Pin 2 has turn on"); return; // 返回数据  

    } else if(action == "off") { // a=off  

      digitalWrite(2, HIGH); // 熄灭板载led  

      server.send ( 200, "text/html", "Pin 2 has turn off"); return;  

    }  

    server.send ( 200, "text/html", "unknown action"); return;  

  }  

  server.send ( 200, "text/html", "action no found");  

}  

//3-5-3 网页修改普通家庭WIFI连接账号密码
/* WIFI更改处理 

 * 访问地址为htp://192.162.xxx.xxx/wifi?config=on&name=Testwifi&pwd=123456
  根据wifi进入 WIFI数据处理函数
  根据config的值来进行 on
  根据name的值来进行  wifi名字传输
  根据pwd的值来进行   wifi密码传输


 */  


void handleWifi(){
  
  
   if(server.hasArg("config")) { // 请求中是否包含有a的参数  

    String config = server.arg("config"); // 获得a参数的值  
        String wifiname;
        String wifipwd;

     
        
    if(config == "on") { // a=on  
          if(server.hasArg("name")) { // 请求中是否包含有a的参数  
        wifiname = server.arg("name"); // 获得a参数的值

          }
          
    if(server.hasArg("pwd")) { // 请求中是否包含有a的参数  
         wifipwd = server.arg("pwd"); // 获得a参数的值    
           }
                  
          String backtxt= "Wifiname: "+ wifiname  +"/r/n wifipwd: "+ wifipwd ;// 用于串口和网页返回信息
          
          Use_Serial.println ( backtxt); // 串口打印给电脑
          
          server.send ( 200, "text/html", backtxt); // 网页返回给手机提示
           // wifi连接开始

         wifiname.toCharArray(config_wifi.stassid, 50);    // 从网页得到的 WIFI名
         wifipwd.toCharArray(config_wifi.stapsw, 50);  //从网页得到的 WIFI密码               
         
         saveConfig();
         wifi_Init();
 
              
     
       
          return;          
           

    } else if(config == "off") { // a=off  
                server.send ( 200, "text/html", "config  is off!");
        return;

    }  

    server.send ( 200, "text/html", "unknown action"); return;  

  }  

  server.send ( 200, "text/html", "action no found");  
  
  }


//3-5-4  网页认证西电上网-- 无用
void http_wifi_xidian(){

////  通过上网认证，请自己修改postDate中的学号和密码
 // 例如：
  /*网页认证上网post*/
//  String  Referer="http://10.255.44.33/srun_portal_pc.php?ac_id=1&";
//  char* host = "10.255.44.33";
//  int httpPort = 80;
//  String host_ur = "srun_portal_pc.php";

/*网页认证上网模式 */
// String usr_name;//账号
// String usr_pwd;//密码
//  String postDate = String("")+"action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&url=&username=+"+usr_name+"&password="+usr_pwd;
  
  // 认证上网
  http_request ruqest;
  ruqest.Referer="http://10.255.44.33/srun_portal_pc.php?ac_id=1&";
  String("10.255.44.33").toCharArray( ruqest.host , 20); 
  ruqest.httpPort = 80;
  ruqest.host_ur = "srun_portal_pc.php";
  ruqest.usr_name="1601120383";//账号  密码已修改 要+1
  ruqest.usr_pwd="mimaHENFuza";//密码  密码已修改 要-1
  ruqest.postDate = String("")+"action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&url=&username=+"+ ruqest.usr_name+"&password="+ruqest.usr_pwd;
     

   if (hdulogin(ruqest) == 0) {
    
      Use_Serial.println("WEB Login Success!");
    }
    else {
      
      Use_Serial.println("WEB Login Fail!");
    }
  
  }


//3-5-5-1 网页认证上网
void handleWifi_wangye(){
  if(server.hasArg("config")) { // 请求中是否包含有a的参数  
// 1 解析数据是否正常
    String config = server.arg("config"); // 获得a参数的值  
        String wifi_wname,wifi_wpwd,wifi_wip,wifi_postdata;
          
  if(config == "on") { // a=on  
        if(server.hasArg("wifi_wname")) { // 请求中是否包含有a的参数  
        wifi_wname = server.arg("wifi_wname"); // 获得a参数的值
           if(wifi_wname.length()==0){            
             server.send ( 200, "text/html", "please input WIFI名称!"); // 网页返回给手机提示
            return;} 
          }
        if(server.hasArg("wifi_wpwd")) { // 请求中是否包含有a的参数  
        wifi_wpwd = server.arg("wifi_wpwd"); // 获得a参数的值
               
          }
     if(server.hasArg("wifi_wip")) { // 请求中是否包含有a的参数  
         wifi_wip = server.arg("wifi_wip"); // 获得a参数的值    
          if(wifi_wip.length()==0){            
             server.send ( 200, "text/html", "please input 登陆网址!"); // 网页返回给手机提示
            return;} 

           }
        if(server.hasArg("wifi_postdata")) { // 请求中是否包含有a的参数  

            String message=server.arg(3);
            for (uint8_t i=4; i<server.args(); i++){
               message += "&" +server.argName(i) + "=" + server.arg(i) ;
             }
  
         wifi_postdata = message; // 获得a参数的值 
            if(wifi_postdata.length()==0){            
             server.send ( 200, "text/html", "please input 联网信息!"); // 网页返回给手机提示
            return;}    
           }
       
//--------------------------------- wifi连接-------------------------------------------------
         wifi_wname.toCharArray(config_wifi.stassid, 50);    // 从网页得到的 WIFI名
         wifi_wpwd.toCharArray(config_wifi.stapsw, 50);  //从网页得到的 WIFI密码               
         
         saveConfig();
      
         // wifi连接开始
         wifi_Init();
 
  //--------------------------------- 认证上网-------------------------------------------------      


  
// 2 发送认证
                
// ruqest.Referer="http://10.255.44.33/srun_portal_pc.php";
  http_request ruqest_http;
  ruqest_http.httpPort = 80; 
  ruqest_http.Referer=wifi_wip;// 登录网址
  ruqest_http.postDate=wifi_postdata;
   
   Use_Serial.println (wifi_wip);
   Use_Serial.println (wifi_postdata);
  

    
  http_jiexi(ruqest_http);
          
           return; 
  } else if(config == "off") { // a=off  
                server.send ( 200, "text/html", "config  is off!");
        return;
  }  
    server.send ( 200, "text/html", "unknown action"); return;  
  }  
      server.send ( 200, "text/html", "action no found");  

  }

//3-5-5-2 网页解析IP消息 
void http_jiexi(http_request ruqest){
  
    int datStart ; int datEnd;

    // 1 解析ip
   //   http://10.255.44.33/ 
    char h[]="http://";  char l[]="/";
    datStart  =  ruqest.Referer.indexOf(h)+ strlen(h);
    datEnd= ruqest.Referer.indexOf(l,datStart);
    String host=ruqest.Referer.substring(datStart, datEnd);
    host.toCharArray( ruqest.host , 20); 
    
    Use_Serial.println ( ruqest.host); // 串口打印给电脑

    // 2 解析请求的网页文件-登录页面
   //   srun_portal_pc.php?ac_id&";
    char s[] ="?"; 
    datStart  = ruqest.Referer.indexOf(l,datStart)+strlen(l);
   // datEnd = ruqest.Referer.indexOf(s,datStart)-strlen(s)+1;
   if(ruqest.Referer.indexOf(s,datStart)==-1)
    {datEnd = ruqest.Referer.length();}
    else{    
       datEnd = ruqest.Referer.indexOf(s,datStart)-1;
      }
    ruqest.host_ur  = String(ruqest.Referer.substring(datStart, datEnd));
    Use_Serial.println ( ruqest.host_ur); // 串口打印给电脑

 
    Use_Serial.println( ruqest.postDate);
     
   if (hdulogin(ruqest) == 0) {   
      Use_Serial.println("WEB Login Success!");
    }
    else {  
      Use_Serial.println("WEB Login Fail!");
    }    
          return;  
  
  }

//3-5-5-3 网页认证发送HTTP请求
/*---------------------------------------------------------------*/
int hdulogin(struct http_request ruqest) {
  WiFiClient client;

  if (!client.connect(ruqest.host, ruqest.httpPort)) {
    Use_Serial.println("connection failed");
    return 1;
  }
  delay(10);
 
  if (ruqest.postDate.length() && ruqest.postDate != "0") {
    String data = (String)ruqest.postDate;
    int length = data.length();

/*
gzip：GNU压缩格式

compress：UNIX系统的标准压缩格式

deflate：是一种同时使用了LZ77和哈弗曼编码的无损压缩格式

identity：不进行压缩
*/
    String postRequest =
                         (String)("POST ") + "/"+ruqest.host_ur+" HTTP/1.1\r\n" +
                         "Host: " +ruqest.host + "\r\n" +
                         "Connection: Keep Alive\r\n" +
                         "Content-Length: " + length + "\r\n" +
                         "Accept:text/html,application/xhtml+xml,application/xml;*/*\r\n" +
                         "Origin: http://"+ruqest.host+"\r\n" +
                          "Upgrade-Insecure-Requests: 1"+"\r\n" +
                         "Content-Type: application/x-www-form-urlencoded;" + "\r\n" +
                         "User-Agent: zyzandESP8266\r\n" +
                        //  "Accept-Encoding: gzip, deflate"+"\r\n" +
                          "Accept-Encoding: identity"+"\r\n" +
                          "Accept-Language: zh-CN,zh;q=0.9"+"\r\n" +                                             
                         "\r\n" +
                         data + "\r\n";
//  String postDate = String("")+"action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&url=&username=+"+usr_name+"&password="+usr_pwd;
    client.print(postRequest);
    delay(600);
    //处理返回信息
    String line = client.readStringUntil('\n');
    Use_Serial.println(line);
    while (client.available() > 0) {
    Use_Serial.println(client.readStringUntil('\n'));
    line += client.readStringUntil('\n');
    
   // line += client.readStringUntil('\n');
    }

    http_html=line;
    
    client.stop();
    
    if (line.indexOf("时间") != -1 || line.indexOf("登陆") != -1) { //认证成功
      return 0;
    
    }
    else {
      return 2;
    }

  }
  client.stop();
  return 2;
}

//3-5-6 网页测试
void handleTest(){ 
     server.send(200, "text/html", http_html);
  }

//3-5-。。。 网页没有对应请求如何处理
void handleNotFound() {  

  String path = server.uri();  

  Serial.print("load url:");  

  Serial.println(path);  

  String contentType = getContentType(path);  

  String pathWithGz = path + ".gz";  

  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  

    if(SPIFFS.exists(pathWithGz))  

      path += ".gz";  

    File file = SPIFFS.open(path, "r");  

    size_t sent = server.streamFile(file, contentType);  

    file.close(); 

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message); 

    return;  

  }  

  String message = "File Not Found\n\n";  

  message += "URI: ";  

  message += server.uri();  

  message += "\nMethod: ";  

  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";  

  message += "\nArguments: ";  

  message += server.args();  

  message += "\n";  

  for ( uint8_t i = 0; i < server.args(); i++ ) {  

    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";  

  }  

  server.send ( 404, "text/plain", message );  

}  
// 解析请求的文件
/** 

 * 根据文件后缀获取html协议的返回内容类型 

 */  

String getContentType(String filename){  

  if(server.hasArg("download")) return "application/octet-stream";  

  else if(filename.endsWith(".htm")) return "text/html";  

  else if(filename.endsWith(".html")) return "text/html";  

  else if(filename.endsWith(".css")) return "text/css";  

  else if(filename.endsWith(".js")) return "application/javascript";  

  else if(filename.endsWith(".png")) return "image/png";  

  else if(filename.endsWith(".gif")) return "image/gif";  

  else if(filename.endsWith(".jpg")) return "image/jpeg";  

  else if(filename.endsWith(".ico")) return "image/x-icon";  

  else if(filename.endsWith(".xml")) return "text/xml";  

  else if(filename.endsWith(".pdf")) return "application/x-pdf";  

  else if(filename.endsWith(".zip")) return "application/x-zip";  

  else if(filename.endsWith(".gz")) return "application/x-gzip";  

  return "text/plain";  

}  


//3-6-1 MQTT初始化  -----------------------------------

void Mqtt_int(){
   //MQTT绑定初始化
    onoffbutton.setCallback(onoffcallback);
    mqtt.subscribe(&onoffbutton);
  }


//------mqtt回调函数-----------------
void onoffcallback(char *data, uint16_t len) {
  Serial.print("Hey we're in a onoff callback, the button value is: ");
  Serial.println(data);
}

//3-6-2------mqtt连接检测-----------------
void MQTT_reconnect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Use_Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    
       server.handleClient();  
       
       Use_Serial.println(mqtt.connectErrorString(ret));
       Use_Serial.println("Retrying MQTT connection in 10 seconds...");
       mqtt.disconnect();
       delay(10000);  // wait 10 seconds
      retries--;
     if (retries == 0) {
         // basically die and wait for WDT to reset me
         //while (1);
         Use_Serial.println("Retrying MQTT connection 3 fali...");
         return;
      }
  }
  Use_Serial.println("MQTT Connected!");

   
}

////3-6-3 MQTT主函数
void Mqtt_Domain(){
   MQTT_reconnect();
   mqtt.processPackets(2000);
  if(! mqtt.ping()) {
       mqtt.disconnect();
  } 
    
  }

//------------------------------------------- void setup() ------------------------------------------

void setup() {
    Use_Serial.begin(115200);  
    get_espid();
    LED_Int();  
     
   // loadConfig();// 读取信息 WIFI
   // wifi_Init(); // 3次尝试
   
    SET_AP(); // 建立WIFI
    SPIFFS.begin(); // 文件系统 启用 此方法装入SPIFFS文件系统。必须在使用任何其他FS API之前调用它。如果文件系统已成功装入，则返回true，否则返回false。
    Server_int();
    Mqtt_int();
}


//------------------------------------------- void void loop()  ------------------------------------------

void loop() 
{

   // 等待网页请求
   server.handleClient();  

   // 等待WIFI连接
   if(WiFi.status() == WL_CONNECTED){  
    // WIFI连接正常，连接MQTT通信
   Mqtt_Domain();
                   }
   else {
    //WIFI断开，重新连接WIFI
      loadConfig();
      wifi_Init();     
         }

}




