# Hardware

## Plataforma de prototipagem

**ESP32 DevKitC** (Espressif Systems): SoC com dois núcleos Xtensa LX6 de até
240 MHz, 520 KB de SRAM, conversor A/D de 12 bits e conectividade Wi-Fi
802.11 b/g/n e Bluetooth integradas. A escolha frente a Arduino Uno, NodeMCU
(ESP8266) e Raspberry Pi está justificada no artigo: Wi-Fi nativo (dispensa
módulos externos e Firmata), mais memória/GPIOs que o ESP8266 e custo/consumo
muito menores que o Raspberry Pi, decisivo para um dispositivo vestível.

## Lista de materiais

| Componente | Qtde. | Função | Conexão |
|---|---|---|---|
| ESP32 DevKitC | 1 | Processamento + Wi-Fi/MQTT | — |
| LCD 16x2 com módulo I2C | 1 | Exibição local (glicemia, tendência, dose) | SDA=GPIO21, SCL=GPIO22, VCC=5V |
| Botão tátil | 2 | Registro de insulina rápida/basal | GPIO25, GPIO26 (pull-up interno) |
| Potenciômetro 10 kΩ | 1 | Sensor analógico de seleção de dose (1–30 UI) | GPIO34 (ADC), 3V3 |
| Buzzer magnético | 1 | Alarmes sonoros e confirmações | GPIO27 |
| LED 5 mm (verde/amarelo/vermelho) | 3 | Estado glicêmico | GPIO32/33/14, resistores 220 Ω |
| Protoboard e jumpers | — | Interligação (montagem no simulador) | — |

## Sensor (cenário real vs. simulação)

Na implementação real, o elemento sensor é um CGM subcutâneo comercial
(referência: FreeStyle Libre 2 — disco de 35×5 mm, filamento de ~5 mm com
glicose-oxidase, leitura por NFC ISO 15693 e alarmes por BLE, 14 dias de uso).
O ESP32 atuaria como gateway NFC/BLE→MQTT. Na simulação, o sensor é
substituído por um **sensor virtual**: o firmware reproduz, em tempo acelerado
(5 s = 15 min), a janela de 72 h de dados reais do paciente 1006 do dataset
ShanghaiT1DM embarcada em `firmware/cgm_data.h` (288 leituras, 43–272 mg/dL,
12 doses de insulina).

## Montagem

O circuito completo está em `firmware/diagram.json` (abre direto no Wokwi).
Diagramas em `docs/img/` e no artigo (modelo de montagem e fluxograma).

Para uma versão física vestível: variantes compactas (ESP32-C3 Super Mini,
22,5×18 mm, ou módulo ESP32-PICO-MINI, 13,2×16,6 mm) com bateria Li-Po,
display OLED 0,96" e um único botão, conforme discutido no artigo.
