/*
   Autor: André Massa
   Data: 2017
   FUNCIONALIDADES: Codigo do Coordenador
   1-XBee coordinator recebe umidade e temperatura do end-vice
   2-ESP8266 envia como cliente os dados para ThingSpeak
*/

/**
   Referência da biblioteca usada
   \file
         ESP8266 RESTful Bridge example
   \author
         Tuan PM <tuanpm@live.com>
   fonte: https://github.com/tuanpmt/espduino
*/
/*
   ATENCAO
   1-Baudrate fixo de 19200 no firmware do Taun PM
   2-Comunicação arduino-ESP feita pelo TX-RX, logo não pode usar
     Serial.print, pois polui o Tx-Rx e não funciona!
*/

/*
   CODIGO XBEE FUNCIONA
   -Falta verificar por que o check sum nao bate
   -Existe 1 byte (ou 2) estranhos. O primeiro do endereço e antes
   -Verificar o check sum recebido!!!!
*/

//!!!!!!!!!! FUNCIONA !!!!!!!!!!

//----------Bibliotecas---------------------//
#include <SoftwareSerial.h>
#include <espduino.h>
#include <rest.h>

//----------Definição MACROS--------------------//
#define LENGTH 2
#define NB_BYTE_SOURCE_ADD 8
#define NB_BYTE_NETWORK_ADD 2
#define NB_BYTE_DATA 2
#define START_BYTE 0x7E
#define BUFFER_SIZE 25  //tamanho do pacote ZigBee Receive Packet

//#define ESPERA_CONECTAR 5000

//----------Variaveis XBee--------------------//
union length2B {
  unsigned char bt [2]; //2 bytes
  unsigned short int data; // 2 bytes
};

union length4B {
  unsigned char bt [4]; //4 bytes
  unsigned long int data; // 4 bytes
};

//ler temperatura e umidade
union u_tag {
  uint8_t b[4];
  float fval;
} uH, uT;

union length2B comprimento;
byte startByte;
byte frameType;
byte checkSum;
union length4B sourceAddressHigh; //limitação do Arduino UNO
union length4B sourceAddressLow;
union length2B networkAdd;
byte receiveOpt;

byte v1;

//Variaveis gerais
int i, qtdAvailable;

float umidade, temperatura;

int endDevice=999;

unsigned long cont = 0;

//----------------------------------------------//

SoftwareSerial debugPort(2, 3); // RX, TX

SoftwareSerial SSRxTx(5, 6); //(Rx,Tx)

ESP esp(&Serial, &debugPort, 4); //O QUE EH O 4?????

REST rest(&esp);

boolean wifiConnected = false;

boolean pacoteCorreto = false;

void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if (res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if (status == STATION_GOT_IP) {
      debugPort.println("WIFI CONNECTED");

      wifiConnected = true;
    } else {
      wifiConnected = false;
    }

  }
}

void setup() {
  Serial.begin(19200);
  debugPort.begin(19200);

  SSRxTx.begin(9600);

  esp.enable();
  delay(500);

  esp.reset();
  delay(500);

  while (!esp.ready());

  debugPort.println("ARDUINO: setup rest client");
  if (!rest.begin("api.thingspeak.com")) {
    Serial.println("ARDUINO FAILED SETUP");
    debugPort.println("ARDUINO: failed to setup rest client");
    while (1);
  }

  /*setup wifi*/
  debugPort.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);

  esp.wifiConnect("LSERF", "lserflab25x"); //insere (SSID, WifiPassword)
  // Rede Celular: ("WiFi-Massa", "msfa0921")
  // Rede Lab: ("LSERF", "lserflab25x")
  debugPort.println("ARDUINO: system started");

  debugPort.println("*** END BEGIN ***");
}


