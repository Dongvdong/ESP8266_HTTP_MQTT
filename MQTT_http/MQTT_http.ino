#include <ESP8266WiFi.h>
#include <string.h>

//-------HTTP---------
#include <ESP8266HTTPClient.h>

//---------服务器---------
#include <ESP8266WebServer.h>  
ESP8266WebServer server ( 80 );  
#include <FS.h> 


////------是否开启打印-----------------
#define Use_Serial Serial



//------mqtt-----------------
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


//------mqtt头定义-----------------
#define AIO_SERVER      "io.adafruit.com"  //mqtt服务器
#define AIO_SERVERPORT  1883  //端口
#define AIO_USERNAME    ""
#define AIO_KEY         ""
WiFiClient mqttclient;

//------mqtt通信-----------------
Adafruit_MQTT_Client mqtt(&mqttclient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/test", MQTT_QOS_1);

//------mqtt回掉函数-----------------
void onoffcallback(char *data, uint16_t len) {
  Serial.print("Hey we're in a onoff callback, the button value is: ");
  Serial.println(data);
}

//------mqtt连接-----------------
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 10 seconds...");
       mqtt.disconnect();
       delay(10000);  // wait 10 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
 
//-----ESP获取自身ID-----------------
#ifdef ESP8266
extern "C" {
#include "user_interface.h"   //含有system_get_chip_id（）的库
}
#endif


//储存SN号
String SN;


/*----------------WIFI账号和密码--------------*/

// char* ssid = "stu-xdwlan";    // Enter SSID here
// char* password = "";  //Enter Password here

 char ssid[50] = "";    // Enter SSID here
 char password[50] = "";  //Enter Password here

//String ssid = "stu-xdwlan"; 
//String password = ""; 

//--------------HTTP请求------------------
struct http_request {  
  String  Referer;
  char* host;
  int httpPort=80;
  String host_ur ;
  
  String usr_name;//账号
  String usr_pwd;//密码
 
  String postDate;

  };




  
/*---------------------------HTTP请求-------------------------------*/
  
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

    String postRequest =
                         (String)("POST ") + "/"+ruqest.host_ur+" HTTP/1.1\r\n" +
                         "Host: " +ruqest.host + "\r\n" +
                         "Connection: Keep Alive\r\n" +
                         "Content-Length: " + length + "\r\n" +
                         "Accept: */*\r\n" +
                         "Origin: http://"+ruqest.host+"\r\n" +
                          "Upgrade-Insecure-Requests: 1"+"\r\n" +
                         "Content-Type: application/x-www-form-urlencoded;" + "\r\n" +
                         "User-Agent: zyzandESP8266\r\n" +
                          "Accept-Encoding: gzip, deflate"+"\r\n" +
                          "Accept-Language: zh-CN,zh;q=0.9"+"\r\n" +                     
                         "\r\n" +
                         data + "\r\n";

    client.print(postRequest);
    delay(600);
    //处理返回信息
    String line = client.readStringUntil('\n');
    while (client.available() > 0) {
      line += "\r\n"+client.readStringUntil('\n');
    }
    Use_Serial.println(line);
    client.stop();
    
    if (line.indexOf("时间") != -1 || line.indexOf("登陆") != -1) { //认证成功
      return 0;
       Use_Serial.println("time ----------- find ");
    }
    else {
      return 2;
    }

  }
  client.stop();
  return 2;
}


//-----------------HTTP请求---------

void http_wifi(){
  
  // 认证上网
  http_request ruqest;
  ruqest.Referer="http://10.255.44.33/srun_portal_pc.php?ac_id=1&";
  ruqest.host = "10.255.44.33";
  ruqest.httpPort = 80;
  ruqest.host_ur = "srun_portal_pc.php";
  ruqest.usr_name="1601120382";//账号  密码已修改 要+1
  ruqest.usr_pwd="mimaHENFuzb";//密码  密码已修改 要-1
  ruqest.postDate = String("")+"action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&url=&username=+"+ ruqest.usr_name+"&password="+ruqest.usr_pwd;
     

   if (hdulogin(ruqest) == 0) {
    
      Use_Serial.println("WEB Login Success!");
    }
    else {
      
      Use_Serial.println("WEB Login Fail!");
    }
  
  }


