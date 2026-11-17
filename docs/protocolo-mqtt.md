# Interfaces e protocolo de comunicação (MQTT)

## Módulo de comunicação

O nó usa o **rádio Wi-Fi 802.11 b/g/n integrado ao ESP32** com a pilha TCP/IP
(lwIP) do ESP-IDF. O cliente MQTT é a biblioteca **PubSubClient** (Arduino),
conectada ao broker público **broker.hivemq.com, porta 1883 (TCP)**. Não há
Firmata nem conversor serial-USB: o microcontrolador fala MQTT diretamente
com a internet.

- Conexão com **Last Will Testament**: ao conectar, o nó registra a mensagem
  "offline" (retida) no tópico de status; se cair, o broker a publica
  automaticamente. Conectado, publica "online" (retida).
- QoS 0 na publicação do firmware; assinantes usam QoS 1.
- Mensagens em JSON (~150–220 bytes).

## Tópicos

Base: `mackenzie/cgm-iot/paciente-1006/`

| Tópico | Direção | Conteúdo |
|---|---|---|
| `glicemia` | nó → nuvem | `{"paciente","glicemia","tendencia","taxa","projecao15","amostra","t_ms"}` |
| `alerta` | nó → nuvem | `{"paciente","tipo","glicemia","msg"}` (hipo, hipo_urgente, hiper, hiper_urgente, hipo_prevista) |
| `insulina` | nó → nuvem | `{"paciente","dose_UI","tipo","origem","glicemia","t_ms"}` (origem: manual/dataset) |
| `status` | nó → nuvem | `online`/`offline` (retida; LWT) |
| `comando` | nuvem → nó | `{"id","atuador"}` com atuador ∈ {buzzer, led_verde, led_amarelo, led_vermelho} |
| `ack` | nó → nuvem | `{"id","atuador","status","t_acao_ms"}` (confirma o acionamento; o id ecoado permite medir o RTT) |

## Fluxo de dados

ESP32 (Wi-Fi) → broker MQTT (TCP/IP, porta 1883) → assinantes:
Node-RED (dashboard + botões de comando + cálculo de tempo de resposta),
bot Telegram (notificações a cuidadores) e o script
`ferramentas/medir_latencia.py` (medições).

## Medição dos tempos de resposta

- **Sensor → plataforma:** intervalo entre chegadas das leituras no assinante
  (nominal 5000 ms na simulação acelerada); o desvio reflete latência + jitter
  do caminho nó→broker→assinante.
- **Comando → atuador:** RTT entre a publicação do comando e o recebimento do
  ack após o acionamento físico do atuador no nó. O campo `t_acao_ms` do ack
  reporta o tempo local de acionamento no ESP32.
