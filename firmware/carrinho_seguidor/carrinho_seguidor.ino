// =====================================================================
//  Carrinho seguidor de linha - ESP32 + L298N + barra de 8 sensores (QTR-8RC)
//  Rede Wi-Fi propria: "ESP32" / senha "esp12345"  ->  abra http://192.168.4.1
// =====================================================================
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "pagina.h"

// ---------------- Wi-Fi ----------------
const char* WIFI_NOME  = "ESP32";
const char* WIFI_SENHA = "esp12345";   // precisa ter no minimo 8 caracteres

// ---------------- Pinos ----------------
const int ENA = 18, IN1 = 16, IN2 = 17;                     // motor direito  (L298N lado A)
const int ENB = 22, IN3 = 19, IN4 = 21;                     // motor esquerdo (L298N lado B)
const int SENSORES[8] = {32, 33, 25, 26, 27, 14, 13, 4};    // D1 (esquerda) ... D8 (direita)
const int PINO_IR = 23;                                     // furo IR da barra: liga os LEDs infravermelhos
const int PINO_BATERIA = 34;                                // meio do divisor de tensao
const int PINO_LED = 2;                                     // LED azul da placa

// ---------------- Ajustes do seu carrinho ----------------
const bool INVERTE_ESQ = false;        // motor girando ao contrario? troque para true
const bool INVERTE_DIR = false;
const bool LINHA_PRETA = true;         // linha preta no chao branco
const float DIVISOR_BAT = 4.0;         // 4 resistores iguais de 100k: 3 do lado da bateria, 1 do lado do GND
const float BAT_VAZIA = 4.4;           // 4 pilhas AA: 4,4 V vazia
const float BAT_CHEIA = 6.4;           //              6,4 V cheia
const uint16_t TIMEOUT_RC = 3000;      // us - tempo maximo de leitura (mais alto = mais folga com a barra longe do chao)
// sem calibracao usa a faixa fixa abaixo; depois de calibrar cada sensor usa o proprio branco e preto
const int BRANCO_MAX = 600;            // leitura ate aqui = branco (o branco oscila entre 0 e ~600)
const int LIMIAR_PRETO = 1500;         // leitura acima disso = sensor no preto
const int CALIB_MIN_FAIXA = 300;       // diferenca minima entre branco e preto para confiar na calibracao
const unsigned long PARADA_SEGURANCA = 1000;  // ms sem comando no manual = para (o Wi-Fi as vezes atrasa)
const unsigned long CONFIRMA_LINHA = 100;     // ms vendo a linha antes de comecar a andar (evita falso disparo)
const int VEL_RE = 160;                       // forca da re quando perde a linha
const unsigned long RE_MAX = 2500;            // ms de re sem achar a linha antes de desistir e parar
const int VEL_CENTRO = 150;                   // forca para girar no lugar ate a linha ficar no meio
const int CENTRO_OK = 500;                    // linha a ate meio sensor do centro = centralizada
const unsigned long CENTRALIZA_MAX = 2000;    // ms tentando centralizar antes de desistir e parar
const unsigned long TODOS_PRETO_MAX = 500;    // ms com os 8 no preto = carrinho levantado ou fora da pista

// ---------------- Estado ----------------
enum Modo { PARADO, AUTO, MANUAL, CALIBRANDO };
const char* NOMES_MODO[] = {"Parado", "Automático", "Manual", "Calibrando"};
Modo modo = PARADO;

WebServer server(80);
Preferences prefs;

// vb  = velocidade no modo automatico
// rt  = folga no centro: distancia do centro (1000 = um sensor) em que os dois motores ficam iguais
// cv  = forca da curva em % (100 = nas pontas a roda de fora vai a 255 e a de dentro a -255)
// bal = balanco dos motores em % (positivo = tira forca do direito, negativo = do esquerdo)
// pm  = forca minima que faz o motor sair do lugar (zona morta)
// vm  = velocidade no modo manual
// inv = inverte o lado da direcao (barra de sensores montada ao contrario)
struct Config { int vb; int rt; int cv; int bal; int pm; int vm; int inv; } cfg = {150, 250, 80, 0, 90, 180, 0};
unsigned long salvarCfgEm = 0;

