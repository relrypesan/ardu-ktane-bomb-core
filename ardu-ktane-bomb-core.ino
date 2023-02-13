
/**   Arduino: Mega
  *   created by: Relry Pereira dos Santos
  */
#include <EEPROM.h>
#include <Arduino.h>
#include <Wire.h>
#include <KtaneCore.h>
#include <ArduUtil.h>

#define BLT_SERIAL Serial

#define PIN_SDA 20
#define PIN_SCL 21
#define LED_WARNING 13

#define MAX_ADDRESS 8

#define EEPROM_ADDRESS_TIME_DISPLAY 1023

#define ID_DISPLAY_TIMER F("module-display")
#define MAX_FAULT_DEFUSE 3

typedef struct MODULE {
  short address;
  char *codeName;
  char *version;
  Status status;
  struct MODULE *nextModule;
} Module;

Module *modules, *currentMod, *timerModule;
short numberModules = 0;

Status currentStatusBomb = RESETING;

volatile unsigned long lastMillis = 0;
short countFault = 0;
short countModulesToDefuse = 0;
short countModulesDefused = 0;

void setup() {
  pinMode(LED_WARNING, OUTPUT);

  Wire.begin();
  BLT_SERIAL.begin(38400);
  BLT_SERIAL.println(F("Iniciando programa..."));

  // delay para garantir que os modulos já estarão iniciados e endereçados
  delay(1000);

  numberModules = waitBuscaDispositivosConectados();
  printAllModules();

  initConfigModules();
  sendResetGame();

  BLT_SERIAL.println();
  BLT_SERIAL.println((String)F("Modulos configurados: ") + (String)numberModules);
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
    case DEFUSED:
    case STOP_GAME:
      executeStopGame();
      break;
  }
}

int waitBuscaDispositivosConectados() {
  int countModules = 0;
  bool ready = false;
  do {
    countModules = readModules();
    bool allRequiredModulesOk = validRequiredModules();
    ready = true;

    // Caso não exista nenhum modulo, fica no loop de alerta
    if (countModules == 0) {
      BLT_SERIAL.println(F("ALERTA!!!\nNao existe nenhum modulo configurado."));
      ready = false;
    } else if (allRequiredModulesOk == false) {
      BLT_SERIAL.println(F("ALERTA!!!\nModulos necessarios para o jogo."));
      ready = false;
    }

    if(ready == false) {
      BLT_SERIAL.println(F("Pressione 'r' para fazer a varredura novamente."));
      while (true) {
        if (BLT_SERIAL.available()) {
          if (((char)BLT_SERIAL.read()) == 'r') break;
        }
        digitalWrite(LED_WARNING, HIGH);
        delay(250);
        digitalWrite(LED_WARNING, LOW);
        delay(250);
      }      
    }
  } while (ready == false);

  BLT_SERIAL.println(F("Busca de modulos finalizada."));

  return countModules;
}

void initConfigModules() {
  BLT_SERIAL.println((String)F("Tamanho da memoria EEPROM: ") + EEPROM.length());
  int timeDisplay = 0;
  timeDisplay = EEPROM.get(EEPROM_ADDRESS_TIME_DISPLAY, timeDisplay);
  BLT_SERIAL.println((String)F("Valor da memoria EEPROM: ") + timeDisplay);
  if (timeDisplay == -1) {
    timeDisplay = 300;
    BLT_SERIAL.println((String)F("Valor inicial salvo na memoria EEPROM: ") + timeDisplay);
    BLT_SERIAL.println((String)F("tamanho de bytes: ") + sizeof(timeDisplay));
    EEPROM.put(EEPROM_ADDRESS_TIME_DISPLAY, timeDisplay);
  }

  if (timerModule != NULL) {
    long timeDisplayTmp = (long)timeDisplay * 1000;
    BLT_SERIAL.println(F("Tempo inicial definido no modulo display."));
    writeConfigRegisterModule(timerModule->address, 'c', String(timeDisplayTmp));
  }
}

bool validRequiredModules() {
  if (timerModule == NULL) {
    BLT_SERIAL.println((String)F("Modulo requirido não encontrado: ") + (String)ID_DISPLAY_TIMER);
    return false;
  }
  return true;
}

