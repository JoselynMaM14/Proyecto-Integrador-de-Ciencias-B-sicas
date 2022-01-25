/* Fecha: 21 de diciembre de 2021.
 
 */

/*Bibliotecas necesarias para la ejecución del programa*/
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <WiFi.h>  // Biblioteca para el control de WiFi
#include <PubSubClient.h> //Biblioteca para conexion MQTT
#include <DFRobot_MAX30102.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define BOT_TOKEN "5000181782:AAFeV1qAA3S4TonoagO74pME4RwZi74ndXI" // se obtiene al momento de crear el chat bot en telegram
const unsigned long BOT_MTBS = 1000; // mean time between scan messages

/*Conexion ESP32 y sensores
    MLX90614            Esp32    |    MAX30102       ESP32
    Vin                 3.3v     |    Vin            3V3
    GND                 GND      |    GND            GND
    SCL                 GPIO22   |    SCL            GPIO22
    SDA                 GPIO21   |    SDA            GPIO21
*/

//---------------------------Conectividad---------------------------------------------------
//Datos de WiFi

const char* ssid = "*******" ;// Aquí debes poner el nombre de tu red
const char* password = "******";  // Aquí debes poner la contraseña de tu red

//Datos del broker MQTT
const char* mqtt_server = "3.65.154.195"; // Si estas en una red local, coloca la IP asignada, en caso contrario, coloca la IP publica
IPAddress server(3,65,154,195);

// Objetos
WiFiClient espClient; // Este objeto maneja los datos de conexion WiFi
PubSubClient client(espClient); // Este objeto maneja los datos de conexion al broker

//-------------------Data sensors-----------------------------------------------------------------
//Declaración del objeto.
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
DFRobot_MAX30102 particleSensor;  //reconocimiento de sensor
//Variables sensor MLX90614
float TempMed;
float TempReal;

// Variables sensor MAX30102
int32_t SPO2; //SPO2
int8_t SPO2Valid; //Flag to display if SPO2 calculation is valid
int32_t heartRate; //Heart-rate
int8_t heartRateValid;

/*Declaración de las variables que nos serviran como temporizadores*/
unsigned long timeNow;//Almacenaremos el tiempo en ms
unsigned long timeNow_MAX;//Almacenaremos el tiempo en ms
unsigned long PreviousTime_MLX90614=0;
unsigned long PreviousTime_MAX30102=0;
unsigned long timeLast_sensors;// Variable que nos permite controlar el tiempo de los sensores junto con previous

/*Declaración de las variables que nos permiten determinar el tiempo de retardo para ejecutar 
cada acción simulando un delay*/

const int Time_MLX90614=5000;//Declaramos que la medición del sensor de temperatura se ejecute cada 15s
const int Time_MAX30102=5000;//Declaramos que la medición del sensor de temperatura se ejecute cada 15s

/*Variables de conectividad wifi*/

int flashLedPin = 2;  // Para indicar el estatus de conexión
int statusLedPin = 19; // Para ser controlado por MQTT
unsigned long timeLast;// Variable de control de tiempo no bloqueante
unsigned long timeLast_MAX;// Variable de control de tiempo no bloqueante
const int wait = 5000;  // Indica la espera cada 5 segundos para envío de mensajes MQTT
const int wait_MAX = 5000;  // Indica la espera cada 5 segundos para envío de mensajes MQTT

/*-----------Sección de telegram---------------------------------------------------*/
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done
/*-----------------Variables para comprobar status de telegram----------------*/
int SPO2andBPMstatus=0;
int temperaturaStatus=0;