void loop() {

  char response[266];
  esp.process();

  if (wifiConnected) {

    qtdAvailable = SSRxTx.available();

    if (qtdAvailable >= BUFFER_SIZE) { //sempres entra no loop

      debugPort.print("Recebimento #");
      debugPort.println(cont++);

      pacoteCorreto = recebeFrame();

      if (pacoteCorreto) { //pacote dentro dos conformes

        char buff[64];
        char str_hum[6], str_temp[6];

        dtostrf(umidade, 4, 2, str_hum); //converte o valor double (val) em ASCII (s)(val,width, prec, s)
        dtostrf(temperatura, 4, 2, str_temp);

        //seleciona o campo para onde enviar os dados
        switch (endDevice) {
          
          //key do DaLuz= 110UYAPC7KQ3WQ2Q
          //Write key Massa= RDY31KYV5MHYWY6K
          
          case 2: //E2
            sprintf(buff, "/update?api_key=RDY31KYV5MHYWY6K&field1=%s&field2=%s", str_hum, str_temp);
            break;
          case 3: //E3
            sprintf(buff, "/update?api_key=RDY31KYV5MHYWY6K&field3=%s&field4=%s", str_hum, str_temp);
            break;
          case 4: //E4
            sprintf(buff, "/update?api_key=RDY31KYV5MHYWY6K&field5=%s&field6=%s", str_hum, str_temp);
            break;
        }

        debugPort.println(buff);
        rest.get((const char*)buff);
        debugPort.println("ARDUINO: send get");

        if (rest.getResponse(response, 266) == HTTP_STATUS_OK) {
          debugPort.print("ARDUINO: GET successful - ");
          debugPort.println(response);
        }
        else {
          debugPort.println("ARDUINO: GET not successful");
        }

        pacoteCorreto = false;

        debugPort.println("--");
        //      delay(1000);//tempo espera padrao: 30000 ms
      }
    } else {
      //  debugPort.print("error,\r\n");
    }

  }//if wifiConnected

}// if loop

/********************* SUB ROTINAS ********************/

boolean recebeFrame() {

  //RECEBENDO E REGISTRANDO DADOS DO FRAME

  //start byte
  startByte = SSRxTx.read();

  if (startByte == START_BYTE) {

    //Lê tamanho conteúdo do frame
    for (i = LENGTH - 1; i >= 0; i--) {
      comprimento.bt [i] = SSRxTx.read();
    }

    frameType = SSRxTx.read();

    v1 = SSRxTx.read(); //O QUE EH ESSE DADO? Endereco destino end device?

    //The frame type is 0x90 (ZigBee Receive Packet)
    if (frameType == 0x90) {
      debugPort.println("FRAME_CORRETO");

      processo();

      debugPort.print("Umidade: ");
      debugPort.println(umidade);
      debugPort.print("Temperatura: ");
      debugPort.println(temperatura);

      return true;
    }
    else {
      debugPort.println("FrameType diferente");
      return false;
    }
  }//if start byte
  else {
    debugPort.println("StartByte diferente");
    return false;
  }

}

void processo() {

  //Source Address quebrado em dois
  for (i = NB_BYTE_SOURCE_ADD / 2 - 1; i >= 0 ; i--) {
    sourceAddressHigh.bt [i] = SSRxTx.read();
  }

  for (i = NB_BYTE_SOURCE_ADD / 2 - 1; i >= 0 ; i--) {
    sourceAddressLow.bt [i] = SSRxTx.read();
  }

  //Network address
  for (i = NB_BYTE_NETWORK_ADD - 1; i >= 0 ; i--) {
    networkAdd.bt [i] = SSRxTx.read();
  }

  receiveOpt = SSRxTx.read();

  //salva umidade
  for (i = 0; i < 4; i++) {
    uH.b[i] = SSRxTx.read();
  }
  umidade = uH.fval;

  //salva temperatura
  for (i = 0; i < 4; i++) {
    uT.b[i] = SSRxTx.read();
  }
  temperatura = uT.fval;

  checkSum = SSRxTx.read();

  verificaEndDevice();
}

//INFORMACOES XBEEs
//Product family: XBP24-ZB

// PAN ID: F0
//endereço C1 API: 0013A200 40683FE2
//endereço C2 API: 0013A200 404960B8
//endereço E2: 0013A200 40683F8D  (DH=DL=0) OK
//Endereco E3: 0013A200 4089CB93  (DH=DL=0) OK
//Endereço E4: 0013A200 40683F74  (DH=DL=0) OK

//END DEVICE -> COODRDINATOR DH e DL = 0

//Pacote recebido de 25 bytes

//CONFIGURACOES PADRAO
//IR IO Sampling Rate 1000 ms

void verificaEndDevice() { //DISCRIMINA O NOME DO NÓ END
  switch (sourceAddressLow.data) {

    case 0x4089CB96:
      debugPort.println("E1");
      endDevice = 1;
      break;
    case 0x40683F8D:
      debugPort.println("E2"); //usado
      endDevice = 2;
      break;
    case 0x4089CB93:
      debugPort.println("E3"); //usado
      endDevice = 3;
      break;
    case 0x40683F74:
      debugPort.println("E4"); //usado
      endDevice = 4;
      break;
    case 0x40645560:
      debugPort.println("E5");
      endDevice = 5;
      break;

    default:
      debugPort.println("E?");
      endDevice = 0;
      break;
  }
}