void executeInGame() {
  if (BLT_SERIAL.available()) {
    if (BLT_SERIAL.read() == 'r') {
      BLT_SERIAL.println(F("Resetando modulos."));
      writeAllRegisterModules_byte(STATUS, RESETING);
      currentStatusBomb = RESETING;
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastMillis > 1000) {
    if (validaTimer() == STOP_GAME) {
      BLT_SERIAL.println(F("GAME OVER!"));
      currentStatusBomb = STOP_GAME;
      sendEndGame();
    }
    validaPenalidadeModulos();
    validaModulosDefusados();
    Serial.println((String)F("Memory free: ") + freeMemory());
    lastMillis = currentMillis;
  }

  if (countFault >= MAX_FAULT_DEFUSE) {
    BLT_SERIAL.println(F("GAME OVER!"));
    currentStatusBomb = STOP_GAME;
    sendEndGame();
  } else if (countModulesToDefuse == countModulesDefused) {
    BLT_SERIAL.println(F("PARABENS! Bomba defusada a tempo!"));
    currentStatusBomb = DEFUSED;
    sendEndGame();
  }
}

void executeStopGame() {
  BLT_SERIAL.println(F("Fim de jogo reinicie para jogar novamente."));
  while (true) {
    if (BLT_SERIAL.available()) {
      if (BLT_SERIAL.read() == 'r') {
        BLT_SERIAL.println(F("Resetando modulos."));
        writeAllRegisterModules_byte(STATUS, RESETING);
        currentStatusBomb = RESETING;
        break;
      }
    }
  }
}

void validaPenalidadeModulos() {
  currentMod = modules;
  while (currentMod != NULL) {
    if (!((String)currentMod->codeName).equalsIgnoreCase(ID_DISPLAY_TIMER)) {
      String message = requestRegisterModule_String(currentMod->address, FAULT);
      if (!message.equals("") && message.toInt() == 1) {
        BLT_SERIAL.println((String)F("address...: 0x0") + currentMod->address);
        BLT_SERIAL.println((String)F("codeName..: ") + currentMod->codeName);
        BLT_SERIAL.println((String)F("Fault desc: ") + message);
        countFault++;
      }
    }
    currentMod = currentMod->nextModule;
  }
}

void validaModulosDefusados() {
  currentMod = modules;
  while (currentMod != NULL) {
    if (!((String)currentMod->codeName).equalsIgnoreCase(ID_DISPLAY_TIMER)) {
      // String message = requestRegisterModule_String(currentMod->address, STATUS);
      byte *response;
      requestRegisterModule_byte(currentMod->address, STATUS, &response);
      if (currentMod->status != DEFUSED && (Status)*response == DEFUSED) {
        currentMod->status = DEFUSED;
        BLT_SERIAL.println((String)F("address...: 0x0") + currentMod->address);
        BLT_SERIAL.println((String)F("codeName..: ") + currentMod->codeName);
        BLT_SERIAL.println((String)F("MODULO DEFUSADO!"));
        countModulesDefused++;
      }
      free(response);
    }
    currentMod = currentMod->nextModule;
  }
}

Status validaTimer() {
  byte *response;
  requestRegisterModule_byte(timerModule->address, STATUS, &response);
  Status status = (Status)*response;
  free(response);
  // BLT_SERIAL.println((String)F("status modulo: ") + Status_name[*response]);
  return status;
}

bool validaModulosReady() {
  bool modulesReady = true;
  currentMod = modules;
  countModulesToDefuse = countModulesDefused = 0;

  while (currentMod != NULL) {
    byte *response;

    requestRegisterModule_byte(currentMod->address, STATUS, &response);
    // free(&(currentMod->status));
    currentMod->status = (Status)*response;
    free(response);
    printInfoModule(currentMod);
    if (currentMod->status != READY) modulesReady = false;
    if (currentMod != timerModule) countModulesToDefuse++;

    currentMod = currentMod->nextModule;
  }

  return modulesReady;
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
  while (true) {
    command = BLT_SERIAL.readStringUntil(';');

    if (command.equalsIgnoreCase("m")) {
      String addressMod = BLT_SERIAL.readStringUntil(';');
      if (addressMod.equals("")) addressMod = "-1";

      Module *mod = getModuleByAddress(addressMod.toInt());
      if (mod != NULL) {
        BLT_SERIAL.println(F("Modulo encontrado para configurar: "));
        printInfoModule(mod);

        String action = BLT_SERIAL.readStringUntil(';');

        if (action.equalsIgnoreCase("e")) {
          String mensagem = BLT_SERIAL.readStringUntil(';');
          if (mensagem.equals("0") || mensagem.equals("1")) {
            writeRegisterModule_byte(mod->address, ENABLED, mensagem.equals("1"));
          } else {
            BLT_SERIAL.println((String)F("opcao invalida: ") + mensagem);
          }
        } else if (action.equalsIgnoreCase("c")) {
          String mensagem = BLT_SERIAL.readStringUntil(';');
          BLT_SERIAL.println((String)F("data enviado: ") + mensagem);
          if (mod == timerModule) {
            int valor = ((int)(mensagem.toInt() / (long)1000));
            BLT_SERIAL.println(F("Salvando na EEPROM o valor do timer em segundos."));
            BLT_SERIAL.println((String)F("Valor salvo: ") + valor);
            EEPROM.put(EEPROM_ADDRESS_TIME_DISPLAY, valor);
          }
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
        if (validaModulosReady()) {
          break;
        } else {
          BLT_SERIAL.println(F("Existem modulos que nao estao prontos"));
        }
      } else if (action.equalsIgnoreCase("r")) { // reset modules
        sendResetGame();
        delay(500);
        validaModulosReady();
      } else if (action.equalsIgnoreCase("s")) { // search modules
        waitBuscaDispositivosConectados();
      } else if (action.equalsIgnoreCase("l")) { // list modules
        printAllModules();
      }
    }

    BLT_SERIAL.println((String)F("Memory free: ") + freeMemory());
  }

  BLT_SERIAL.println((String)F("Modulos a serem defusados: ") + countModulesToDefuse);
  BLT_SERIAL.println((String)F("Começando jogo em..."));
  for (int i = 3; i > 0; i--) {
    BLT_SERIAL.println(i);
    delay(1000);
  }
  BLT_SERIAL.println((String)F("GO!"));

  currentStatusBomb = IN_GAME;
  sendBeginGame();
}

void printAllModules() {
  Module *tmpModule = modules;
  unsigned short countModules = 0;
  BLT_SERIAL.println(F("---------------------------------"));
  while (tmpModule != NULL) {
    printInfoModule(tmpModule);
    countModules++;
    tmpModule = tmpModule->nextModule;
  }
  BLT_SERIAL.println((String)F("Modules count: ") + countModules);
}

void printInfoModule(Module *mod) {
  if (mod != NULL) {
    BLT_SERIAL.println((String)F("address.: ") + (String)mod->address);
    BLT_SERIAL.println((String)F("codeName: ") + (String)mod->codeName);
    BLT_SERIAL.println((String)F("version.: ") + (String)mod->version);
    BLT_SERIAL.println((String)F("status..: ") + Status_name[mod->status]);
    BLT_SERIAL.println(F("---------------------------------"));
  }
}

Module *getModuleByAddress(int address) {
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

  while (currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, STATUS, IN_GAME);
    currentMod = currentMod->nextModule;
  }

  BLT_SERIAL.println(F("Modulos avisados do inicio do game."));
}

void sendResetGame() {
  currentMod = modules;

  while (currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, STATUS, RESETING);
    currentMod = currentMod->nextModule;
  }

  countFault = 0;

  BLT_SERIAL.println(F("Modulos avisados do reset do game."));
}

