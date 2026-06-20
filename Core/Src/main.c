/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "mbedtls.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fonts.h"  // fontes para o display TFT
#include "tft.h"    // funções do display TFT
#include "user_setting.h"  // configurações do usuário
#include <math.h>       // funções matemáticas (sin, cos, log...)
#include <stdlib.h>     // rand(), srand()
#include <stdio.h>    // sprintf() para formatar strings
#include "arm_math.h"   // biblioteca CMSIS-DSP para FFT
#include "arm_const_structs.h"   // estruturas prontas da FFT (ex: 1024 pontos)
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// Define o tamanho total do buffer
// 2048 = 1024 amostras reais + 1024 zeros (parte imaginária)
// A FFT complexa precisa de pares [real, imaginário]
#define TEST_LENGTH_SAMPLES 2048
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1; // estrutura de controle do ADC1

TIM_HandleTypeDef htim1;  // estrutura de controle do Timer1 (usado pelo TFT)
TIM_HandleTypeDef htim3;  // estrutura de controle do Timer3 (trigger do ADC)

UART_HandleTypeDef huart2; // estrutura de controle da UART2 (debug serial)

/* USER CODE BEGIN PV */
// Armazena o ID do controlador do display TFT
// Cada modelo de display tem um ID diferente (ex: 0x9341 para ILI9341)
uint16_t ID=0;

// Flag setada pela ISR do botão
// 'volatile': o compilador não pode otimizar/cachear esta variável
// pois ela é modificada em contexto de interrupção
volatile uint8_t botao_pressionado = 0;

// Flag setada pela ISR do ADC quando 1024 amostras foram coletadas
// Sinaliza ao loop principal que pode processar os dados
volatile uint8_t adc_pronto = 0;

/* Variáveis para FFT dinâmico */
// Contador de amostras coletadas pelo ADC
// 'volatile' obriga o compilador a sempre ler da memória
// pois essa variável é modificada dentro de uma interrupção (ISR)
volatile uint32_t nAmostras = 0;

// Buffer onde a ISR do ADC armazena as amostras capturadas
// Tamanho 2048: posições pares = parte real, ímpares = 0 (imaginário)
float32_t dinamicInput_f32_10kHz[TEST_LENGTH_SAMPLES];


// Buffer de saída da FFT — armazena as magnitudes de cada bin
// Tamanho 1024 = metade de 2048 (apenas bins úteis)
// 'static' significa que só existe neste arquivo
static float32_t testOutput[TEST_LENGTH_SAMPLES/2];

