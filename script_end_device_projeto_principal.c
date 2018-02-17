/*
 * Autor: André Massa
 * Data: 2017
 * Função: Script para envio de dados de temperatura e umidade do DHT, utilizando processamento
 * do arduino em modo low power e envio pelo XBee. O DHT é ligado a cada periodo de leitura e depois desligado
 * e o XBee é acordado utilizando o pino corresponde pelo arduino, quando este acorda.
 * 
 * Referências:
 * https://learn.sparkfun.com/tutorials/reducing-arduino-power-consumption
 * http://streylab.com/blog/2012/10/14/sending-humidity-and-temperature-data-with-zigbee.html
 */
 
/*
   Versão 4.5
   -sem led 13
   -ligamento do DHT22 por pino digital: corrente DHT22 = 1,5mA e max pino IO = 40 mA
   -xbee end device configurado como Sleep Mode: Pin wake mode, ATSM 1
   -introdução de delay proprio: myDelay
*/

//*****************FUNCIONA!!************

#include "LowPower.h"
#include "XBee.h"
#include "DHT.h"

#define DHTPIN 2     // data pin of the DHT sensor

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

/*
 * DHT11 tem resistor de pull up de 5.1k, tensao 3.5V< Vcc <5.5V, intervalo de medidas de 5 segundos.
 */

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);

/*
  This example is for Series 2 XBee
  Sends a ZB TX request with the value of analogRead(pin5) and checks the status response for success
*/

// create the XBee object
XBee xbee = XBee();

// we are going to send two floats of 4 bytes each
uint8_t payload[8] = { 0, 0, 0, 0, 0, 0, 0, 0};

// union to convery float to byte string
union u_tag {
  uint8_t b[4];
  float fval;
} u;


// SH + SL Address of receiving XBee
XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x40683FE2);
ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

//INFORMACOES XBEEs
//Product family: XBP24-ZB

// PAN ID: F0
//endereço C1 API: 0013A200 40683FE2
//endereço C2 API: 0013A200 404960B8
//endereço E2: 0013A200 40683F8D  (DH=DL=0) OK
//Endereco E3: 0013A200 4089CB93  (DH=DL=0) OK
//Endereço E4: 0013A200 40683F74  (DH=DL=0) OK

//END DEVICE -> COODRDINATOR DH e DL = 0

//Pacote enviado de 25 bytes

//CONFIGURACOES PADRAO
//IR IO Sampling Rate 1000 ms

//-------------------------------------------

int pinPowerDHT = 5;     //pino que alimenta o DHT
int pinXbeeWakeMode = 6;  //pino que acorda/dorme o XBee
int pinReadXbeeStatus = 7;  //verifica se o Xbee esta dormindo ou acordado
int pinReadXbeeCTS = 8; //verifica pino 12 de fluxo CTS

int xbeeSleepStatus = 0;
int xbeeCTS = 0;


/* Instruções Pin Wake Mode:
   -pino 9 do xbee para sleep em HIGH (3V3) para ativar modo SLEEP
   -modo sleep consome menos de 10uA
   -Xbee demora 13ms para acordar
*/

/*
   https://www.guiduc.org/wp/2012/01/trouble-with-xbee-serie-2-sleep-mode/
    With the default values (SN=1 and SP=0x20), if the end device sleeps for more
    than 320 ms (SN * SP * 10 ms), its parent device will forget it and, when it wakes up,
    it will have to perform the long association process (which takes several seconds to complete).
    If the delay is increased (by increasing SN and SP) to a value longer than the maximum sleep
    period of the end device, the parent device will keep the information about its son and, when
    it wakes up, the association is much faster (less than a second).

  It now works fine!
*/

/*
   To wake a sleeping module operating in Pin Hibernate Mode, de-assert Sleep_RQ (pin 9).
   The module will wake when Sleep_RQ is de-asserted and is ready to transmit or receive when
   the CTS line is low. When waking the module, the pin must be de-asserted at least two 'byte times'
   after CTS goes low. This assures that there is time for the data to enter the DI buffer.
*/

void setup()
{

  Serial.begin(9600);

  pinMode(pinPowerDHT, OUTPUT); //alimentar DHT
  pinMode(pinXbeeWakeMode, OUTPUT); //dormir/acordar Xbee
  pinMode(pinReadXbeeStatus, INPUT);
  pinMode(pinReadXbeeCTS, INPUT);

  pinMode(13, OUTPUT); //led teste

  dht.begin();
  xbee.begin(Serial);

  digitalWrite(pinXbeeWakeMode, HIGH); //Pin 9 HIGH = dormir Xbee
  //Pin 9 LOW = acordar Xbee
  Serial.println(" Begin End device");

}

int k = 1;