void sendEndGame() {
  currentMod = modules;

  while (currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, STATUS, STOP_GAME);
    currentMod = currentMod->nextModule;
  }

  BLT_SERIAL.println(F("Modulos avisados do FIM do game."));
}

int readModules() {
  int numbersModules = 0;
  currentMod = modules;

  while (currentMod != NULL) {
    modules = modules->nextModule;
    free(currentMod->codeName);
    free(currentMod->version);
    free(currentMod);
    currentMod = modules;
  }

  modules = currentMod = timerModule = NULL;

  BLT_SERIAL.println(F("Iniciando a varredura de modulos."));
  for (int x = 0; x < MAX_ADDRESS; x++) {
    BLT_SERIAL.println((String)F("------------- address 0x0") + (String)x + (String)F(" -------------"));
    //inicia a transmição com um endereço e finaliza para verificar se houve resposta(se existe)
    BLT_SERIAL.println((String)F("Iniciando leitura do endereco: ") + (String)x);
    Wire.beginTransmission(x);
    int error = Wire.endTransmission();

    //retorno 0(zero) resposta do endereço foi bem sucedida
    if (error == 0) {
      BLT_SERIAL.println(F("Modulo encontrado."));
      if (modules == NULL) {
        modules = currentMod = (Module *)malloc(sizeof(Module));
      } else {
        currentMod->nextModule = (Module *)malloc(sizeof(Module));
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
      currentMod->version = slaveVersion;
      currentMod->status = (Status)*slaveStatus;
      currentMod->nextModule = NULL;

      free(slaveStatus);

      if (((String)slaveCodeName).equalsIgnoreCase(ID_DISPLAY_TIMER)) {
        BLT_SERIAL.println("Encontrado modulo timer.");
        timerModule = currentMod;
      }

      numbersModules++;
      BLT_SERIAL.println((String)F("reqSlave_address: ") + (String)currentMod->address);
    } else {
      BLT_SERIAL.println((String)F("Sem Resposta, retorno: ") + (String)error);
    }
    BLT_SERIAL.println(F("------------------------------------------------"));
  }
  BLT_SERIAL.println(F("Varredura de modulos finalizadas."));

  currentMod = modules;

  BLT_SERIAL.println((String)F("Numero de modulos encontrados: ") + (String)numbersModules);
  return numbersModules;
}

boolean writeAllRegisterModules_byte(EnumRegModule moduleRegister, byte value) {
  currentMod = modules;

  while (currentMod != NULL) {
    writeRegisterModule_byte(currentMod->address, moduleRegister, value);
    currentMod = currentMod->nextModule;
  }
}

boolean writeRegisterModule_byte(int i2c_addr, EnumRegModule moduleRegister, byte value) {
  byte error_code;
  boolean data_valid = false;
  RegRequest regReq = { WRITE, MESSAGE, moduleRegister };

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  Wire.write(value);
  error_code = Wire.endTransmission();

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }

  return true;
}