void setup() {
 Serial.begin(115200);
 //inicializamos al sensor mlx90614
 mlx.begin();
  //Init serial
  while (!particleSensor.begin()) {
    Serial.println("MAX30102 was not found");
    delay(1000);
  }

 
  particleSensor.sensorConfiguration(/*ledBrightness=*/50, /*sampleAverage=*/SAMPLEAVG_4, \
                        /*ledMode=*/MODE_MULTILED, /*sampleRate=*/SAMPLERATE_100, \
                        /*pulseWidth=*/PULSEWIDTH_411, /*adcRange=*/ADCRANGE_16384);

  //Setup del mlx90614
  mlx.begin(); //Se inicia el sensor
                        
 /*Inicializamos los pines del ESP32CAM como SDA y SCL*/
 //declaración de los pines que nos permitiran declarar la conectividad a internet.
 pinMode (flashLedPin, OUTPUT);
 pinMode (statusLedPin, OUTPUT);
 digitalWrite (flashLedPin, LOW);
 digitalWrite (statusLedPin, HIGH);

  Serial.println();
  Serial.println();
  Serial.print("Conectar a ");
  Serial.println(ssid);
 
  WiFi.begin(ssid, password); // Esta es la función que realiz la conexión a WiFi
 
  while (WiFi.status() != WL_CONNECTED) { // Este bucle espera a que se realice la conexión
    digitalWrite (statusLedPin, HIGH);
    delay(500); //dado que es de suma importancia esperar a la conexión, debe usarse espera bloqueante
    digitalWrite (statusLedPin, LOW);
    Serial.print(".");  // Indicador de progreso
    delay (5);
  }
  
  // Cuando se haya logrado la conexión, el programa avanzará, por lo tanto, puede informarse lo siguiente
  Serial.println();
  Serial.println("WiFi conectado");
  Serial.println("Direccion IP: ");
  Serial.println(WiFi.localIP());

  // Si se logro la conexión, encender led
  if (WiFi.status () > 0){
  digitalWrite (statusLedPin, LOW);
  }
  
  delay (1000); // Esta espera es solo una formalidad antes de iniciar la comunicación con el broker

  //Conexión con el broker MQTT
  client.setServer(server, 1883); // Conectarse a la IP del broker en el puerto indicado
  client.setCallback(callback); // Activar función de CallBack, permite recibir mensajes MQTT y ejecutar funciones a partir de ellos
  delay(1500);  // Esta espera es preventiva, espera a la conexión para no perder información

  /*-------------------------Sección conexión TELEGRAM setup----------------------------------*/
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

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

void loop() {
 timeLast_sensors=millis();
 timeNow = millis(); // Control de tiempo para esperas no bloqueantes
 timeNow_MAX=millis();
 
/*---------------MQTT-----------------------------------------------------------------*/
 //Verificar siempre que haya conexión al broker
  if (!client.connected()) {
    reconnect();  // En caso de que no haya conexión, ejecutar la función de reconexión, definida despues del void setup ()
  }// fin del if (!client.connected())
  client.loop(); // Esta función es muy importante, ejecuta de manera no bloqueante las funciones necesarias para la comunicación con el broker

  /*-----------------Looop de conexión TELEGRAM--------------------------------------------------*/

  
  
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
/*------------------------Rutina para llamar a las funciones de los sensores---------------------------------------------------*/
int interruptor_sensores=0;
if(temperaturaStatus==1){
  interruptor_sensores=1;
}
if(SPO2andBPMstatus==1){
  interruptor_sensores=2;
}
switch (interruptor_sensores) {
  case 1:
    MLX90614();
    break;
  case 2:
    MAX30102();
    break;
}
}
/*------------------------------Sección de funciones de conectividad a MQTT------------------------------------------------------------*/
// Función para reconectarse
void reconnect() {
  // Bucle hasta lograr conexión
  while (!client.connected()) { // Pregunta si hay conexión
    Serial.print("Tratando de contectarse...");
    // Intentar reconexión
    if (client.connect("ESP32CAMClient")) { //Pregunta por el resultado del intento de conexión
      Serial.println("Conectado");
      client.subscribe("SignosVitales/Temperatura/CasaRetiro1"); // Esta función realiza la suscripción al tema
      client.subscribe("SignosVitales/Oxigenacion/CasaRetiro1");
      client.subscribe("SignosVitales/bpm/CasaRetiro1");
    }// fin del  if (client.connect("ESP32CAMClient"))
    else {  //en caso de que la conexión no se logre
      Serial.print("Conexion fallida, Error rc=");
      Serial.print(client.state()); // Muestra el codigo de error
      Serial.println(" Volviendo a intentar en 5 segundos");
      // Espera de 5 segundos bloqueante
      delay(5000);
      Serial.println (client.connected ()); // Muestra estatus de conexión
    }// fin del else
  }// fin del bucle while (!client.connected())
}// fin de void reconnect(


// Esta función permite tomar acciones en caso de que se reciba un mensaje correspondiente a un tema al cual se hará una suscripción
 void callback(char* topic, byte* message, unsigned int length) {
  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageTemp; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageTemp);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32CAM está suscrito al tema SignosVitales/Temperatura/CasaRetiro"
  if (String(topic) == "SignosVitales/Temperatura/CasaRetiro1") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageTemp == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")
    else if(messageTemp == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")


//-------------------------------------------------------------------------------------------------
  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageOX; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageOX += (char)message[i];
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageOX);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32CAM está suscrito al tema SignosVitales/Temperatura/CasaRetiro"
  if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageOX == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1"")
    else if(messageOX == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")



  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageBPM; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageBPM += (char)message[i];
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageBPM);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32CAM está suscrito al tema SignosVitales/Temperatura/CasaRetiro"
  if (String(topic) == "SignosVitales/bpm/CasaRetiro1") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageBPM == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/bpm/CasaRetiro1"")
    else if(messageBPM == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/bpm/CasaRetiro1"")
}