uint16_t bruto[8], minimo[8], maximo[8];
bool calibrado = false;      // ja existe calibracao salva na flash
int filtrado[8];             // leitura bruta suavizada (media movel)
int valor[8];                // 0 = chao, 1000 = linha
int posicao = 3500;          // 0 (D1) ... 7000 (D8)
bool linhaVisivel = false;
int pretos = 0;              // quantos sensores veem preto (faixa de chegada = quase todos)
bool andando = false;

unsigned long ultimoCmd = 0, calibInicio = 0, vistaDesde = 0, todosDesde = 0;
unsigned long testeAte = 0;    // teste de motores pelo painel (para sozinho depois de 2 s)

// etapas do modo automatico
enum Etapa { AGUARDANDO, SEGUINDO, DANDO_RE, CENTRALIZANDO };
Etapa etapa = AGUARDANDO;
unsigned long etapaDesde = 0;

// voltas
unsigned long voltaInicio = 0, ultimaVolta = 0, recorde = 0, marcaDesde = 0, ultimaMarca = 0;
unsigned long voltasSessao = 0;

// odometro vitalicio (salvo na flash) - so conta quando esta na bateria, nao no cabo USB
unsigned long sessao = 0, baseLigado = 0, baseAndando = 0, totalVoltas = 0;
unsigned long ligadoMs = 0, andandoMs = 0, vezesLigado = 0;
bool contouLigado = false;

const float BAT_PRESENTE = 3.0;        // abaixo disso = sem bateria (so USB)
float bateriaV = -1;
int bateriaPct = 0;
bool naBateria = false;
bool avisouBateria = false, logouBateria = false;
int clientes = 0;

// =====================================================================
//  Logs na memoria flash (LittleFS)
//  Cada linha: sessao|millis|categoria|mensagem
//  Categorias: SIS (sistema) COR (corrida) PID BAT (bateria) CON (conexao)
// =====================================================================
const char* LOG_ATUAL = "/log.txt";
const char* LOG_ANTIGO = "/log_old.txt";
const size_t LOG_MAX = 40000;          // ao passar disso o arquivo vira o "antigo"

void logar(const char* cat, const String& msg) {
  Serial.printf("[%s] %s\n", cat, msg.c_str());
  if (LittleFS.exists(LOG_ATUAL)) {
    File f = LittleFS.open(LOG_ATUAL, "r");
    size_t tam = f.size();
    f.close();
    if (tam > LOG_MAX) {
      LittleFS.remove(LOG_ANTIGO);
      LittleFS.rename(LOG_ATUAL, LOG_ANTIGO);
    }
  }
  File f = LittleFS.open(LOG_ATUAL, "a");
  if (!f) return;
  f.printf("%lu|%lu|%s|%s\n", sessao, millis(), cat, msg.c_str());
  f.close();
}

String segundos(unsigned long ms) {
  return String(ms / 1000.0, 1) + " s";
}

const char* motivoReset() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "ligado pelo botão";
    case ESP_RST_SW:       return "reinício por software";
    case ESP_RST_PANIC:    return "travamento do programa";
    case ESP_RST_BROWNOUT: return "queda de tensão (bateria fraca?)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_EXT:      return "botão reset";
    default:               return "desconhecido";
  }
}

// =====================================================================
//  Motores
// =====================================================================
// zona morta: abaixo de cfg.pm o motor so chia e nao gira. Espalha 1..255 entre pm..255
// para que qualquer forca pedida (inclusive a roda de dentro numa curva) realmente gire
int comZonaMorta(int vel) {
  if (vel == 0) return 0;
  int forca = cfg.pm + (long)min(abs(vel), 255) * (255 - cfg.pm) / 255;
  return vel > 0 ? forca : -forca;
}

void motor(int en, int a, int b, int vel, bool inverte) {
  if (inverte) vel = -vel;
  vel = comZonaMorta(constrain(vel, -255, 255));
  digitalWrite(a, vel > 0);
  digitalWrite(b, vel < 0);
  ledcWrite(en, abs(vel));
}

void motores(int esq, int dir) {
  andando = esq != 0 || dir != 0;
  // balanco: um motor mais forte que o outro faz o carrinho andar torto
  if (cfg.bal > 0) dir = (long)dir * (100 - cfg.bal) / 100;
  if (cfg.bal < 0) esq = (long)esq * (100 + cfg.bal) / 100;
  motor(ENB, IN3, IN4, esq, INVERTE_ESQ);
  motor(ENA, IN1, IN2, dir, INVERTE_DIR);
}