boolean writeConfigRegisterModule(int i2c_addr, char command, String message) {
  byte error_code;
  RegRequest regReq = { WRITE, MESSAGE, DATA };

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  Wire.write(command);
  Wire.write(message.c_str());
  error_code = Wire.endTransmission();

  if (error_code) {
    BLT_SERIAL.println((String)F("Fim da transmissao com erro: ") + error_code);
    return false;
  }

  return true;
}

boolean requestRegisterModule(int i2c_addr, EnumRegModule moduleRegister, char **buffer) {
  byte error_code;
  boolean data_valid = false;
  RegRequest regReq = { READ, LENGHT, moduleRegister };

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  error_code = Wire.endTransmission();

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }

  Wire.requestFrom(i2c_addr, 1);
  if (Wire.available()) {
    int numResponse = Wire.read();
    regReq.lorm = MESSAGE;

    Wire.beginTransmission(i2c_addr);
    Wire.write(regReq);
    error_code = Wire.endTransmission();

    if (error_code) {
      BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
      return false;
    }

    Wire.requestFrom(i2c_addr, numResponse);

    if (Wire.available()) {
      (*buffer) = (char *)malloc(sizeof(char) * numResponse + 1);
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

String requestRegisterModule_String(int i2c_addr, EnumRegModule moduleRegister) {
  byte error_code;
  RegRequest regReq = { READ, MESSAGE, moduleRegister };

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  error_code = Wire.endTransmission();

  if (error_code) {
    BLT_SERIAL.println((String)F("Fim da transmissao com erro: ") + (String)error_code);
    return "";
  }

  Wire.requestFrom(i2c_addr, 32);
  delay(50);

  if (Wire.available()) {
    String message = Wire.readStringUntil(';');
    Serial.println((String)F("Mensagem string recebida: ") + message);
    return message;
  }

  return "";
}

boolean requestRegisterModule_byte(int i2c_addr, EnumRegModule moduleRegister, byte **buffer) {
  byte error_code;
  boolean data_valid = false;
  RegRequest regReq = { READ, MESSAGE, moduleRegister };

  Wire.beginTransmission(i2c_addr);
  Wire.write(regReq);
  error_code = Wire.endTransmission();

  if (error_code) {
    BLT_SERIAL.println("Fim da transmissao com erro: " + error_code);
    return false;
  }

  Wire.requestFrom(i2c_addr, 1);

  (*buffer) = (byte *)malloc(sizeof(byte));
  if (Wire.available()) {
    byte resp = Wire.read();
    (**buffer) = resp;
    return true;
  }

  return false;
}