/*--------------------------------Sección de funciones de los sensores--------------------------------------------------------------*/
/*Generamos las funciones de cada sensor para tener un código más ordenado */
void MLX90614(){
   if (timeNow - timeLast > wait) { // Manda un mensaje por MQTT cada cinco segundos
    timeLast = timeNow; // Actualización de seguimiento de tiempo

    //Lectura del sensor de temperatura mlx90614 sin contemplar el error
    TempMed=mlx.readObjectTempC();//Lectura del sensor
    TempReal=TempMed+4.38;//lectura tomando en cuenta el error    
    char dataString[8]; // Define una arreglo de caracteres para enviarlos por MQTT, especifica la longitud del mensaje en 8 caracteres
    dtostrf(TempReal, 1, 2, dataString);  // Esta es una función nativa de leguaje AVR que convierte un arreglo de caracteres en una variable String
    Serial.print("La temperarura es: "); // Se imprime en monitor solo para poder visualizar que el evento sucede
    Serial.println(dataString);
    Serial.println();
    delay(1000);
    client.publish("SignosVitales/Temperatura/CasaRetiro1", dataString); // Esta es la función que envía los datos por MQTT, especifica el tema y el valor. Es importante cambiar el tema para que sea unico SignosVitales/Temperatura/Temaconotronombre
  }// fin del if (timeNow - timeLast > wait)  
}

void MAX30102()
{
 if (timeNow_MAX - timeLast_MAX >wait_MAX) {
  timeLast_MAX=timeNow_MAX;
  Serial.println(F("Espera 4 segundos"));
  particleSensor.heartrateAndOxygenSaturation(/**SPO2=*/&SPO2, /**SPO2Valid=*/&SPO2Valid, /**heartRate=*/&heartRate, /**heartRateValid=*/&heartRateValid);
  char dataStringspo2[8];
  char dataStringhb[8];
  //Esta sección nos ayuda a evitar un poco de ruido del sensor MAX30102
  if(SPO2<0){
    SPO2=0;
    }
  if(heartRate<0){
    heartRate=0;
    }
  dtostrf(SPO2, 1, 2, dataStringspo2);
  dtostrf(heartRate, 1, 2, dataStringhb);
  Serial.print("SPO2: ");
  Serial.println(dataStringspo2);
  Serial.print("Bpm: ");
  Serial.println(dataStringhb);
  client.publish("SignosVitales/Oxigenacion/CasaRetiro1", dataStringspo2);
  client.publish("SignosVitales/bpm/CasaRetiro1", dataStringhb);
 }
}

/*---------------------Sección de envío de mensaje telegram--------------------------*/

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
      welcome += "Este servicio te pemitirá conocer tu Temperatura, SPO2 y BPM, el cual podrás visualizar en Node-Red\n";
      welcome += "Selecciona el texto en azul, segun sea el caso:\n";
      welcome += "/TemperaturaON: Para visualizar tu temperatura\n";
      welcome += "/TemperaturaOFF: Para dejar de tomar la temperatura\n";
      welcome += "/SPO2andBPMON: Para visualizar la oxigenación y los latidos por minuto\n";
      welcome += "/SPO2andBPMOFF: Para dejar de visualizar la oxigenación y los latidos por minuto\n";
      welcome += "/status: Para conocer la función que se tiene activada \n";
      welcome += "Instrucciones de uso: \n";
      welcome += "1. Elegir el signo vital a medir. \n";
      welcome += "Para ello seleccione el comando /TemperaturaON o /SPO2andBPMON segun sea el caso \n";
      welcome += "Para apagar la medicion seleccionada se hace uso del comando /TemperaturaOFF o /SPO2andBPMOFF segun sea el caso.\n";
      welcome += "NOTA: EL ULTIMO VALOR MEDIDO POR LOS SENSORES SERÁ EL ALMACENADO\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}
