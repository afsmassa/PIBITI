/*
   Autor: André Massa
   Data: 17/04/17
   Versão 5: versão 4 + configuração rede ITAL + correção nao recebimento de pacote NTP
*/

/*LIMITAÇÕES:
   1-Nao considera horario de verao
   2-Requisita o horario ao servidor NTP a cada envio de dados de temperatura
*/

/*
 * Referências:
 * http://fabianoallex.blogspot.com.br/2015/08/arduino-ntp-acessando-servidor-de-hora.html
 * Stackoverflow
 */
//---ETHERNET----//
#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {0x90, 0x02, 0xDA, 0x00, 0x23, 0x37};
//IP 192.168.1.97
IPAddress ip(192, 168, 33, 100);       //Define o endereco IP
IPAddress gateway(192, 168, 32, 1);  //Define o gateway
IPAddress subnet(255, 255, 252, 0); //Define a máscara de rede
IPAddress dns2(201, 55, 40, 34);

/*DADOS REDE LSERF CSS PONTO 3

  Segue IP : 143.106.8.164

  gateway: 143.106.8.129

  máscara de rede: 255.255.255.192

  DNS primário: 143.106.8.30

  DNS secundário: 143.106.8.29
*/

/*DADOS REDE ITAL
  IP: 192.168.33.100

  Mask: 255.255.252.0

  Gateway: 192.168.32.1

  DNS: 201.55.40.34

  Eder de Souza

*/

// change to your server
IPAddress server(143, 106, 186, 7); //lalt: 143.106.186.7

//nome do domínio para servidores virtuais
char serverName[] = "lalt.fec.unicamp.br";
// se nao existir nome de dominio, utilizar o endereço IP abaixo:
// char serverName[] = "74.125.227.16";

// porta do servidor
int serverPort = 80;

EthernetClient client;
int totalCount = 0;

char pageAdd[64]; //variavel string para envio de dados pro servidor

// delay em milisegundos
#define delayMillis 300000UL  //padrao CIVIL: 300000

unsigned long thisMillis = 0;
unsigned long lastMillis = 0;
unsigned long difMillis;

//----ETHERNET UDP-------------------//
#include <EthernetUdp.h>

unsigned int localPort = 8888;        // local port to listen for UDP packets
char timeServer[] = "129.6.15.30";  //endereço do servidor NTP
                                      // original: time.nist.gov NTP server

/*SERVIDORES NTP NIST
 * http://tf.nist.gov/tf-cgi/servers.cgi
 * 
 * time-c.nist.gov 129.6.15.30  (servidor com IP liberado no ITAL)
 */

//servidor NTP brasileiro: http://www.pool.ntp.org/pt/use.html

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

//---SENSOR------------------------------------//
//BIBLIOTECAS SENSOR
#include <OneWire.h>
#include <DallasTemperature.h>

// Porta do pino digital de sinal do DS18B20
#define ONE_WIRE_BUS 3
#define TEMPERATURE_PRECISION 9 // 9 bits = 0,5 ºC . Resolução padrão inicializada: 12

// Define uma instancia do oneWire para comunicacao com o sensor/*-----( Declare objects )-----*/
OneWire oneWire(ONE_WIRE_BUS);    // Create a 1-wire object

DallasTemperature sensors(&oneWire);
DeviceAddress sensorA; //sensor A
DeviceAddress sensorB; //sensor B

//-----BIBLIOTECAS e definicoes Fabiano -----
#include "Dns.h"

#define LEAP_YEAR(_year) ((_year%4)==0)
static  byte monthDays[] = {31, 28, 31, 30 , 31, 30, 31, 31, 30, 31, 30, 31};

void setup() {

  Serial.begin(9600);

  setupServidor();

  setupSensor(&sensorA, &sensorB);
}


float tempA, tempB; //armazenamento de temperaturas

unsigned long epochReceived;  //recebimento do Unix Time
unsigned long epochReceived2;