void loop() {

  Serial.print("k: ");
  Serial.println(k);

  xbeeSleepStatus = digitalRead(pinReadXbeeStatus);

  // Serial.print("Xbee Status 1(LOW=dormindo): ");
  Serial.println(xbeeSleepStatus);  //pin 13 LOW = dormindo

  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  // 37,5 * 8 segundos = 5 min
  if (k == 2) {

    leituraTempUmid();
    enviarDadosXbee();
    k = 1;
    Serial.println("-"); //recomeca
  }
  else {
    k++;
  }

}

void leituraTempUmid() {

  digitalWrite(pinPowerDHT, HIGH); //liga DHT
  myDelay(2000);// dht precisa de mais 2 segundos para energizar e fazer a leitura correta (4s total)
  myDelay(2000);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (!isnan(t) && !isnan(h)) {

    // convert humidity into a byte array and copy it into the payload array
    u.fval = h;

    Serial.print("H: ");
    Serial.print(u.fval);
    for (int i = 0; i < 4; i++) {
      payload[i] = u.b[i];
      Serial.print(" ");
      Serial.print(payload[i], HEX);
    }

    // same for the temperature
    u.fval = t;
    Serial.println();
    Serial.print("T: ");
    Serial.print(u.fval);
    for (int i = 0; i < 4; i++) {
      payload[i + 4] = u.b[i];
      Serial.print(" ");
      Serial.print(payload[i + 4], HEX);
    }
    Serial.println();
    digitalWrite(pinPowerDHT, LOW); //Desliga DHT
  }
}

void enviarDadosXbee() {

  Serial.print (" Wake=1: ");
  digitalWrite(pinXbeeWakeMode, LOW); //pin 9 LOW = acordar Xbee
  myDelay(13); //Xbee leva 13ms para acordar em modo Pin Hibernate (pin wake mode)

  xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
  Serial.println(xbeeSleepStatus); //pin 13 HIGH 3V3 = acordado

  Serial.print (" CTS=0: ");
  xbeeCTS = digitalRead(pinReadXbeeCTS);
  Serial.println(xbeeCTS); //pin 12 xbee CTS == 0 > pronto pra fluxo

  // if (xbeeSleepStatus == 1 && xbeeCTS == 0) {
  xbee.send(zbTx); //envia dados

  /* Serial.print (" Wake=1-send: ");
    xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
    Serial.println(xbeeSleepStatus); //pin 13 HIGH 3V3 = acordado
  */
  // after sending a tx request, we expect a status response
  // wait up to half second for the status response
  if (xbee.readPacket(500)) {

    /*Serial.print (" Wake=1-readPacket: ");
      xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
      Serial.println(xbeeSleepStatus); //pin 13 HIGH 3V3 = acordado
    */
    // got a response!

    // should be a znet tx status
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);

      //    Serial.print (" Wake=1-getResponse: ");
      //    xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
      //      Serial.println(); //pin 13 HIGH 3V3 = acordado

      // get the delivery status, the fifth byte
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        // success.  time to celebrate
        Serial.println(" SUCESSO");

      } else {
        // the remote XBee did not receive our packet. is it powered on?
        Serial.println(" NAO_RECEBIDO");
      }
    }
  } else if (xbee.getResponse().isError()) {

    Serial.print (" Wake=1-getResponseError1: ");
    xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
    Serial.println(xbeeSleepStatus); //pin 13 HIGH 3V3 = acordado
    //nss.print("Error reading packet.  Error code: ");
    //nss.println(xbee.getResponse().getErrorCode());
    Serial.println(" Erro1");
  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    Serial.println(" Erro2");

    Serial.print (" Wake=1-getResponseError2: ");
    xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
    Serial.println(xbeeSleepStatus); //pin 13 HIGH 3V3 = acordado
  }
  // }if

  Serial.print("CTS=0: ");
  xbeeCTS = digitalRead(pinReadXbeeCTS);
  Serial.println(xbeeCTS); //pin 12 xbee CTS == 0 > pronto pra fluxo

  //  myDelay(13); //Xbee leva 13ms para acordar em modo Pin Hibernate (pin wake mode)
  digitalWrite(pinXbeeWakeMode, HIGH); //pin 9 HIGH = dormir Xbee
  //tentar forçar que o XB durma nao adianta, pois ele fica acordado ate enviar/receber os dados

  myDelay(13); //DELAY IMPORTANTE PARA FUNCIONAMENTO
  Serial.print(" Sleep=0: ");  //pin 13 LOW = dormindo

  xbeeSleepStatus = digitalRead(pinReadXbeeStatus);
  Serial.println(xbeeSleepStatus); //pin 13 LOW = dormindo

  Serial.print("CTS=1: ");
  xbeeCTS = digitalRead(pinReadXbeeCTS);
  Serial.println(xbeeCTS); //pin 12 xbee CTS == 0 > pronto pra fluxo

}

void myDelay(unsigned long espera) {
  unsigned long previousMillis = millis();
  while (millis() - previousMillis < espera) {
  }
}

/*
   The use of delay() in a sketch has significant drawbacks.
   No other reading of sensors, mathematical calculations, or pin
   manipulation can go on during the delay function, so in effect,
   it brings most other activity to a halt.
*/
