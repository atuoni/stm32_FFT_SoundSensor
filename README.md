# Analisador de Espectro de Áudio — STM32F4 + KY-037

Sistema embarcado que captura áudio através de um sensor de som **KY-037**, processa o sinal com **FFT (CMSIS-DSP)** e exibe os domínios do tempo e da frequência em um display **TFT**, com captura disparada por botão e dados detalhados enviados via **UART**.

---

## 📋 Visão Geral

O projeto realiza:

1. **Aquisição de áudio** via ADC1 (canal PC3), disparada por Timer3 a 10kHz
2. **Pré-processamento**: remoção do componente DC (offset do microfone)
3. **FFT complexa de 1024 pontos** usando a biblioteca CMSIS-DSP
4. **Exibição gráfica** no display TFT: domínio do tempo e domínio da frequência
5. **Exportação de dados** via UART em formato CSV para análise externa

---

## 🔧 Hardware Necessário

| Componente | Função |
|---|---|
| STM32F4 (Discovery/Nucleo) | Microcontrolador principal |
| Display TFT (ILI9341 ou compatível) | Exibição gráfica dos gráficos |
| Sensor de som KY-037 | Captura do sinal de áudio analógico |
| Botão (push-button) | Dispara nova aquisição |
| Cabo USB-Serial | Debug e exportação de dados via UART |

### Conexões principais

| Sinal | Pino STM32 | Observação |
|---|---|---|
| KY-037 AO (saída analógica) | PC3 (ADC1_IN13) | Sinal de áudio |
| KY-037 VCC | 3.3V ou 5V | Conforme o módulo |
| KY-037 GND | GND | — |
| Botão Start | Conforme `Start_BT_Pin` no `main.h` | Pull-up interno habilitado |
| UART2 TX/RX | Conforme placa | 115200 baud, 8N1 |
| Display TFT | Barramento paralelo (PA, PB, PC) | Ver `MX_GPIO_Init()` |

---

## ⚙️ Configuração de Clock e Periféricos

- **Clock do sistema:** 84 MHz (HSI 16MHz → PLL)
- **Taxa de amostragem do ADC:** 10 kHz (TIM3: prescaler 84, period 100)
- **Resolução do ADC:** 12 bits (0–4095)
- **Tamanho da FFT:** 1024 pontos (cobre até ~5 kHz, dentro do limite de Nyquist)
- **UART:** 115200 baud, 8 bits, sem paridade, 1 stop bit

---

## 🚀 Como Usar

1. Compile e grave o firmware via STM32CubeIDE
2. Conecte um terminal serial (ex: PuTTY, RealTerm) a 115200 baud para visualizar os dados
3. A tela exibirá **"Pressione o botão"**
4. Pressione o botão físico conectado ao `Start_BT_Pin`
5. O sistema captura 1024 amostras (~100ms), processa a FFT e exibe:
   - **Domínio do tempo** (4 segundos)
   - **Domínio da frequência** (4 segundos)
6. Após o ciclo, a mensagem inicial retorna e uma nova captura pode ser iniciada

---

## 📊 Dados via Serial (UART)

A cada captura, os seguintes dados são enviados pela UART2:

### Resumo da aquisição
```
[t=15234] FREQ_DOMINANTE=996.1Hz MAG=0.4523
```

### Espectro completo em formato CSV
```
freq_hz,magnitude
0.0,0.002341
9.8,0.015234
19.5,0.008912
...
996.1,0.452300
```

Esses dados podem ser copiados diretamente para um arquivo `.csv` e analisados em Excel, Python (pandas/matplotlib) ou qualquer ferramenta de planilha.

---

## 🧮 Fundamentos Técnicos

### Cálculo de frequência por bin

```
resolução_em_Hz = taxa_de_amostragem / tamanho_da_FFT
                = 10000 / 1024
                ≈ 9,77 Hz por bin

frequência_do_bin[i] = i × 9,77 Hz
```

### Remoção do componente DC

O KY-037 gera um sinal com offset de tensão (tipicamente próximo de 1,65V). Antes da FFT, a média do sinal é calculada e subtraída de cada amostra, evitando um pico espúrio no bin 0 que mascararia as frequências reais de interesse.

---

## 📁 Estrutura do Projeto

```
.
├── Core/
│   ├── Inc/            # Headers (main.h, etc.)
│   └── Src/             # main.c e demais fontes
├── Drivers/              # HAL e CMSIS (gerados pelo CubeMX)
├── Middlewares/          # mbedTLS e outras bibliotecas
├── *.ioc                 # Configuração do STM32CubeMX
└── README.md
```

---

## ⚠️ Limitações Conhecidas

- A taxa de amostragem de 10kHz limita a análise a frequências até ~5kHz (Nyquist)
- O KY-037 é um sensor de baixo custo, com resposta não-linear e possível saturação em sons muito altos
- A FFT é recalculada apenas quando o botão é pressionado (não há streaming contínuo)
- Sem janelamento (window function) aplicado — pode haver vazamento espectral em sinais não periódicos dentro da janela de amostragem

---
## Video

Click the image below to watch the project demonstration.

<p align="center">
  <a href="https://youtu.be/nDVOEo5WYgo">
    <img src="https://img.youtube.com/vi/nDVOEo5WYgo/0.jpg" width="600">
  </a>
</p>

## Author

Amauri Tuoni

## 📄 Licença

Este projeto utiliza bibliotecas HAL e CMSIS-DSP da STMicroelectronics, sujeitas aos termos de licença da ST (ver arquivo `LICENSE` gerado pelo CubeMX).
