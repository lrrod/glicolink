# Monitor remoto de glicemia (CGM) com diário de insulina — IoT/MQTT

Projeto da disciplina **Objetos Inteligentes Conectados** (Universidade Presbiteriana Mackenzie).
Autor: **Luiz Rodrigo Alves Vergino**.

Sistema IoT aberto, de vocação vestível, que atua como gateway de monitoramento
contínuo de glicose (CGM): reproduz leituras reais de um sensor CGM (dados do
dataset público ShanghaiT1DM), calcula setas de tendência, dispara alertas
locais (LCD, LEDs, buzzer) e remotos, integra um diário digital de insulina e
aceita **comandos de atuadores pela internet**, tudo via **protocolo MQTT**
sobre TCP/IP. Na implementação real, o nó ESP32 leria um sensor subcutâneo
comercial (ex.: FreeStyle Libre 2) por NFC/BLE; nesta fase, o sensor é
substituído por um sensor virtual alimentado com dados reais de pacientes e o
hardware roda no simulador Wokwi, com todo o restante do fluxo (broker,
dashboard, notificações) real e executável.

**Simulação online (Wokwi):** https://wokwi.com/projects/479241117092881409

## Como reproduzir

1. Abra o projeto no Wokwi (link acima) ou crie um projeto ESP32 (Arduino) e
   copie `firmware/sketch.ino`, `firmware/cgm_data.h` e `firmware/diagram.json`.
   Adicione as bibliotecas `PubSubClient` e `LiquidCrystal I2C` (Library Manager).
2. Rode a simulação. O ESP32 conecta ao Wi-Fi simulado e ao broker público
   `broker.hivemq.com:1883`, e passa a publicar nos tópicos abaixo.
3. Observe as mensagens com qualquer cliente MQTT, por exemplo:
   `mosquitto_sub -h broker.hivemq.com -t "mackenzie/cgm-iot/paciente-1006/#" -v`
4. Dashboard: instale Node-RED com `node-red-dashboard`, importe
   `nodered/flow.json` e faça o deploy. Interface em `http://localhost:1880/ui`,
   com curva glicêmica ao vivo, alertas, diário de insulina e **botões que
   acionam o buzzer/LED do ESP32 via MQTT**, exibindo o tempo de resposta.
5. Medições de latência: com a simulação rodando,
   `python3 ferramentas/medir_latencia.py 4` (requer `pip install paho-mqtt`).

Interaja com o protótipo: o potenciômetro escolhe a dose (1 a 30 UI), o botão
azul registra insulina rápida e o verde registra basal; os LEDs indicam o
estado glicêmico (verde = alvo, amarelo = atenção/preditivo, vermelho =
hipo/hiperglicemia) e o buzzer soa nos alertas.

## Hardware

Plataforma: **ESP32 DevKitC** (Wi-Fi 802.11 b/g/n integrado, dois núcleos a
240 MHz, ADC de 12 bits). Periféricos: display LCD 16x2 com interface I2C
(GPIO 21/22), dois botões táteis (GPIO 25/26, pull-up interno), potenciômetro
10 kΩ (GPIO 34), buzzer (GPIO 27) e três LEDs com resistores de 220 Ω
(GPIO 32/33/14). Detalhes, diagrama de montagem e esquema em
[`docs/hardware.md`](docs/hardware.md).

## Comunicação (MQTT sobre TCP/IP)

O módulo de comunicação é o rádio Wi-Fi integrado do próprio ESP32 com a pilha
TCP/IP do ESP-IDF; o cliente MQTT é a biblioteca PubSubClient (sem necessidade
de Arduino+Firmata ou conversores serial-USB). Tópicos, formato das mensagens
JSON, QoS, mensagem retida e Last Will estão documentados em
[`docs/protocolo-mqtt.md`](docs/protocolo-mqtt.md).

## Estrutura do repositório

| Pasta | Conteúdo |
|---|---|
| `firmware/` | Código-fonte do ESP32 (Arduino/C++), dados do sensor virtual e circuito do Wokwi |
| `nodered/` | Fluxo do dashboard e do controle remoto (importável no Node-RED) |
| `dados/` | Janela de 72 h de dados reais de CGM e insulina (paciente 1006, ShanghaiT1DM) |
| `ferramentas/` | Script de medição dos tempos de resposta via MQTT |
| `docs/` | Documentação de hardware, protocolo e imagens |

## Fonte dos dados

ZHAO, Q. et al. Chinese diabetes datasets for data-driven machine learning.
*Scientific Data*, v. 10, art. 35, 2023. Dataset: https://doi.org/10.6084/m9.figshare.20444397