// =====================================================================
//  Sensores (barra RC: carrega o capacitor e mede o tempo de descarga)
// =====================================================================
void lerSensores() {
  for (int i = 0; i < 8; i++) {
    pinMode(SENSORES[i], OUTPUT);
    digitalWrite(SENSORES[i], HIGH);
  }
  delayMicroseconds(10);
  for (int i = 0; i < 8; i++) {
    bruto[i] = TIMEOUT_RC;
    pinMode(SENSORES[i], INPUT);
  }
  uint32_t inicio = micros(), dt;
  uint8_t pendentes = 0xFF;
  while (pendentes && (dt = micros() - inicio) < TIMEOUT_RC) {
    for (int i = 0; i < 8; i++) {
      if ((pendentes & (1 << i)) && digitalRead(SENSORES[i]) == LOW) {
        bruto[i] = dt;
        pendentes &= ~(1 << i);
      }
    }
  }

  long soma = 0, peso = 0;
  pretos = 0;
  for (int i = 0; i < 8; i++) {
    // media movel: suaviza a oscilacao do branco (cada leitura nova vale 1/3)
    filtrado[i] = (filtrado[i] * 2 + bruto[i]) / 3;
    int v;
    bool preto;
    if (calibrado && maximo[i] - minimo[i] >= CALIB_MIN_FAIXA) {
      // calibrado: o branco e o preto medidos deste sensor viram 0 e 1000, preto = passou da metade
      v = constrain((long)(filtrado[i] - minimo[i]) * 1000 / (maximo[i] - minimo[i]), 0, 1000);
      if (!LINHA_PRETA) v = 1000 - v;
      preto = v >= 500;
    } else {
      // faixa fixa: ate BRANCO_MAX = 0 (chao), TIMEOUT_RC = 1000 (linha)
      int leitura = LINHA_PRETA ? filtrado[i] : TIMEOUT_RC - filtrado[i];
      v = constrain((long)(leitura - BRANCO_MAX) * 1000 / (TIMEOUT_RC - BRANCO_MAX), 0, 1000);
      preto = leitura > LIMIAR_PRETO;
    }
    valor[i] = v;
    int p = v - 150;             // ignora o ruido do branco no calculo da posicao
    if (p > 0) { soma += (long)p * i * 1000; peso += p; }
    if (preto) pretos++;
  }
  // linha = de 1 a 5 sensores no preto. Os 8 no preto = faixa de chegada, carrinho levantado
  // ou fora da pista (sem nada embaixo o sensor tambem le 2500)
  linhaVisivel = pretos >= 1 && pretos <= 5 && peso > 0;
  if (linhaVisivel) posicao = soma / peso;    // senao guarda a ultima posicao (lado onde viu a linha)
}

void carregarCalibracao() {
  for (int i = 0; i < 8; i++) { minimo[i] = 150; maximo[i] = TIMEOUT_RC; }
  if (prefs.getBytesLength("cmin") == sizeof(minimo)) {
    prefs.getBytes("cmin", minimo, sizeof(minimo));
    prefs.getBytes("cmax", maximo, sizeof(maximo));
    calibrado = true;
  }
}

// =====================================================================
//  Modos
// =====================================================================
void setModo(Modo m) {
  if (m == modo) return;
  motores(0, 0);
  modo = m;
  testeAte = 0;
  voltaInicio = 0;
  vistaDesde = 0;
  todosDesde = 0;
  etapa = AGUARDANDO;
  if (m == CALIBRANDO) {
    calibInicio = millis();
    for (int i = 0; i < 8; i++) { minimo[i] = TIMEOUT_RC; maximo[i] = 0; }
  }
  logar("SIS", String("Modo: ") + NOMES_MODO[m]);
}