/* ------------------------------------------------------------------
* Global variables for FFT Bin Example
* ------------------------------------------------------------------- */
uint32_t fftSize = 1024; // número de pontos da FFT
uint32_t ifftFlag = 0;   // 0 = FFT direta, 1 = FFT inversa
uint32_t doBitReverse = 1;  // 1 = aplica bit-reversal (necessário para o algoritmo)
uint32_t testIndex = 0;   // bin onde foi encontrada a maior magnitude


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  // configura UART2 a 115200 baud (debug via USB-Serial)
  MX_USART2_UART_Init();
  // inicializa biblioteca de criptografia (não usada ativamente)
  MX_MBEDTLS_Init();
  // configura ADC1: 12 bits, trigger TIM3, canal 13 (PC3)
  MX_ADC1_Init();
  // configura Timer3: prescaler=83, period=99 → 10kHz de trigger
  MX_TIM3_Init();

  /* USER CODE BEGIN 2 */
  // Configura os pinos GPIO específicos do display TFT
  // (dados, controle, chip select, reset, etc.)
  tft_gpio_init();

  // Inicia o Timer1 — usado internamente pelo driver do TFT
  // para medir tempo de operações do display
  HAL_TIM_Base_Start(&htim1);

  // Lê o ID do controlador do display via SPI/paralelo

  // Retorna ex: 0x9341 para ILI9341, 0x7789 para ST7789
  ID = tft_readID();

  // Aguarda 100ms para o display estabilizar após leitura do ID
  HAL_Delay(100);

  // inicializa o display com base no ID lido
  tft_init(ID);

  // rotação 1 = landscape (320×240)
  tft_setRotation(1);

  // preenche a tela de preto
  tft_fillScreen(BLACK);

  // sem inversão de cores
  tft_invertDisplay(0);

  // Escreve mensagem inicial na tela
  // Parâmetros: posição Y=15, cor branca, fonte mono12x7bold, escala 1
  tft_printnewtstr(15, WHITE, &mono12x7bold, 1, (uint8_t*)"Pressione o botao");


  //---------------Agora vai para o LCD----------------
  int32_t i;  // iterador dos loops de desenho
  int32_t amp; // amplitude em pixels calculada para cada amostra
  int32_t escala;  // fator de escala para ampliar o sinal no display
  int32_t yini;   // coordenada Y inicial da barra vertical
  int32_t yfinal;  // coordenada Y final da barra vertical
  int32_t ycentro;  // coordenada Y do centro do gráfico
  char num[30];
  int32_t size;

  // Cópia das primeiras 320 amostras reais do sinal no tempo
  // Salva ANTES da FFT, pois a FFT modifica o buffer in-place
  // 320 = largura do display em pixels (um ponto por coluna)
  float32_t espelho_pre_processamento[320];
  // Maior magnitude encontrada no espectro
  // Usada para normalizar o gráfico de frequência (maior barra = tela cheia)
  float32_t maxValue;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  // Verifica se a flag foi setada pela ISR do botão
	  // Só entra aqui quando o usuário pressionar o botão físico
	  if(botao_pressionado)
	  {
		  // Limpa a flag imediatamente para permitir nova detecção futura
		  botao_pressionado = 0;

		  // Limpa a tela e informa o usuário que a captura está em andamento
		  tft_fillScreen(BLACK);
		  tft_printnewtstr(15, WHITE, &mono12x7bold, 1, (uint8_t*)"Adquirindo...");


		  nAmostras  = 0;  // Zera o contador de amostras antes de iniciar nova captura
		  adc_pronto = 0;   // Garante que a flag de conclusão está limpa

	      // Habilita o ADC1 em modo interrupção
	      // A cada conversão completa, chama HAL_ADC_ConvCpltCallback automaticamente
		  HAL_ADC_Start_IT(&hadc1);

		  // Inicia o Timer3 no canal 1
		  // Gera evento TRGO (trigger output) a cada 100 ciclos de 1MHz = 10kHz
		  // Cada TRGO dispara uma conversão no ADC1
		  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

		  // Busy-wait: fica aqui até a ISR do ADC setar adc_pronto = 1
		  // Isso ocorre após 1024 conversões (≈ 0,1 segundo a 10kHz)
		  while(!adc_pronto);

		  // Copia as 320 primeiras amostras REAIS para exibição no domínio do tempo
		  // i*2 pula as posições ímpares (parte imaginária = 0)
		  // Necessário porque a FFT vai modificar dinamicInput_f32_10kHz in-place
		  for(i = 0; i < 320; i++)
			  espelho_pre_processamento[i] = dinamicInput_f32_10kHz[i*2];

		  // Calcula a média aritmética das amostras reais
		  // Representa o offset DC do sinal (tensão de polarização do microfone)
		  float32_t media = 0.0f;
		  for(i = 0; i < (int32_t)fftSize; i++)
			  media += dinamicInput_f32_10kHz[i * 2];
		  media /= (float32_t)fftSize;


		  // Subtrai a média de cada amostra
		  // Centraliza o sinal em zero, evitando pico espúrio no bin 0 da FFT
		  for(i = 0; i < (int32_t)fftSize; i++)
			  dinamicInput_f32_10kHz[i * 2] -= media;

	      // Executa a FFT complexa de 1024 pontos
	      // &arm_cfft_sR_f32_len1024: estrutura com coeficientes (twiddle factors) pré-calculados
	      // dinamicInput_f32_10kHz: buffer de entrada/saída (modificado in-place)
	      // ifftFlag = 0: FFT direta (domínio do tempo → domínio da frequência)
	      // doBitReverse = 1: aplica reordenação bit-reversal (necessária pelo algoritmo)
		  arm_cfft_f32(&arm_cfft_sR_f32_len1024, dinamicInput_f32_10kHz, ifftFlag, doBitReverse);

		  // Calcula a magnitude de cada bin: sqrt(real² + imaginário²)
		  // Entrada: dinamicInput_f32_10kHz (saída complexa da FFT, 2048 floats)
		  // Saída: testOutput (1024 magnitudes, uma por bin)
		  arm_cmplx_mag_f32(dinamicInput_f32_10kHz, testOutput, fftSize);

	      // Encontra o valor máximo em testOutput
	      // maxValue: magnitude do bin dominante (frequência com mais energia)
	      // testIndex: índice do bin dominante
	      // Frequência dominante = testIndex * (fs / fftSize) = testIndex * 9,77 Hz
		  arm_max_f32(testOutput, fftSize, &maxValue, &testIndex);

		  // Se maxValue for zero (sem sinal), a divisão no gráfico causaria NaN/Inf
		  // Substitui por valor mínimo positivo para evitar erro
		  if(maxValue < 1e-6f)
			  maxValue = 1e-6f;

		  // Encontra o valor máximo em testOutput
		  // maxValue: magnitude do bin dominante (frequência com mais energia)
		  // testIndex: índice do bin dominante
		  // Frequência dominante = testIndex * (fs / fftSize) = testIndex * 9,77 Hz
		  arm_max_f32(testOutput, fftSize, &maxValue, &testIndex);


		  // ══════════════════════════════════════════════
		  // RESUMO DA AQUISIÇÃO — enviado uma vez por captura
		  // ══════════════════════════════════════════════
		  float32_t freq_dominante = (float32_t)testIndex * 10000.0f / 1024.0f;
		  // Converte o índice do bin para frequência real em Hz
		  // fórmula: bin * (taxa_amostragem / numero_pontos_fft)

		  char resumo[100];
		  int rsize = sprintf(resumo,
		      "\r\n[t=%lu] FREQ_DOMINANTE=%.1fHz MAG=%.4f\r\n",
		      HAL_GetTick(),      // tempo em ms desde o boot — identifica QUANDO foi essa captura
		      freq_dominante,     // frequência calculada do bin de pico
		      maxValue);          // magnitude (intensidade) do pico
		  HAL_UART_Transmit(&huart2, (uint8_t*)resumo, rsize, 200);
		  // ══════════════════════════════════════════════

		  tft_fillScreen(BLACK);
		  // ... continua o código do display do domínio do tempo

		  // Limpa a tela antes de desenhar o novo gráfico
		  tft_fillScreen(BLACK);

		  // Escreve o título na linha 15 pixels do topo
		  // Cor branca, fonte mono12x7bold, escala 1
		  tft_printnewtstr (15, WHITE, &mono12x7bold, 1, (uint8_t *)"FFT-Dominio do Tempo");

	      // Fator de amplificação: multiplica o sinal normalizado (-1 a +1) por 500
	      // Ajuste empírico para o KY-037 — sinal de microfone tem amplitude pequena
		  escala = 500;

		  // Centro vertical do gráfico = metade da tela (240px / 2 = 120)
		  // Amostras positivas sobem, negativas descem a partir deste ponto
		  ycentro = 120;

		  // percorre cada coluna do display (320 pixels de largura)
		  for(i=0; i<320; i++)
		  {
			  // Converte a amostra float (-1.0 a +1.0) em pixels
			  // Exemplo: amostra = 0.05, escala = 3000 → amp = 150 pixels
			  amp = (int32_t)(espelho_pre_processamento[i]*escala);

			  // Envia o valor pela UART para debug no PC (ex: Serial Monitor)
			  //size = sprintf(num,"%d\r\n",amp);
			  //HAL_UART_Transmit(&huart2, num, size, 10);

			  /* Proteção de borda: limita amp ao espaço disponível na tela */
			  if(amp >  ycentro)
				  amp =  ycentro; // Limita para não ultrapassar a borda superior
			  if(amp < -ycentro)
				  amp = -ycentro; // Limita para não ultrapassar a borda inferior


			  // Calcula onde começa e termina a barra vertical
			  // Sinal positivo: barra sobe a partir do centro
			  // Sinal negativo: barra desce a partir do centro
			  if(amp>0)
			  {
				  // Sinal positivo: barra vai do centro para CIMA
				  // yini menor que ycentro (tela tem Y crescendo para baixo)
				  yini = ycentro - amp;
				  yfinal = ycentro;
			  }
			  else
			  {
				  // Sinal negativo: barra vai do centro para BAIXO
				  // -amp é positivo, então yfinal > ycentro
				  yini = ycentro;
				  yfinal = ycentro-amp;
			  }
			  // Desenha retângulo de 1 pixel de largura na coluna i
			  // Da linha yini até yfinal, na cor amarela
			  // O +1 garante que ao menos 1 pixel é desenhado quando amp=0
			  tft_fillRect(i, yini, 1, yfinal-yini+1, YELLOW);
			  //tft_drawPixel(i, ycentro-amp, YELLOW);
		  }
		  // Mantém o gráfico do domínio do tempo visível por 4 segundos
		  HAL_Delay(4000);
		  // Limpa a tela antes do próximo gráfico
		  tft_fillScreen(BLACK);

		  // Escreve o título do espectro de frequências
		  tft_printnewtstr (15, WHITE, &mono12x7bold, 1, (uint8_t *)"FFT-Dominio da Freq.");

		  // Escala do espectro: após normalização por maxValue, amp vai de 0 a 150 pixels
		  // Ajuste conforme necessário para ocupar bem a tela verticalmente
		  escala = 150;

		  // Base das barras perto da borda inferior da tela (altura = 240px)
		  // Barras crescem para cima a partir deste ponto
		  ycentro = 230;

		  // ══════════════════════════════════════════════
		  // CABEÇALHO CSV — identifica as colunas dos dados que virão
		  // ══════════════════════════════════════════════
		  char header[30];
		  int hsize = sprintf(header, "freq_hz,magnitude\r\n");
		  HAL_UART_Transmit(&huart2, (uint8_t*)header, hsize, 100);
		  // ══════════════════════════════════════════════

		  // Percorre 320 bins de frequência (dos 1024 disponíveis, exibe os primeiros 320)
		  // Cada bin corresponde a: i * 10000 / 1024 ≈ i * 9,77 Hz
		  // Bin 320 ≈ 3125 Hz — cobre a faixa audível relevante do microfone
		  for(i=0; i<320; i++)
		  {
			  // Normaliza a magnitude do bin pelo valor máximo
			  // testOutput[i] / maxValue → valor entre 0.0 e 1.0
			  // Multiplica por escala para converter em pixels
			  // O bin dominante terá sempre amp = escala (barra máxima)
			  amp = (int32_t)(testOutput[i]*escala/maxValue);
			  //size = sprintf(num,"%d\r\n",amp);
			  //HAL_UART_Transmit(&huart2, num, size, 10);

			  /* Proteção de borda */
			  // Limita para não ultrapassar a borda superior da tela
			  if(amp > ycentro)
				  amp = ycentro;
			  // Magnitudes são sempre positivas, mas garante que não há valor negativo
			  if(amp < 0)
				  amp = 0;

			  // ══════════════════════════════════════════════
			  // CALCULA A FREQUÊNCIA REAL DESTE BIN
			  // ══════════════════════════════════════════════
			  float32_t freq_hz = (float32_t)i * 10000.0f / 1024.0f;
			  // i = índice do bin (0 a 319)
			  // 10000 = taxa de amostragem em Hz
			  // 1024 = número de pontos da FFT
			  // Resultado: cada bin equivale a ~9,77 Hz

			  // ══════════════════════════════════════════════
			  // ENVIA A LINHA CSV: frequência,magnitude
			  // ══════════════════════════════════════════════
			  size = sprintf(num, "%.1f, %6f\r\n", freq_hz, testOutput[i]);
			  // %.1f = frequência com 1 casa decimal (ex: 996.1)
			  // %ld  = amplitude em pixels (long int)
			  HAL_UART_Transmit(&huart2, (uint8_t*)num, size, 10);
			  // ══════════════════════════════════════════════

			  // Barras sempre crescem de ycentro (base) para cima
			  yini   = ycentro - amp;
			  yfinal = ycentro;

			  // Desenha a barra do espectro na coluna i, cor amarela
			  tft_fillRect(i, yini, 1, yfinal-yini+1, YELLOW);
		  }
		  // Aguarda 4 segundos com o espectro visível
		  HAL_Delay(4000);
		  //Limpa a linha de texto da tela
		  tft_fillRect(0, 0, 320, 22, BLACK);
		  //escreve texto para pressionar o botão para nova aquisição de dados
		  tft_printnewtstr(15, WHITE, &mono12x7bold, 1, (uint8_t*)"Pressione o botao...");
	  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 84-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xFFFF-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 84-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_8
                          |GPIO_PIN_9|GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PC1 PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : Start_BT_Pin */
  GPIO_InitStruct.Pin = Start_BT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Start_BT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA4 PA8
                           PA9 PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_8
                          |GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB10 PB3 PB4
                           PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// Chamada automaticamente pelo HAL quando o ADC termina uma conversão
// Contexto: interrupção — deve ser rápida, sem HAL_Delay ou operações lentas
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hacd)
{
	uint32_t vADC;

	// Lê o resultado da conversão: valor de 0 a 4095 (12 bits)
	// 0    → 0V (GND)
	// 2048 → 1,65V (meio da faixa)
	// 4095 → 3,3V (VCC)
	vADC = HAL_ADC_GetValue(&hadc1);

	// Normaliza o valor para a faixa -1.0 a +1.0
	// Subtrai 2048 para centralizar em zero (remove o offset DC de 1,65V)
	// Divide por 2048 para normalizar a amplitude
	// Armazena na posição REAL do buffer (índice par)
	dinamicInput_f32_10kHz[nAmostras*2] = ((float32_t)vADC - 2048.0f) / 2048.0f;


	// Parte IMAGINÁRIA = 0 (sinal do microfone é real, não complexo)
	// A FFT complexa exige pares [real, imaginário] mesmo para sinais reais
	dinamicInput_f32_10kHz[nAmostras*2+1] = 0.0F;

	// Incrementa o contador e verifica se coletou todas as amostras necessárias
	if(++nAmostras >= fftSize)
	{
		// Para o ADC — impede novas conversões
		// Sem isso o ADC continuaria convertendo e sobrescreveria o buffer
		if( HAL_ADC_Stop_IT(&hadc1) != HAL_OK)
		{
			Error_Handler();
		}

		 // Para o Timer3 — cessa a geração de triggers para o ADC
		if(HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1) != HAL_OK)
		{
			Error_Handler();
		}
		// Sinaliza ao loop principal que os dados estão prontos para processamento
		adc_pronto = 1;
	}
}
// Chamada automaticamente pelo HAL quando ocorre interrupção em qualquer pino EXTI
// Contexto: interrupção — deve ser rápida
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Variável estática: mantém seu valor entre chamadas (não é reiniciada)
	// Armazena o tempo do último pressionamento válido
    static uint32_t ultimo_tick = 0;
    // Verifica se a interrupção veio do pino do botão Start_BT
    // Necessário pois todos os pinos EXTI chamam a mesma função
    if(GPIO_Pin == Start_BT_Pin)
    {
    	// Obtém o tempo atual em milissegundos desde o boot
        uint32_t agora = HAL_GetTick();
        // Evita múltiplos disparos por bounce mecânico do botão
        if(agora - ultimo_tick > 200)  // 200ms de debounce
        {
        	// Sinaliza ao loop principal que o botão foi pressionado
            botao_pressionado = 1;

            // Atualiza o tempo do último pressionamento válido
            ultimo_tick = agora;
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
