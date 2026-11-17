#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Medidor de tempos de resposta do protótipo via MQTT.

Mede, pelo caminho real da internet (broker público):
1) SENSOR -> PLATAFORMA: intervalo entre chegadas das leituras de glicemia
   publicadas pelo ESP32 (esperado: 5000 ms na simulação acelerada); o desvio
   em relação a 5000 ms reflete a latência/jitter fim a fim da publicação.
2) COMANDO -> ATUADOR: tempo de ida e volta (RTT) entre a publicação de um
   comando no tópico de comando e o recebimento da confirmação (ack)
   publicada pelo ESP32 após acionar o atuador.

Uso: com a simulação rodando no Wokwi, execute
    python3 medir_latencia.py [n_medicoes]
Resultado: tabela no terminal + arquivo medicoes.csv
"""
import json, sys, time, statistics, csv
import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883
BASE = "mackenzie/cgm-iot/paciente-1006"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 4

t_glic = []          # timestamps de chegada das leituras
rtt_por_atuador = {"buzzer": [], "led_amarelo": []}
pendentes = {}       # id -> (t_envio, atuador)

def on_connect(cl, userdata, flags, rc, props=None):
    cl.subscribe([(f"{BASE}/glicemia", 1), (f"{BASE}/ack", 1)])
    print(f"Conectado a {BROKER}; aguardando mensagens do protótipo...")

def on_message(cl, userdata, m):
    global t_glic
    agora = time.time()
    if m.topic.endswith("/glicemia"):
        t_glic.append(agora)
        d = json.loads(m.payload)
        print(f"  leitura #{len(t_glic)}: {d['glicemia']} mg/dL recebida")
    elif m.topic.endswith("/ack"):
        d = json.loads(m.payload)
        if d.get("id") in pendentes:
            t0, atuador = pendentes.pop(d["id"])
            rtt = (agora - t0) * 1000
            rtt_por_atuador[atuador].append(rtt)
            print(f"  ack {atuador}: RTT = {rtt:.0f} ms (ação local no ESP32: {d.get('t_acao_ms','?')} ms)")

cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
cli.on_connect = on_connect
cli.on_message = on_message
cli.connect(BROKER, PORT, 60)
cli.loop_start()
time.sleep(2)

# ---- 1) sensor -> plataforma: espera N+1 leituras ----
print(f"\n[1/2] Medindo chegada de {N+1} leituras do sensor (aprox. {5*(N+1)} s)...")
while len(t_glic) < N + 1:
    time.sleep(0.2)
intervalos = [(t_glic[i+1]-t_glic[i])*1000 for i in range(N)]
desvios = [abs(dt - 5000) for dt in intervalos]

# ---- 2) comando -> atuador: N comandos por atuador ----
print(f"\n[2/2] Enviando {N} comandos por atuador...")
for atuador in rtt_por_atuador:
    for i in range(N):
        cid = int(time.time()*1000)
        pendentes[cid] = (time.time(), atuador)
        cli.publish(f"{BASE}/comando", json.dumps({"id": cid, "atuador": atuador}), qos=1)
        t_lim = time.time() + 10
        while cid in pendentes and time.time() < t_lim:
            time.sleep(0.05)
        time.sleep(1.0)

cli.loop_stop()

# ---- relatório ----
print("\n===== RESULTADOS =====")
linhas = []
for i, dt in enumerate(intervalos, 1):
    linhas.append([i, "Sensor CGM (leitura -> plataforma)", f"{dt:.0f} ms (desvio {dt-5000:+.0f} ms)"])
for atuador, rtts in rtt_por_atuador.items():
    for i, r in enumerate(rtts, 1):
        linhas.append([i, f"Atuador {atuador} (comando -> ação, RTT)", f"{r:.0f} ms"])
for num, item, valor in linhas:
    print(f"{num:>2}  {item:<45} {valor}")
print("\nMédias:")
print(f"  intervalo entre leituras: {statistics.mean(intervalos):.0f} ms (nominal 5000 ms); desvio médio {statistics.mean(desvios):.0f} ms")
for atuador, rtts in rtt_por_atuador.items():
    if rtts:
        print(f"  RTT {atuador}: {statistics.mean(rtts):.0f} ms (min {min(rtts):.0f} / max {max(rtts):.0f})")

with open("medicoes.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["medida", "sensor_atuador", "tempo"])
    w.writerows(linhas)
print("\nSalvo em medicoes.csv")