void loop() {

  //Formato da string enviada para o Servidor LALT
  //formato: 9|99999999999999|9.99
  //[id do dispositivo | data e hora no formato ddmmyyyyhhiiss | valor da temperatura ]

  //----- ENVIO SERVIDOR LALT----------//

  Ethernet.maintain();

  thisMillis = millis();

  //diferença de tempo MAIOR ou IGUAL
  if (thisMillis - lastMillis >= delayMillis)
  {
    difMillis = (thisMillis - lastMillis)/1000; // conversao de mili segundos para segundos

    lastMillis = thisMillis;

    requisitaTemperatura(&sensorA, &sensorB, &tempA, &tempB);

    epochReceived2 = epochReceived;

    epochReceived = requisitaHorario();

    //se houve erro no envio do NTP server, o epochReceived não altera seu valor
    //logo, a correção é feita manualmente
    if (epochReceived == epochReceived2) {
      epochReceived = epochReceived + difMillis;
    }

    byte ano, mes, dia, dia_semana, hora, minuto, segundo;
    localTime(&epochReceived, &segundo, &minuto, &hora, &dia, &dia_semana, &mes, &ano); //extrai data e hora do unix time

    sprintf(pageAdd, "/tkr/server/?data=0|%02i%02i%i%02i%02i%02i|%d.%02d;1|%02d%02d%d%02i%02i%02i|%d.%02d", dia, mes + 1, ano + 1900, hora
            , minuto, segundo, (int)tempA, (int)(tempA * 100) % 100, dia, mes + 1, ano + 1900, hora, minuto, segundo, (int)tempB, (int)(tempB * 100) % 100);
    // 20 caracteres de dados para 1 sensor
    // 18 caracteres alem dos dados

    // funcao sprintf nao aceita float (decisao dos desenvolvedores pra economizar memoria)
    // outra tecnica pra converter float em string "dtostrf"

    if (!getPage(server, serverPort, pageAdd)) Serial.print(F("Fail "));
    else Serial.print(F("Pass "));

    totalCount++;

    Serial.println(pageAdd);
  }//if maior que delay millis

}//loop

byte getPage(IPAddress ipBuf, int thisPort, char *page)
{
  int inChar;
  char outBuf[128];

  Serial.print(F("connecting..."));

  if (client.connect(ipBuf, thisPort) == 1)
  {
    Serial.println(F("connected"));

    sprintf(outBuf, "GET %s HTTP/1.1", page);
    client.println(outBuf);
    sprintf(outBuf, "Host: %s", serverName);
    client.println(outBuf);
    client.println(F("Connection: close\r\n"));
  }
  else
  {
    Serial.println(F("failed"));
    return 0;
  }

  // connectLoop controls the hardware fail timeout
  int connectLoop = 0;

  while (client.connected())
  {
    while (client.available())
    {
      inChar = client.read();
      Serial.write(inChar);
      // set connectLoop to zero if a packet arrives
      connectLoop = 0;
    }

    connectLoop++;

    // if more than 10000 milliseconds since the last packet
    if (connectLoop > 10000)
    {
      // then close the connection from this end.
      Serial.println();
      Serial.println(F("Timeout"));
      client.stop();
    }
    // this is a delay for the connectLoop timing
    delay(1);
  }

  Serial.println();

  Serial.println(F("disconnecting."));
  // close client end
  client.stop();

  return 1;
}

