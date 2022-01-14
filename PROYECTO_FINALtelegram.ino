#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Wifi network station credentials
/*#define WIFI_SSID "Totalplay-559E"
#define WIFI_PASSWORD "559EB28CPBfPW9Vu"*/

#define WIFI_SSID "MIRANDA"
#define WIFI_PASSWORD "369369-+0@@@@@"

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "5000181782:AAFeV1qAA3S4TonoagO74pME4RwZi74ndXI"

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

int SPO2andBPMstatus=0;
int temperaturaStatus=0;

void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/TemperaturaON")
    {
      temperaturaStatus= 1;
      Serial.print("temperaturaStatus= ");
      Serial.println(temperaturaStatus);
      bot.sendMessage(chat_id, "Temperatura is ON", "");
    }

    if (text == "/TemperaturaOFF")
    {
      temperaturaStatus= 0;
      Serial.print("temperaturaStatus= ");
      Serial.println(temperaturaStatus);
      bot.sendMessage(chat_id, "Temperatura is OFF", "");
    }

    if (text == "/SPO2andBPMON")
    {
      SPO2andBPMstatus= 1;
      Serial.print("SPO2andBPMstatus= ");
      Serial.println(SPO2andBPMstatus);
      bot.sendMessage(chat_id, "SPO2andBPMstatus is ON", "");
    }
    
    if (text == "/SPO2andBPMOFF")
    {
      SPO2andBPMstatus= 0;
      Serial.print("SPO2andBPMstatus= ");
      Serial.println(SPO2andBPMstatus);
      bot.sendMessage(chat_id, "SPO2andBPMstatus is OFF", "");
    }

    if (text == "/status")
    {
      if (temperaturaStatus==1)
      {
        bot.sendMessage(chat_id, "temperatura is ON", "");
      }
      else
      {
        bot.sendMessage(chat_id, "temperatura is OFF", "");
      }
      if (SPO2andBPMstatus==1)
      {
        bot.sendMessage(chat_id, "SPO2andBPM is ON", "");
      }
      else
      {
        bot.sendMessage(chat_id, "SPO2andBPM is OFF", "");
      }
    }

    if (text == "/start")
    {
      String welcome = "Bienvenido a tu servicio de monitoreo de salud " + from_name + ".\n";
      welcome += "Este servicio te pemitirá conocer tu Temperatura, SPO2 y BPM\n";
      welcome += "Escribe:\n";
      welcome += "/TemperaturaON: Para visualizar tu temperatura\n";
      welcome += "/TemperaturaOFF: Para dejar de tomar la temperatura\n";
      welcome += "/SPO2andBPMON: Para visualizar la oxigenación y los latidos por minuto\n";
      welcome += "/SPO2andBPMOFF: Para dejar de visualizar la oxigenación y los latidos por minuto\n";
      welcome += "/status: Para conocer la función que tienes activa";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}


void setup()
{
  Serial.begin(115200);
  Serial.println();
  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}
