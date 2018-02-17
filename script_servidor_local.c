/*
   Autor: André MASSA
   Data: 2017
   Função: Recebe valores das entradas analógicas do 
   Xbee e apresenta no browser
   usando rede local e shield ethernet do arduino
*/


//Bibliotecas
#include <SPI.h>
#include <Ethernet.h>

//Definição MACROS
#define LENGTH 2
#define NB_BYTE_SOURCE_ADD 8
#define NB_BYTE_NETWORK_ADD 2
#define NB_BYTE_DATA 2
#define START_BYTE 0x7E
#define BUFFER_SIZE 28
#define AMOSTRAGEM 1023
#define TENSAO_MAX 1.2
#define NB_ANALOG_IN 4

//Variaveis de leitura
union length2B {
  unsigned char bt [2]; //2 bytes
  unsigned short int data; // 2 bytes
};

union length4B {
  unsigned char bt [4]; //4 bytes
  unsigned long int data; // 4 bytes
};

union length2B comprimento;
byte startByte;
byte frameType;
byte checkSum;
union length4B sourceAddressHigh; //limitação do Arduino UNO
union length4B sourceAddressLow;
union length2B networkAdd;
byte receiveOpt;
byte nbSamples;
union length2B analogSample[NB_ANALOG_IN];
byte analogChanMask;
union length2B digChanMask;

//Variaveis gerais
int verifCheckSum;
int i, j;
float volts[NB_ANALOG_IN];
char bufferMessage[50];
int nbAnalogIn;
int serialAvailable;
unsigned long qtdPacotes = 0;
unsigned long qtdAcertos = 0;
float taxaAcerto = 0;

//Tempo
unsigned long tempo;

int analogIn0,analogIn1,analogIn2,analogIn3; 
//le os valores absolutos das tensões dos XBees

//*******************************************

//Inicialização de variaveis da Rede
byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02}; 
                                                          //endereço mac aleatorio
IPAddress ip(143, 106, 8, 164);       //Define o endereco IP
IPAddress gateway(143, 106, 8, 129);  //Define o gateway
IPAddress subnet(255, 255, 255, 192); //Define a máscara de rede

//ip: 192.168.0.2

/*DADOS REDE LSERF CSS PONTO 3

  Segue IP : 143.106.8.164

  gateway: 143.106.8.129

  máscara de rede: 255.255.255.192

  DNS primário: 143.106.8.30

  DNS secundário: 143.106.8.29
*/

/* DADOS ANTERIORES DE REDE DO LAB DE BAIXO

   IPAddress ip(192, 168, 0, 2);       //Define o endereco IP
  IPAddress gateway(192, 168, 1, 1);  //Define o gateway
  IPAddress subnet(255, 255, 255, 0); //Define a máscara de rede
*/

//Inicializa o servidor web na porta 80 padrao HTTP
EthernetServer server(80);

