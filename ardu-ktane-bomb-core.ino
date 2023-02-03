/**   Arduino: Mega
  *   created by: Relry Pereira dos Santos
  */
#include <Arduino.h>
#include <Wire.h>
#include <KtaneCore.h>
#include <SoftwareSerial.h>

#define BLT_SERIAL Serial1

#define PIN_SDA 20
#define PIN_SCL 21

#define PIN_RESET 2

#define MAX_ADDRESS 8

#define LED_WARNING 13

typedef struct MODULE {
  int address;
  char *codeName;
  char *version;
  Status status;
  struct MODULE *nextModule;
} Module;

Module *modules, *currentMod, *timerModule;

SoftwareSerial bluetooth(10, 11); // RX, TX 

void setup() {
  pinMode(LED_WARNING, OUTPUT);

  Wire.begin();
  BLT_SERIAL.begin(38400);
  BLT_SERIAL.println(F("Iniciando programa..."));

  int numberModules = 0;
  do {
    numberModules = readModules();

    // Caso não exista nenhum modulo, fica no loop de alerta
    if (numberModules == 0) {
      BLT_SERIAL.println(F("ALERTA!!!"));
      BLT_SERIAL.println(F("Não existe nenhum módulo configurado."));
      BLT_SERIAL.println(F("Pressione 'r' para fazer a varredura novamente."));
      while (true) {
        if(BLT_SERIAL.available()){
          if (((char)BLT_SERIAL.read()) == 'r') break;
        }        
        digitalWrite(LED_WARNING, HIGH);
        delay(250);
        digitalWrite(LED_WARNING, LOW);
        delay(250);
      }
    }
  } while(numberModules == 0);

  BLT_SERIAL.println("---------------------------------");
  while (currentMod != NULL) {
    BLT_SERIAL.println("address.: " + (String)currentMod->address);
    BLT_SERIAL.println("codeName: '" + (String)currentMod->codeName + "'");
    BLT_SERIAL.println("version.: " + (String)currentMod->version);
    BLT_SERIAL.println("status..: " + Status_name[currentMod->status]);
    BLT_SERIAL.println("---------------------------------");
    currentMod = currentMod->nextModule;
  }

  BLT_SERIAL.println();
  BLT_SERIAL.println((String)F("Modulos configurados: ") + (String) numberModules);
  BLT_SERIAL.println((String)F("Fim do SETUP."));

  return waitBeginGame();
}

void loop() {
  if(BLT_SERIAL.available()) {
    if(BLT_SERIAL.read() == 'r') {
      BLT_SERIAL.println("Resetando modulos.");
      writeAllRegisterModules_byte(STATUS, RESETING);
      return waitBeginGame();
    }
  }
  delay(1000);

  if(validaTimer() == STOP_GAME) {
    BLT_SERIAL.println("GAME OVER!");
    char c = '0';
    while(c != 'r') {
      if(BLT_SERIAL.available()) {
        if(BLT_SERIAL.read() == 'r') {
          BLT_SERIAL.println("Resetando modulos.");
          writeAllRegisterModules_byte(STATUS, RESETING);
          return waitBeginGame();
        }
      }
    }
  }  
}

Status validaTimer() {
  byte *response;
  requestRegisterModule_byte(timerModule->address, STATUS, &response);
  BLT_SERIAL.print("status modulo: ");
  BLT_SERIAL.println(Status_name[*response]);
  return (Status)*response;
}

void waitBeginGame() {
  BLT_SERIAL.println("Aguardando instrucao para iniciar jogo.");
  BLT_SERIAL.println(" 1 - Jogar");
  BLT_SERIAL.println(" e - Ativa os modulos(teste)");
  BLT_SERIAL.println(" d - Desativa os modulos(teste)");
  BLT_SERIAL.println(" r - Resetar(em jogo)");

  char command = '0';
  do {
    command = BLT_SERIAL.read();
    if(command == 'e') {
      writeAllRegisterModules_byte(ENABLED, true);
    } else if(command == 'd') {
      writeAllRegisterModules_byte(ENABLED, false);
    } else if(command == 'c') {
      BLT_SERIAL.println("Enviando dados para configurar modulo.");
      int numBytes = BLT_SERIAL.available();
      char buf[numBytes+1];
      BLT_SERIAL.readBytes(buf, numBytes);
      buf[numBytes] = '\0';
      BLT_SERIAL.print("buf: ");
      BLT_SERIAL.println(buf);
      String mensagem = "";
      mensagem.concat(buf);
      BLT_SERIAL.print("mensagem: ");
      BLT_SERIAL.println(mensagem);
      writeConfigRegisterModule(timerModule->address, command, mensagem);
    }
  } while(command != '1');
  
  BLT_SERIAL.println("Começando jogo em...");
  BLT_SERIAL.println("3");
  delay(1000);
  BLT_SERIAL.println("2");
  delay(1000);
  BLT_SERIAL.println("1");
  delay(1000);

  sendBeginGame();
  return loop();
}

void sendBeginGame() {
  currentMod = modules;

  while(currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, STATUS, IN_GAME);
    currentMod = currentMod->nextModule;
  }

  BLT_SERIAL.println("Modulos avisados do inicio do game.");
}