void calibrar(unsigned long agora) {
  for (int i = 0; i < 8; i++) {
    if (bruto[i] < minimo[i]) minimo[i] = bruto[i];
    if (bruto[i] > maximo[i]) maximo[i] = bruto[i];
  }
  unsigned long t = agora - calibInicio;
  if (t < 4000) {
    // gira para um lado e para o outro passando os sensores sobre a linha
    int fase = t / 1000;
    int v = (fase == 0 || fase == 3) ? 120 : -120;
    motores(-v, v);
    return;
  }
  motores(0, 0);
  prefs.putBytes("cmin", minimo, sizeof(minimo));
  prefs.putBytes("cmax", maximo, sizeof(maximo));
  calibrado = true;
  String ruins = "", faixas = "";
  for (int i = 0; i < 8; i++) {
    if (maximo[i] - minimo[i] < CALIB_MIN_FAIXA) ruins += " D" + String(i + 1);
    faixas += " " + String(minimo[i]) + "-" + String(maximo[i]);
  }
  logar("SIS", "Calibração (branco-preto em µs):" + faixas);
  if (ruins.length()) logar("SIS", "Calibração fraca (passe a linha embaixo deles ou baixe a barra):" + ruins);
  else logar("SIS", "Calibração concluída");
  setModo(PARADO);
}

void contarVolta(unsigned long agora) {
  if (!voltaInicio) {
    voltaInicio = agora;
    logar("COR", "Largada");
    return;
  }
  unsigned long t = agora - voltaInicio;
  voltaInicio = agora;
  ultimaVolta = t;
  voltasSessao++;
  totalVoltas++;
  prefs.putULong("tv", totalVoltas);
  String msg = "Volta " + String(voltasSessao) + ": " + segundos(t);
  if (!recorde || t < recorde) {
    recorde = t;
    prefs.putULong("rec", recorde);
    msg += " ★ recorde";
  }
  logar("COR", msg);
}

void mudarEtapa(Etapa e, unsigned long agora) {
  etapa = e;
  etapaDesde = agora;
}

void aguardar(unsigned long agora, const String& motivo) {
  motores(0, 0);
  voltaInicio = 0;
  vistaDesde = 0;
  mudarEtapa(AGUARDANDO, agora);
  logar("COR", motivo);
}

// quanto e para onde virar para trazer a linha ao centro (-3500..3500):
// negativo = virar para a esquerda (linha nos sensores da direita, D5..D8), positivo = para a direita.
// Sempre o lado contrario de onde a barra viu a linha; cfg.inv troca, se a barra for montada ao contrario
int erroLinha() {
  int e = 3500 - posicao;
  return cfg.inv ? -e : e;
}