//----------------------------------连接WIFI--------------------
  void wifi_Init(){
  Use_Serial.println("Connecting to ");
  Use_Serial.println(ssid);
 
  //connect to your local wi-fi network
  WiFi.begin(ssid, password);
 
  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Use_Serial.print(".");
  }
  Use_Serial.println("");
  Use_Serial.println("WiFi connected");

   SN = (String )system_get_chip_id();
   Use_Serial.println(SN);
  }
  
//------------------------------------------- void 服务器 ------------------------------------------
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

/* NotFound处理 

 * 用于处理没有注册的请求地址 

 * 一般是处理一些页面请求 

 */  

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
void handleMain() {  

  /* 返回信息给浏览器（状态码，Content-type， 内容） 

   * 这里是访问当前设备ip直接返回一个String 

   */  

  Serial.print("handleMain");  

  File file = SPIFFS.open("/index.html", "r");  

  size_t sent = server.streamFile(file, "text/html");  

  file.close();  

  return;  

}  
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

         wifiname.toCharArray(ssid, 50);    // 从网页得到的 WIFI名
         wifipwd.toCharArray(password, 50);  //从网页得到的 WIFI密码
                   
          String backtxt= "wifiname:  "+ wifiname  +"     wifipwd:"+ wifipwd ;// 用于串口和网页返回信息
          
          Use_Serial.println ( backtxt); // 串口打印给电脑
          
          server.send ( 200, "text/html", backtxt); // 网页返回给手机提示

           // wifi连接开始
           wifi_Init();
           
           // 如果是西电无线网络，自动发起网页认证
         if (strcmp(WiFi.SSID().c_str(), "stu-xdwlan") == 0 ) {
           http_wifi();
         }
         
          return;          
           

    } else if(config == "off") { // a=off  
                server.send ( 200, "text/html", "config  is off!");
        return;

    }  

    server.send ( 200, "text/html", "unknown action"); return;  

  }  

  server.send ( 200, "text/html", "action no found");  
  
  }

//------------------------------------------- void setup() ------------------------------------------
void setup() {
  Use_Serial.begin(115200);
  
  delay(1000);
 pinMode(D4, OUTPUT);  

 // WiFi.mode(WIFI_STA);
// 设置内网
  IPAddress softLocal(192,168,4,1);   // 1 设置内网WIFI IP地址
  IPAddress softGateway(192,168,4,1);
  IPAddress softSubnet(255,255,255,0);
  WiFi.softAPConfig(softLocal, softGateway, softSubnet);
   
  String apName = ("ESP8266_"+(String)ESP.getChipId());  // 2 设置WIFI名称
  const char *softAPName = apName.c_str();
   
  WiFi.softAP(softAPName, "");      // 3创建wifi  名称 +密码 adminadmin
   
  IPAddress myIP = WiFi.softAPIP();  // 4输出创建的WIFI IP地址
  Serial.print("AP IP address: ");     
  Serial.println(myIP);
   
  Serial.print("softAPName: ");  // 5输出WIFI 名称
  Serial.println(apName);


  
    SPIFFS.begin();

   server.on ("/", handleMain); // 绑定‘/’地址到handleMain方法处理 ----  返回主页面 一键配网页面 
  server.on ("/pin", HTTP_GET, handlePin); // 绑定‘/pin’地址到handlePin方法处理  ---- 开关灯请求 
   server.on ("/wifi", HTTP_GET, handleWifi); // 绑定‘/wifi’地址到handlePWIFI方法处理  --- 重新配网请求
    server.onNotFound ( handleNotFound ); // NotFound处理
   server.begin(); 
   
   Use_Serial.println ( "HTTP server started" ); 
  
 //WIFI初始化
 //  wifi_Init();
 //MQTT绑定初始化
    onoffbutton.setCallback(onoffcallback);
    mqtt.subscribe(&onoffbutton);
 // 网页认证
//    http_wifi();

}


//------------------------------------------- void void loop()  ------------------------------------------

void loop() 
{

   server.handleClient();  

if(WiFi.status() == WL_CONNECTED){
 
     MQTT_connect();
     mqtt.processPackets(2000);
    if(! mqtt.ping()) {
          mqtt.disconnect();
       } 
    }
 else {
     // Serial.print("wifi... failed\n");   
   }

}