int readModules() {
  int numbersModules = 0;
  modules = currentMod = NULL;

  // delay para garantir que os modulos já estarão iniciados e endereçados
  delay(1000);

  BLT_SERIAL.println(F("Iniciando a varredura de modulos."));
  for (int x = 0; x < MAX_ADDRESS; x++) {
    BLT_SERIAL.println("----------------- address 0x0" + (String) x + " -----------------");
    //inicia a transmição com um endereço e finaliza para verificar se houve resposta(se existe)
    BLT_SERIAL.println("Iniciando leitura do endereco: " + (String) x);
    Wire.beginTransmission(x);
    int error = Wire.endTransmission();

    //retorno 0(zero) resposta do endereço foi bem sucedida
    if (error == 0) {
      BLT_SERIAL.println(F("Modulo encontrado."));
      if (modules == NULL) {
        modules = currentMod = (Module*) malloc(sizeof(Module));
      } else {
        currentMod->nextModule = (Module*) malloc(sizeof(Module));
        currentMod = currentMod->nextModule;
      }

      BLT_SERIAL.println(F("Preparando para ler code do modulo."));
      char *slaveCodeName;
      char *slaveVersion;
      byte *slaveStatus;

      requestRegisterModule(x, CODE_NAME, &slaveCodeName);
      BLT_SERIAL.println("Mensagem retornada do slave: '" + (String)slaveCodeName + "'");
      
      requestRegisterModule(x, VERSION, &slaveVersion);
      BLT_SERIAL.println("Mensagem retornada do slave: " + (String)slaveVersion);

      requestRegisterModule_byte(x, STATUS, &slaveStatus);
      BLT_SERIAL.println("Mensagem retornada do slave: " + (String)(*slaveStatus));


      currentMod->address = x;
      currentMod->codeName = slaveCodeName;
      currentMod->version  = slaveVersion;
      currentMod->status   = (Status) *slaveStatus;
      currentMod->nextModule = NULL;

      if(((String)slaveCodeName).equalsIgnoreCase("module-display")) {
        BLT_SERIAL.println("Encontrado modulo timer.");
        timerModule = currentMod;
      }

      numbersModules++;
      BLT_SERIAL.println("reqSlave_address: " + (String)currentMod->address);
    } else {
      BLT_SERIAL.println("Sem Resposta, retorno: " + (String) error);
    }
    BLT_SERIAL.println(F("------------------------------------------------"));
  }
  BLT_SERIAL.println(F("Varredura de modulos finalizadas."));

  currentMod = modules;

  BLT_SERIAL.println("Numero de modulos encontrados: " + (String) numbersModules);
  return numbersModules;
}

boolean writeAllRegisterModules_byte(EnumRegModule moduleRegister, byte value) {
  currentMod = modules;

  while(currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, moduleRegister, value);
    currentMod = currentMod->nextModule;
  }
}

boolean writeRegisterModule_byte(int i2c_addr, EnumRegModule moduleRegister, byte value) {
  byte error_code;
  boolean data_valid = false;
  RegRequest regReq = {WRITE, MESSAGE, moduleRegister};

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  Wire.write(value);
  error_code  = Wire.endTransmission();    

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }
  
  return true;
}

boolean writeConfigRegisterModule(int i2c_addr, char command, String message) {
  byte error_code;
  RegRequest regReq = {WRITE, MESSAGE, DATA};

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  Wire.write(command);
  Wire.write(message.c_str());
  error_code  = Wire.endTransmission();    

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }
  
  return true;
}

boolean requestRegisterModule(int i2c_addr, EnumRegModule moduleRegister, char **buffer) {
  byte error_code;
  boolean data_valid = false;
  RegRequest regReq = {READ, LENGHT, moduleRegister};

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  error_code  = Wire.endTransmission();    

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }
  
  Wire.requestFrom(i2c_addr, 1);
  if(Wire.available()) {
    int numResponse = Wire.read();
    regReq.lorm = MESSAGE;

    Wire.beginTransmission(i2c_addr);                              
    Wire.write(regReq);
    error_code  = Wire.endTransmission();

    if (error_code) {
      BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
      return false;
    }

    Wire.requestFrom(i2c_addr, numResponse);    

    if (Wire.available()) {
      (*buffer) = (char*) malloc(sizeof(char) * numResponse + 1);
      int i = 0;
      while (Wire.available()) {
        (*buffer)[i++] = (char)Wire.read();
      }
      (*buffer)[i] = '\0';
      return true;
    }
  }
  
  return false;
}

boolean requestRegisterModule_byte(int i2c_addr, EnumRegModule moduleRegister, byte **buffer) {
  byte error_code;
  boolean data_valid = false;
  RegRequest regReq = {READ, MESSAGE, moduleRegister};

  Wire.beginTransmission(i2c_addr);                              
  Wire.write(regReq);
  error_code  = Wire.endTransmission();

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }

  Wire.requestFrom(i2c_addr, 1);    

  (*buffer) = (byte*) malloc(sizeof(byte));
  if (Wire.available()) {
    byte resp = Wire.read();
    (**buffer) = resp;
    return true;
  }
  
  return false;
}