void seguirLinha(unsigned long agora) {
  // faixa de chegada: quase todos os sensores no preto por 30 ms enquanto segue
  if (etapa == SEGUINDO && pretos >= 6) {
    if (!marcaDesde) marcaDesde = agora;
    else if (agora - marcaDesde >= 30 && agora - ultimaMarca > 2000) {
      ultimaMarca = agora;
      contarVolta(agora);
    }
  } else {
    marcaDesde = 0;
  }

  // os 8 no preto por muito tempo nao e linha: carrinho levantado ou fora da pista
  if (pretos >= 6) {
    if (!todosDesde) todosDesde = agora;
    if (agora - todosDesde > TODOS_PRETO_MAX && etapa != AGUARDANDO) {
      aguardar(agora, "Todos os sensores no preto (levantado ou fora da pista): parado");
      return;
    }
  } else {
    todosDesde = 0;
  }

  unsigned long t = agora - etapaDesde;
  switch (etapa) {
    case AGUARDANDO:
      // parado: so comeca a andar depois de ver a linha por CONFIRMA_LINHA ms
      motores(0, 0);
      if (!linhaVisivel) { vistaDesde = 0; return; }
      if (!vistaDesde) vistaDesde = agora;
      if (agora - vistaDesde < CONFIRMA_LINHA) return;
      mudarEtapa(SEGUINDO, agora);
      logar("COR", "Linha encontrada: seguindo");
      return;

    case SEGUINDO:
      if (pretos == 0) {
        // linha sumiu: volta de re pelo caminho que veio ate a barra passar por ela de novo
        motores(-VEL_RE, -VEL_RE);
        mudarEtapa(DANDO_RE, agora);
        logar("COR", "Linha perdida: dando ré até encontrar");
        return;
      }
      if (!linhaVisivel) { motores(cfg.vb, cfg.vb); return; }   // faixa de chegada: atravessa reto
      break;   // segue com a direcao por zonas abaixo

    case DANDO_RE:
      if (linhaVisivel) {
        mudarEtapa(CENTRALIZANDO, agora);
        logar("COR", "Linha reencontrada na ré: centralizando");
        return;
      }
      if (t >= RE_MAX) {
        aguardar(agora, "Linha não encontrada na ré: parado, aguardando linha");
        return;
      }
      motores(-VEL_RE, -VEL_RE);
      return;

    case CENTRALIZANDO: {
      // parado no lugar, gira ate a linha ficar nos sensores do meio e so entao segue reto
      if (pretos == 0) { motores(-VEL_RE, -VEL_RE); mudarEtapa(DANDO_RE, agora); return; }
      int erro = erroLinha();
      if (linhaVisivel && abs(erro) <= CENTRO_OK) {
        mudarEtapa(SEGUINDO, agora);
        logar("COR", "Linha no centro: seguindo");
        return;
      }
      if (t >= CENTRALIZA_MAX) {
        aguardar(agora, "Não conseguiu centralizar a linha: parado, aguardando linha");
        return;
      }
      int lado = erro < 0 ? -1 : 1;
      motores(lado * VEL_CENTRO, -lado * VEL_CENTRO);
      return;
    }
  }

  // objetivo: manter a linha nos sensores do meio. So com ela bem no centro os dois motores
  // ficam iguais; quanto mais ela vai para a ponta, mais a roda de fora acelera (ate 255) e a de
  // dentro freia, chegando a girar para tras nas pontas para virar com forca.
  // erro negativo = vira para a esquerda (linha nos sensores da direita)
  int erro = erroLinha();
  int dist = abs(erro);
  int esq = cfg.vb, dir = cfg.vb;
  if (dist > cfg.rt) {
    long f = (long)(dist - cfg.rt) * 1000 / (3500 - cfg.rt);   // 0 na borda do centro, 1000 na ponta
    long g = min(f, 1000L) * cfg.cv / 100;                     // forca da curva aplicada (0..1000)
    int externa = cfg.vb + (long)(255 - cfg.vb) * min(g * 2, 1000L) / 1000;   // acelera ja na metade
    int interna = cfg.vb - (long)(cfg.vb + 255) * g / 1000;                   // freia e inverte
    if (erro < 0) { esq = interna; dir = externa; }
    else          { esq = externa; dir = interna; }
  }
  motores(esq, dir);
}

void aplicarManual(char d) {
  int v = cfg.vm, giro = v * 7 / 10, curva = v * 4 / 10;
  switch (d) {
    case 'f': motores(v, v); break;
    case 'b': motores(-v, -v); break;
    case 'l': motores(-giro, giro); break;
    case 'r': motores(giro, -giro); break;
    case 'q': motores(curva, v); break;     // frente + esquerda
    case 'e': motores(v, curva); break;     // frente + direita
    case 'z': motores(-curva, -v); break;   // re + esquerda
    case 'c': motores(-v, -curva); break;   // re + direita
    default:  motores(0, 0);
  }
}

// =====================================================================
//  Bateria e tarefas de 1 em 1 segundo
// =====================================================================
void lerBateria() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PINO_BATERIA);
  float v = mv / 8.0 / 1000.0 * DIVISOR_BAT;

  bool agoraNaBateria = v >= BAT_PRESENTE;
  if (agoraNaBateria != naBateria) {
    naBateria = agoraNaBateria;
    logar("BAT", naBateria ? "Rodando na bateria" : "Sem bateria: alimentado só pelo USB");
    if (naBateria && !contouLigado) {
      contouLigado = true;
      vezesLigado++;
      prefs.putULong("lig", vezesLigado);
    }
  }

  if (v < 0.5) { bateriaV = -1; return; }   // divisor nao ligado ou bateria desligada
  bateriaV = v;
  bateriaPct = constrain((int)((v - BAT_VAZIA) * 100 / (BAT_CHEIA - BAT_VAZIA)), 0, 100);
  if (!logouBateria) {
    logouBateria = true;
    logar("BAT", "Bateria em " + String(v, 2) + " V (" + bateriaPct + "%)");
  }
  if (bateriaPct < 15 && !avisouBateria) {
    avisouBateria = true;
    logar("BAT", "Bateria baixa: " + String(v, 2) + " V");
  }
}

