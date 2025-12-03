<h1 align="center"> Sistema IoT de Monitoramento de Planta 🌱 </h1>

<p align="center">
Projeto IoT completo utilizando ESP32, sensores ambientais e integração com o ThingSpeak para monitoramento inteligente de plantas.
</p>

<p align="center">
<a href="#-tecnologias">Tecnologias</a>&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;
<a href="#-projeto">Projeto</a>&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;
<a href="#-conexoes-do-projeto">Conexões do Projeto</a>&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;
<a href="#-licença">Licença</a>
</p>

<p align="center">
<img alt="License" src="https://img.shields.io/static/v1?label=license&message=MIT&color=49AA26&labelColor=000000">
</p>

<br>

## 🚀 Tecnologias

Esse projeto foi desenvolvido com:

- **ESP32**
- **Sensores analógicos e digitais**
  - LDR (luminosidade)
  - Sensor de umidade do solo
  - Sensor DHT11 (temperatura e umidade do ar)
  - Sensor de nível tipo boia
  - Módulo Relé
- **ThingSpeak (API + dashboard)**
- **C / Arduino IDE**
- **HTTPClient**
- **WiFi.h**
- **time.h para NTP**
- **chaves.h (Com nome e senha do Wi-Fi, e ID e chaves do ThingSpeak)**

<br>

## 💻 Projeto

Este sistema IoT monitora as condições de uma planta em tempo real utilizando um ESP32 e envia todos os dados automaticamente para o **ThingSpeak**, onde gráficos são gerados para acompanhamento.

Os sensores coletam:

- Luminosidade  
- Umidade do solo  
- Temperatura do ambiente  
- Umidade do ar  
- Nível de água no reservatório  
- Estado da bomba d’água  

A lógica de irrigação funciona automaticamente baseada em:

- Luminosidade
- Umidade do solo
- Horário do dia
- Presença de água no reservatório

<br>

## 🔌 Conexões do Projeto

### 🔧 **Conexões de Hardware**

```
ESP32 ───────────────────────── Sensores / Atuadores
--------------------------------------------------------------
GPIO 32  ─────────────── LDR (Sensor de Luminosidade)
GPIO 34  ─────────────── Sensor de Umidade do Solo (Analógico)
GPIO 23  ─────────────── DHT11 (Temperatura e Umidade)
GPIO 27  ─────────────── Sensor de Boia (Nível de Água)
GPIO 26  ─────────────── LED Indicador
GPIO 14  ─────────────── Módulo Relé → Bomba d'água

5V      ─────────────── Alimentação dos sensores
3.3V    ─────────────── DHT11 e LDR (se necessário)
GND     ─────────────── Terra comum para todos os componentes
```

### ☁️ **Conexões com o ThingSpeak**

O ThingSpeak recebe os dados via requisição HTTP:

```
http://api.thingspeak.com/update?api_key=WRITE_KEY
    &field1=Luminosidade
    &field2=UmidadeSolo
    &field3=UmidadeAr
    &field4=Temperatura
    &field5=EstadoBomba
    &field6=NivelAgua
```

Campos recomendados no ThingSpeak:

| Campo | Conteúdo |
|-------|----------|
| **field1** | Luminosidade |
| **field2** | Umidade do Solo |
| **field3** | Umidade do Ar (%) |
| **field4** | Temperatura (°C) |
| **field5** | Estado da Bomba (0 desligada / 1 ligada) |
| **field6** | Sensor de Boia (0 sem água / 1 com água) |

<br>

## :memo: Licença

Esse projeto está sob a licença MIT.

---

Feito com ♥ by Abyssal Roll 👋
