/**   Arduino: Mega
  *   created by: Relry Pereira dos Santos
  */
#include <Arduino.h>
#include <Wire.h>
#include <KtaneCore.h>

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

Status currentStatusBomb = RESETING;

volatile unsigned long lastMillis = 0;

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

  BLT_SERIAL.println(F("---------------------------------"));
  while (currentMod != NULL) {
    printInfoModule(currentMod);
    currentMod = currentMod->nextModule;
  }

  BLT_SERIAL.println();
  BLT_SERIAL.println((String)F("Modulos configurados: ") + (String) numberModules);
  BLT_SERIAL.println((String)F("Fim do SETUP."));
}

void loop() {
  switch (currentStatusBomb) {
    case RESETING:
    case READY:
      waitBeginGame();
      break;
    case IN_GAME:
      executeInGame();
      break;
    case STOP_GAME:
      executeStopGame();
      break;
  }
}

void executeInGame() {
  if(BLT_SERIAL.available()) {
    if(BLT_SERIAL.read() == 'r') {
      BLT_SERIAL.println(F("Resetando modulos."));
      writeAllRegisterModules_byte(STATUS, RESETING);
      currentStatusBomb = RESETING;
    }
  }
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastMillis > 500) {
    if (validaTimer() == STOP_GAME) {
      BLT_SERIAL.println(F("GAME OVER!"));
      currentStatusBomb = STOP_GAME;
    }
    lastMillis = currentMillis;
  }
}

void executeStopGame() {
  BLT_SERIAL.println(F("Fim de jogo reinicie para jogar novamente."));
  while(true) {
    if(BLT_SERIAL.available()) {
      if(BLT_SERIAL.read() == 'r') {
        BLT_SERIAL.println(F("Resetando modulos."));
        writeAllRegisterModules_byte(STATUS, RESETING);
        currentStatusBomb = RESETING;
        break;
      }
    }
  }
}

Status validaTimer() {
  byte *response;
  requestRegisterModule_byte(timerModule->address, STATUS, &response);
  BLT_SERIAL.println((String)F("status modulo: ") + Status_name[*response]);
  return (Status)*response;
}

void waitBeginGame() {
  BLT_SERIAL.println(F("Aguardando instrucao para iniciar jogo."));
  BLT_SERIAL.println(F("Funcoes de modulos. Exemplo: m;0;c;60000"));
  BLT_SERIAL.println(F("Funcoes do core. Exemplo: c;r"));
  BLT_SERIAL.println(F(" c;1; - Jogar"));
  BLT_SERIAL.println(F(" c;r; - Resetar"));
  BLT_SERIAL.println(F(" m;X;e;1; - Ativar/Desativar o modulo."));
  BLT_SERIAL.println(F(" m;X;c;DATA; - Configurar modulo."));

  String command;
  while(true) {
    command = BLT_SERIAL.readStringUntil(';');

    if (command.equalsIgnoreCase("m")) {
      String addressMod = BLT_SERIAL.readStringUntil(';');
      if (addressMod.equals("")) addressMod = "-1";
      
      Module* mod = getModuleByAddress(addressMod.toInt());
      if (mod != NULL) {
        BLT_SERIAL.println(F("Modulo encontrado para configurar: "));
        printInfoModule(mod);
        
        String action = BLT_SERIAL.readStringUntil(';');
        
        if(action.equalsIgnoreCase("e")) {
          String mensagem = BLT_SERIAL.readStringUntil(';');
          if (mensagem.equals("0") || mensagem.equals("1")) {
            writeRegisterModule_byte(mod->address, ENABLED, mensagem.equals("1"));
          } else {
            BLT_SERIAL.println((String)F("opcao invalida: ") + mensagem);   
          }
        } else if(action.equalsIgnoreCase("c")) {
          String mensagem = BLT_SERIAL.readStringUntil(';');
          BLT_SERIAL.println((String)F("data enviado: ") + mensagem);
          writeConfigRegisterModule(mod->address, action.c_str()[0], mensagem);
        } else {
          BLT_SERIAL.println((String)F("opcao invalida: ") + action);          
        }
      } else {
        BLT_SERIAL.println((String)F("Nao existe modulo com o address: ") + addressMod);
      }
    } else if (command.equalsIgnoreCase("c")) {
      String action = BLT_SERIAL.readStringUntil(';');
      if (action.equalsIgnoreCase("1")) {
        break;
      } else if (action.equalsIgnoreCase("r")) {
        BLT_SERIAL.println(F("Not implemented"));
      }
    }
  }
  
  BLT_SERIAL.println((String)F("Começando jogo em..."));
  for (int i = 3; i > 0; i--) {
    BLT_SERIAL.println(i);
    delay(1000);
  }
  BLT_SERIAL.println((String)F("GO!"));

  currentStatusBomb = IN_GAME;
  sendBeginGame();
}

void printInfoModule(Module* mod) {
  if (mod != NULL) {
    BLT_SERIAL.println((String)F("address.: ") + (String)mod->address);
    BLT_SERIAL.println((String)F("codeName: ") + (String)mod->codeName);
    BLT_SERIAL.println((String)F("version.: ") + (String)mod->version);
    BLT_SERIAL.println((String)F("status..: ") + Status_name[mod->status]);
    BLT_SERIAL.println(F("---------------------------------"));
  }
}

Module* getModuleByAddress(int address) {
  currentMod = modules;
  while (currentMod != NULL) {
    if (currentMod->address == address) {
      return currentMod;
    }
    currentMod = currentMod->nextModule;
  }
  return NULL;
}

void sendBeginGame() {
  currentMod = modules;

  while(currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, STATUS, IN_GAME);
    currentMod = currentMod->nextModule;
  }

  BLT_SERIAL.println(F("Modulos avisados do inicio do game."));
}

int readModules() {
  int numbersModules = 0;
  modules = currentMod = NULL;

  // delay para garantir que os modulos já estarão iniciados e endereçados
  delay(1000);

  BLT_SERIAL.println(F("Iniciando a varredura de modulos."));
  for (int x = 0; x < MAX_ADDRESS; x++) {
    BLT_SERIAL.println((String)F("------------- address 0x0") + (String) x + (String)F(" -------------"));
    //inicia a transmição com um endereço e finaliza para verificar se houve resposta(se existe)
    BLT_SERIAL.println((String)F("Iniciando leitura do endereco: ") + (String) x);
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
      BLT_SERIAL.println((String)F("slaveCodeName: ") + (String)slaveCodeName);
      
      requestRegisterModule(x, VERSION, &slaveVersion);
      BLT_SERIAL.println((String)F("slaveVersion.: ") + (String)slaveVersion);

      requestRegisterModule_byte(x, STATUS, &slaveStatus);
      BLT_SERIAL.println((String)F("slaveStatus..: ") + (String)(*slaveStatus));


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
      BLT_SERIAL.println((String)F("reqSlave_address: ") + (String)currentMod->address);
    } else {
      BLT_SERIAL.println((String)F("Sem Resposta, retorno: ") + (String) error);
    }
    BLT_SERIAL.println(F("------------------------------------------------"));
  }
  BLT_SERIAL.println(F("Varredura de modulos finalizadas."));

  currentMod = modules;

  BLT_SERIAL.println((String)F("Numero de modulos encontrados: ") + (String) numbersModules);
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
    BLT_SERIAL.println((String)F("Fim da transmissao com erro: ") + error_code);
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