// Diagnostico pelo Monitor Serial: envie H, L ou F para o pino IR da barra
void comandoSerial() {
  if (!Serial.available()) return;
  char c = toupper(Serial.read());
  if (c == 'H') { pinMode(PINO_IR, OUTPUT); digitalWrite(PINO_IR, HIGH); }
  else if (c == 'L') { pinMode(PINO_IR, OUTPUT); digitalWrite(PINO_IR, LOW); }
  else if (c == 'F') { pinMode(PINO_IR, INPUT); }
  else if (c == 'T') {
    // descarrega cada saida e solta: se voltar sozinha para HIGH, a barra tem pull-up (tipo analogico)
    Serial.print("[TIPO] apos descarregar: ");
    for (int i = 0; i < 8; i++) {
      pinMode(SENSORES[i], OUTPUT);
      digitalWrite(SENSORES[i], LOW);
      delayMicroseconds(20);
      pinMode(SENSORES[i], INPUT);
      delay(5);
      Serial.print(digitalRead(SENSORES[i]));
      Serial.print(' ');
    }
    Serial.println();
    return;
  }
  else return;
  Serial.printf("[IR] pino IR agora: %c\n", c);
}

void tarefasPorSegundo(unsigned long agora) {
  static unsigned long ultima = 0, ultimoSalvo = 0;
  if (agora - ultima < 1000) return;
  ultima = agora;

  lerBateria();

  // leitura bruta dos sensores no Monitor Serial (para conferir a barra)
  Serial.printf("[SEN] bruto D1..D8: %u %u %u %u %u %u %u %u | posicao %d\n",
                bruto[0], bruto[1], bruto[2], bruto[3], bruto[4], bruto[5], bruto[6], bruto[7], posicao);
  // tensao real de D1 e D2 (pinos 32 e 33 medem tensao mesmo com o Wi-Fi ligado)
  Serial.printf("[ANA] D1 %d mV | D2 %d mV\n", analogReadMilliVolts(SENSORES[0]), analogReadMilliVolts(SENSORES[1]));

  int n = WiFi.softAPgetStationNum();
  if (n != clientes) {
    logar("CON", n > clientes ? "Dispositivo conectado (" + String(n) + " no total)"
                              : "Dispositivo desconectado (" + String(n) + " no total)");
    clientes = n;
    if (n == 0 && modo == MANUAL) setModo(PARADO);
  }

  if (agora - ultimoSalvo >= 60000) {
    ultimoSalvo = agora;
    prefs.putULong("tl", baseLigado + ligadoMs / 1000);
    prefs.putULong("ta", baseAndando + andandoMs / 1000);
  }
}

// =====================================================================
//  Rotas do servidor web
// =====================================================================
void rotaStatus() {
  unsigned long agora = millis();
  char b[520];
  snprintf(b, sizeof(b),
    "{\"m\":%d,\"v\":%.2f,\"p\":%d,\"s\":[%d,%d,%d,%d,%d,%d,%d,%d],\"r\":[%u,%u,%u,%u,%u,%u,%u,%u],\"x\":%d,\"l\":%d,"
    "\"va\":%lu,\"uv\":%lu,\"rec\":%lu,\"nv\":%lu,\"se\":%lu,\"bo\":%lu,\"tl\":%lu,\"ta\":%lu,\"tv\":%lu,\"cl\":%d,\"fb\":%d,\"sg\":%d}",
    modo, bateriaV, bateriaPct,
    valor[0], valor[1], valor[2], valor[3], valor[4], valor[5], valor[6], valor[7],
    bruto[0], bruto[1], bruto[2], bruto[3], bruto[4], bruto[5], bruto[6], bruto[7],
    posicao, linhaVisivel,
    voltaInicio ? agora - voltaInicio : 0UL, ultimaVolta, recorde, voltasSessao,
    sessao, vezesLigado, baseLigado + ligadoMs / 1000, baseAndando + andandoMs / 1000, totalVoltas, clientes,
    naBateria, etapa);
  server.send(200, "application/json", b);
}

void rotaComando() {
  char d = server.arg("d").length() ? server.arg("d")[0] : 's';
  if (modo == CALIBRANDO) { server.send(409, "text/plain", "calibrando"); return; }
  if (d != 's' && modo != MANUAL) setModo(MANUAL);
  if (modo == MANUAL) {
    ultimoCmd = millis();
    aplicarManual(d);
  }
  server.send(200, "text/plain", "ok");
}

