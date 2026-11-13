# LineBot

Robô seguidor de linha com controle remoto e painel web, feito com ESP32. Protótipo de AGV (Veículo Guiado Automaticamente) de baixo custo, com cerca de R$ 300 em peças.

Projeto do TDE de Conectividade de Sistemas Ciberfísicos, Engenharia de Software, PUCPR (2026).

**Equipe:** Victor Gabriel Ozorio, Rodrigo Zanetti, Murilo Proença, Arthur Camargo e Diego Wolnerr
**Orientador:** Prof. Fabio Garcez Bettio

## O que ele faz

- **Segue a linha sozinho.** Lê uma fita preta no chão com uma barra de 8 sensores infravermelhos, calibrados um a um. O objetivo é manter a linha nos sensores do meio: quanto mais ela vai para uma ponta, mais forte ele vira para o lado contrário.
- **Reencontra a linha.** Se perde a linha, dá ré até a barra passar por ela, gira no lugar até centralizá-la e volta a seguir reto. Se não encontrar, para e espera.
- **Controle remoto.** Pode ser dirigido pelo celular ou pelo teclado do PC (W, A, S, D). Se os comandos param de chegar por 1 s, ele para sozinho.
- **Painel web próprio.** O ESP32 cria a rede Wi-Fi `ESP32` e o painel abre em `http://192.168.4.1`, sem precisar de internet.
- **Histórico na memória flash.** Os eventos ficam gravados por sessão (LittleFS), com filtros e download em .txt.

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `firmware/carrinho_seguidor/` | Código do ESP32 para o Arduino IDE. `carrinho_seguidor.ino` é o programa e `pagina.h` é o painel web (HTML, CSS e JS) gravado na memória do ESP32. |
| `site/index.html` | Página 3D de apresentação: o LineBot e três robôs em que o projeto poderia se transformar. |

## Peças

ESP32 DevKit V1, ponte H L298N, barra de 8 sensores IR (tipo QTR-8), chassi com 4 motores DC, 4 pilhas AA para os motores, bateria 9 V com regulador para 5 V para o ESP32, botão liga/desliga, protoboard, jumpers e 4 resistores de 100 kΩ (divisor para medir a bateria).

## Ligações

| Módulo | Sinal | ESP32 |
|---|---|---|
| L298N (motor direito) | ENA, IN1, IN2 | GPIO 18, 16, 17 |
| L298N (motor esquerdo) | ENB, IN3, IN4 | GPIO 22, 19, 21 |
| L298N | GND | GND (terra comum, obrigatório) |
| Barra de sensores | D1 … D8 | GPIO 32, 33, 25, 26, 27, 14, 13, 4 |
| Barra de sensores | IR | GPIO 23 |
| Barra de sensores | VCC, GND | 3V3, GND (não ligar em 5 V) |
| Divisor de tensão | ponto médio (bateria ÷ 4) | GPIO 34 |

## Como gravar

1. No Arduino IDE, instale o pacote de placas **esp32** da Espressif (versão 3.x).
2. Abra `firmware/carrinho_seguidor/carrinho_seguidor.ino`.
3. Escolha a placa **ESP32 Dev Module** e a porta USB.
4. Clique em Carregar.
5. No celular ou no PC, conecte na rede `ESP32` (a senha está em `WIFI_SENHA` no começo do código) e abra `http://192.168.4.1`.

Os ajustes principais ficam no começo do `.ino`: pinos, inversão dos motores, faixas de branco e preto dos sensores e tempos da busca da linha.

## Página 3D

Abra `site/index.html` no navegador. É preciso internet para carregar o three.js e as fontes.