void setup() {
  // put your setup code here, to run once:

  //inicializa conexão Ethernet e o servidor
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();

  Serial.begin(9600);

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("loop");

  recebePacote(); //função que recebe o frame e classifica seus bytes

  //escuta clientes de entrada
  EthernetClient client = server.available();

  // if client <> 0
  if (client) {

    Serial.println("new client");

    //código padrao
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        //        Serial.print("c: ");
        //        Serial.println(c);

        //pedido http acabou,entao manda resposta
        if (c == 'n' && currentLineIsBlank) {
          //envia resposta padrão
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println("Refresh: 2"); //Recarrega a pagina a cada 2seg
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          //Configura o texto e imprime o titulo no browser
          client.print("<font color=#FF0000><b><u>");
          client.print("Envio de informacoes pela rede utilizando Arduino (Refresh de 2s)");
          client.print("</u></b></font>");
          client.println("<br />");
          //          client.println("<br />");

          //Mostra o estado da porta digital 3
          int porta_digital = digitalRead(3);
          //        client.print("Porta Digital 3 : ");
          //        client.print("<b>");
          //        client.print(porta_digital);
          //        client.println("</b>");
          //        client.print("  (0 = Desligada, 1 = Ligada)");
          //        client.println("<br />");

          //Mostra as informacoes lidas pelos sensores
          client.print("Sensor 1: ");
          client.print("<b>");
          client.print(analogIn0);
          client.print(" uni");
          client.println("</b>");
          client.println("<br />");

          client.print("Sensor 2: ");
          client.print("<b>");
          client.print(analogIn1);
          client.print(" uni");
          client.println("</b>");
          client.println("<br />");

          client.print("Sensor 3: ");
          client.print("<b>");
          client.print(analogIn2);
          client.print(" uni");
          client.println("</b>");
          client.println("<br />");

          client.print("Sensor 4: ");
          client.print("<b>");
          client.print(analogIn3);
          client.print(" uni");
          client.println("</b>");

          client.println("</html>");
          break;
        }
        if (c == 'n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != 'r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("");
  }
}

void recebePacote(){

  //buffer com API frame completo. Buffer maximo de 64 bytes (do arduino)
  serialAvailable = Serial.available();
  if (serialAvailable >= BUFFER_SIZE) {
    Serial.println("Ser Av");

    //RECEBENDO E REGISTRANDO DADOS DO FRAME
    //start byte
    startByte = Serial.read();
    if (startByte == START_BYTE) {
      Serial.println("Sta Byt");

      verifCheckSum = 0;
      nbAnalogIn = 0;

      //Lê tamanho conteúdo do frame
      for (i = LENGTH - 1; i >= 0; i--) {
        comprimento.bt [i] = Serial.read();
      }

      frameType = Serial.read();

      //The frame type is 0x92 (IO Data Sample Rx Indicator)
      if (frameType == 0x92) {

        qtdPacotes++;
        //Source Address quebrado em dois
        for (i = NB_BYTE_SOURCE_ADD / 2 - 1; i >= 0 ; i--) {
          sourceAddressHigh.bt [i] = Serial.read();
        }

        for (i = NB_BYTE_SOURCE_ADD / 2 - 1; i >= 0 ; i--) {
          sourceAddressLow.bt [i] = Serial.read();
        }

        for (i = NB_BYTE_NETWORK_ADD - 1; i >= 0 ; i--) {
          networkAdd.bt [i] = Serial.read();
        }

        receiveOpt = Serial.read();
        nbSamples = Serial.read();

        for (i = 1; i >= 0 ; i--) {
          digChanMask.bt[i] = Serial.read();
        }

        analogChanMask = Serial.read();

        //Calcular a quantidade de entradas analógicas ativas

        if ((analogChanMask & B0001) == B0001) {
          nbAnalogIn++;
        }
        if ((analogChanMask & B0010) == B0010) {
          nbAnalogIn++;
        }
        if ((analogChanMask & B0100) == B0100) {
          nbAnalogIn++;
        }
        if ((analogChanMask & B1000) == B1000) {
          nbAnalogIn++;
        }


        //Obs: Não tem digital samples

        for (j = 0; j < nbAnalogIn; j++ ) {
          for (i = NB_BYTE_DATA - 1; i >= 0 ; i--) {
            analogSample[j].bt [i] = Serial.read();
          }
        }

        checkSum = Serial.read();

        //SOMANDO BYTES DO FRAME PARA VERIFICAÇÂO, 
        //FrameType até AnalogSample

        verifCheckSum += frameType;

        for (i = NB_BYTE_SOURCE_ADD / 2 - 1; i >= 0 ; i--) {
          verifCheckSum += sourceAddressHigh.bt [i];
        }

        for (i = NB_BYTE_SOURCE_ADD / 2 - 1; i >= 0 ; i--) {
          verifCheckSum += sourceAddressLow.bt [i];
        }

        for (i = NB_BYTE_NETWORK_ADD - 1; i >= 0 ; i--) {
          verifCheckSum += networkAdd.bt [i];
        }

        verifCheckSum += receiveOpt;

        verifCheckSum += nbSamples;

        for (i = 1; i >= 0 ; i--) {
          verifCheckSum += digChanMask.bt[i];
        }

        verifCheckSum += analogChanMask;

        for (j = 0; j < nbAnalogIn; j++ ) {
          for (i = NB_BYTE_DATA - 1; i >= 0 ; i--) {
            verifCheckSum += analogSample[j].bt [i];
          }
        }
        verifCheckSum = verifCheckSum & B11111111;

        verifCheckSum = 0xFF - verifCheckSum;

        //VERIFICANDO COMPATIBILIDADE CHECKSUM

        if (verifCheckSum == checkSum) {
          qtdAcertos++;
        } else {
          Serial.println("CK S ERRADO");
        }

        //CALCULO e PRINT DA TENSAO em V

        for (j = 0; j < nbAnalogIn; j++) {
          volts[j] = (analogSample[j].data * TENSAO_MAX) / AMOSTRAGEM;
          sprintf(bufferMessage, "T%i V: ", j); //sprintf nao suporta float
          Serial.print(bufferMessage);
          Serial.println(volts[j]);

        }

        //Print taxa de acertos do checkSum
        taxaAcerto = (100 * qtdAcertos) / qtdPacotes;
        Serial.print("TA %: ");
        Serial.println(taxaAcerto);

        //TEMPO
        Serial.print("TIME ms: ");
        tempo = millis(); //da o tempo em milisegundos desde o começo do programa
        Serial.println(tempo);

        //DISCRIMINA O NOME DO NÓ END
        //Considera apenas a entrada analogica zero de cada XBee
        switch (sourceAddressLow.data) {

          case 0x4089CB96:
            Serial.println("E1");
             analogIn0 = analogSample[0].data;
            break;
          case 0x40683F8D:
            Serial.println("E2");
            analogIn1 = analogSample[0].data;
            break;
          case 0x4089CB93:
            Serial.println("E3");
            analogIn2 = analogSample[0].data;
            break;
          case 0x40683F74:
            Serial.println("E4");
            analogIn3 = analogSample[0].data;
            break;
          default:
            Serial.println("??");
            break;

        }

      }
    }
  }
}