// Teste de um motor por vez, sem depender da repeticao de comandos do Wi-Fi
void rotaTesteMotor() {
  if (modo != PARADO) setModo(PARADO);
  int esq = constrain(server.arg("e").toInt(), -255, 255);
  int dir = constrain(server.arg("d").toInt(), -255, 255);
  motores(esq, dir);
  testeAte = millis() + 2000;
  logar("SIS", "Teste de motores: esquerdo " + String(esq) + ", direito " + String(dir));
  server.send(200, "text/plain", "ok");
}

void rotaModo() {
  int m = server.arg("v").toInt();
  if (m >= PARADO && m <= CALIBRANDO) setModo((Modo)m);
  server.send(200, "text/plain", "ok");
}

void rotaConfig() {
  bool mudou = false;
  if (server.hasArg("vb")) { cfg.vb = constrain(server.arg("vb").toInt(), 0, 255); mudou = true; }
  if (server.hasArg("rt")) { cfg.rt = constrain(server.arg("rt").toInt(), 0, 3000); mudou = true; }
  if (server.hasArg("cv")) { cfg.cv = constrain(server.arg("cv").toInt(), 0, 100); mudou = true; }
  if (server.hasArg("inv")) { cfg.inv = server.arg("inv").toInt() ? 1 : 0; mudou = true; }
  if (server.hasArg("bal")) { cfg.bal = constrain(server.arg("bal").toInt(), -50, 50); mudou = true; }
  if (server.hasArg("pm")) { cfg.pm = constrain(server.arg("pm").toInt(), 0, 200); mudou = true; }
  if (server.hasArg("vm")) { cfg.vm = constrain(server.arg("vm").toInt(), 0, 255); mudou = true; }
  if (mudou) salvarCfgEm = millis() + 1500;   // espera parar de mexer para gravar na flash
  char b[120];
  snprintf(b, sizeof(b), "{\"vb\":%d,\"rt\":%d,\"cv\":%d,\"bal\":%d,\"pm\":%d,\"vm\":%d,\"inv\":%d}",
           cfg.vb, cfg.rt, cfg.cv, cfg.bal, cfg.pm, cfg.vm, cfg.inv);
  server.send(200, "application/json", b);
}

void rotaLogs() {
  File arquivos[2];
  size_t total = 0;
  const char* nomes[2] = {LOG_ANTIGO, LOG_ATUAL};
  for (int i = 0; i < 2; i++) {
    if (LittleFS.exists(nomes[i])) {
      arquivos[i] = LittleFS.open(nomes[i], "r");
      total += arquivos[i].size();
    }
  }
  server.setContentLength(total);
  server.send(200, "text/plain; charset=utf-8", "");
  uint8_t buf[512];
  for (int i = 0; i < 2; i++) {
    if (!arquivos[i]) continue;
    while (arquivos[i].available()) {
      size_t n = arquivos[i].read(buf, sizeof(buf));
      server.sendContent((const char*)buf, n);
    }
    arquivos[i].close();
  }
}

void rotaApagarLogs() {
  LittleFS.remove(LOG_ANTIGO);
  LittleFS.remove(LOG_ATUAL);
  logar("SIS", "Logs apagados pelo painel");
  server.send(200, "text/plain", "ok");
}

void rotaHora() {
  static bool sincronizado = false;
  if (!sincronizado && server.hasArg("e")) {
    sincronizado = true;
    logar("SIS", "Relógio sincronizado @" + server.arg("e"));
  }
  server.send(200, "text/plain", "ok");
}

void rotaZerarRecorde() {
  recorde = 0;
  prefs.putULong("rec", 0);
  logar("COR", "Recorde zerado");
  server.send(200, "text/plain", "ok");
}

void rotaZerarOdometro() {
  vezesLigado = naBateria ? 1 : 0;      // se ja esta na bateria, esta vez conta
  baseLigado = baseAndando = ligadoMs = andandoMs = totalVoltas = 0;
  prefs.putULong("lig", vezesLigado);
  prefs.putULong("tl", 0);
  prefs.putULong("ta", 0);
  prefs.putULong("tv", 0);
  logar("SIS", "Odômetro zerado");
  server.send(200, "text/plain", "ok");
}