void setupSensor(DeviceAddress *sensor1, DeviceAddress *sensor2) {
  //------SENSOR------------//

  sensors.begin();
  // Localiza e mostra enderecos dos sensores
  Serial.println("Localizando sensores DS18B20...");
  Serial.print("Foram encontrados ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" sensores.");
  if (!sensors.getAddress(*sensor1, 1))
    Serial.println("Sensores nao encontrados !");

  if (!sensors.getAddress(*sensor2, 0))
    Serial.println("Sensores nao encontrados !");

  // Mostra o endereco do sensor encontrado no barramento
  Serial.print("Endereco sensor A: ");
  mostra_endereco_sensor(*sensor1);

  Serial.println();

  Serial.print("Endereco sensor B: ");
  mostra_endereco_sensor(*sensor2);

  Serial.println();

  // set the resolution to x bit per device
  sensors.setResolution(*sensor1, TEMPERATURE_PRECISION);
  sensors.setResolution(*sensor2, TEMPERATURE_PRECISION);
  // Endereço DS18B20 A (1) = 28 FF E3 41 62 16 03 FD
  // Endereço DS18B20 B (2) = 28 FF 94 27 62 16 03 58

  //verificando resolução padrão
  Serial.print("Sensor A Resolution: ");
  Serial.print(sensors.getResolution(*sensor1), DEC);
  Serial.println();

  Serial.print("Sensor B Resolution: ");
  Serial.print(sensors.getResolution(*sensor2), DEC);
  Serial.println();

}

void mostra_endereco_sensor(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // Adiciona zeros se necessário
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void requisitaTemperatura(DeviceAddress *sensor1, DeviceAddress *sensor2, float *tempC1, float *tempC2) {

  //------SENSOR---------------------------//
  // Le a informacao do sensor
  sensors.requestTemperatures();
  *tempC1 = sensors.getTempC(*sensor1);

  sensors.requestTemperatures();
  *tempC2 = sensors.getTempC(*sensor2);

  // Mostra dados no serial monitor
  Serial.print("Sensor A: ");
  Serial.print(*tempC1);
  Serial.println(" C");


  Serial.print("Sensor B: ");
  Serial.print(*tempC2);
  Serial.println(" C");
}

void setupServidor() {
  //----ENVIO SERVIDOR LALT-------//
  // disable SD SPI
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  // Start ethernet
  Serial.println(F("Starting ethernet..."));
  Ethernet.begin(mac, ip, dns2, gateway, subnet);

  // If using dhcp, comment out the line above
  // and uncomment the next 2 lines plus the Ethernet.maintain call in loop

  //  if (!Ethernet.begin(mac)) Serial.println(F("failed"));
  //  else Serial.println(F("ok"));

  Serial.println(Ethernet.localIP());

  delay(2000);
  Serial.println(F("Ready"));

  //-----SERVIDOR UDP
  //inicializa conexão servidor
  //  server.begin();
  // Serial.println("ANTES UDP BEGIN OK");
  Udp.begin(localPort);
  // Serial.println("SETUP END OK");

}

unsigned long requisitaHorario() {

  //-----RTC NTP Server-------------------------//
  //  Serial.println("LOOP BEGIN OK");
  sendNTPpacket(timeServer); // send an NTP packet to a time server

  // wait to see if a reply is available. valor padrao 1000ms
  delay(1000);

  if ( Udp.parsePacket() ) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //    Serial.print("Seconds since Jan 1 1900 = " );
    //   Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);

    //HORARIO BRASILIA
    Serial.print("UTC -3 time = ");
    epoch = epoch - 3 * 3600;
    Serial.println(epoch);

    return epoch;
  }
}


// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(char* address)
{
  //  Serial.println("FUNCTION sendNTPpacket Begin OK");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //  Serial.println("BEGIN PACKET OK");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  //  Serial.println("FUNCTION sendNTPpacket End OK");
}

// funcao fabiano
void localTime(unsigned long *timep, byte *psec, byte *pmin, byte *phour, byte *pday, byte *pwday, byte *pmonth, byte *pyear) {
  unsigned long long epoch = * timep;
  byte year;
  byte month, monthLength;
  unsigned long days;

  *psec  =  epoch % 60;
  epoch  /= 60; // now it is minutes
  *pmin  =  epoch % 60;
  epoch  /= 60; // now it is hours
  *phour =  epoch % 24;
  epoch  /= 24; // now it is days
  *pwday =  (epoch + 4) % 7;

  year = 70;
  days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= epoch) {
    year++;
  }
  *pyear = year; // *pyear is returned as years from 1900

  days  -= LEAP_YEAR(year) ? 366 : 365;
  epoch -= days; // now it is days in this year, starting at 0

  for (month = 0; month < 12; month++) {
    monthLength = ( (month == 1) && LEAP_YEAR(year) ) ? 29 : monthDays[month]; // month==1 -> february
    if (epoch >= monthLength) {
      epoch -= monthLength;
    } else {
      break;
    }
  }

  *pmonth = month;  // jan is month 0
  *pday   = epoch + 1; // day of month
}