// =====================================================================
//  Setup e loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);
  pinMode(PINO_IR, OUTPUT);
  digitalWrite(PINO_IR, HIGH);   // LEDs infravermelhos da barra sempre acesos
  for (int p : {IN1, IN2, IN3, IN4}) pinMode(p, OUTPUT);
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);
  motores(0, 0);
  analogSetPinAttenuation(PINO_BATERIA, ADC_11db);

  LittleFS.begin(true);   // formata na primeira vez
  prefs.begin("carro", false);

  sessao = prefs.getULong("boots", 0) + 1;
  prefs.putULong("boots", sessao);
  baseLigado = prefs.getULong("tl", 0);
  baseAndando = prefs.getULong("ta", 0);
  totalVoltas = prefs.getULong("tv", 0);
  vezesLigado = prefs.getULong("lig", 0);
  recorde = prefs.getULong("rec", 0);
  cfg.vb = prefs.getInt("vb", cfg.vb);
  cfg.rt = prefs.getInt("rt2", cfg.rt);
  cfg.cv = prefs.getInt("cv2", cfg.cv);
  cfg.inv = prefs.getInt("inv2", cfg.inv);
  cfg.bal = prefs.getInt("bal", cfg.bal);
  cfg.pm = prefs.getInt("pm", cfg.pm);
  cfg.vm = prefs.getInt("vm", cfg.vm);
  carregarCalibracao();

  logar("SIS", "Boot #" + String(sessao) + ": " + motivoReset());

  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(WIFI_NOME, WIFI_SENHA))
    logar("CON", "Rede " + String(WIFI_NOME) + " criada em " + WiFi.softAPIP().toString());
  else
    logar("CON", "Falha ao criar a rede Wi-Fi");

  server.on("/", []() { server.send_P(200, "text/html", PAGINA); });
  server.on("/s", rotaStatus);
  server.on("/c", rotaComando);
  server.on("/m", rotaModo);
  server.on("/p", rotaConfig);
  server.on("/l", rotaLogs);
  server.on("/lc", rotaApagarLogs);
  server.on("/t", rotaHora);
  server.on("/rr", rotaZerarRecorde);
  server.on("/zo", rotaZerarOdometro);
  server.on("/mt", rotaTesteMotor);
  server.onNotFound([]() { server.send(404, "text/plain", "nao encontrado"); });
  server.begin();
}

void loop() {
  // atende o painel ANTES de ler o relogio: os comandos gravam millis() e "agora"
  // nunca pode ser menor que eles (a conta sem sinal viraria um numero gigante)
  server.handleClient();

  static unsigned long anterior = millis();
  unsigned long agora = millis();
  if (naBateria) {
    ligadoMs += agora - anterior;
    if (andando) andandoMs += agora - anterior;
  }
  anterior = agora;

  lerSensores();

  switch (modo) {
    case AUTO:       seguirLinha(agora); break;
    case CALIBRANDO: calibrar(agora); break;
    case MANUAL:
      if (agora - ultimoCmd > PARADA_SEGURANCA && andando) {
        Serial.printf("[MAN] parada de seguranca: %lu ms sem comando\n", agora - ultimoCmd);
        motores(0, 0);
      }
      break;
    default:
      if (testeAte && agora > testeAte) {
        testeAte = 0;
        motores(0, 0);
      }
      break;
  }

  if (salvarCfgEm && agora > salvarCfgEm) {
    salvarCfgEm = 0;
    prefs.putInt("vb", cfg.vb);
    prefs.putInt("rt2", cfg.rt);
    prefs.putInt("cv2", cfg.cv);
    prefs.putInt("inv2", cfg.inv);
    prefs.putInt("bal", cfg.bal);
    prefs.putInt("pm", cfg.pm);
    prefs.putInt("vm", cfg.vm);
    logar("PID", "Ajuste salvo: vel " + String(cfg.vb) + ", folga centro " + String(cfg.rt) +
                 ", curva " + String(cfg.cv) + "%, balanço " + String(cfg.bal) +
                 "%, força mín " + String(cfg.pm) + ", manual " + String(cfg.vm) +
                 (cfg.inv ? ", direção invertida" : ""));
  }

  tarefasPorSegundo(agora);
  comandoSerial();

  // LED: aceso andando sozinho, piscando parado
  digitalWrite(PINO_LED, modo == AUTO || modo == MANUAL || (modo == PARADO && (agora / 500) % 2));
}