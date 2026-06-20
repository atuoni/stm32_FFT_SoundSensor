/**
 ******************************************************************************
 * @file    tft.h
 * @brief 	LCD driver para HX8347G/ST7789 customizado.
 *			Nome original hx8347g.c
 *
 * @author MCD Application Team
 * @date 06-May-2014
 * @version V1.0.0
 ******************************************************************************
 * @attention
 *
 *<h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
 *
 *Redistribution and use in source and binary forms, with or without modification,
 *are permitted provided that the following conditions are met:
 *1. Redistributions of source code must retain the above copyright notice,
 *this list of conditions and the following disclaimer.
 *2. Redistributions in binary form must reproduce the above copyright notice,
 *this list of conditions and the following disclaimer in the documentation
 *and/or other materials provided with the distribution.
 *3. Neither the name of STMicroelectronics nor the names of its contributors
 *may be used to endorse or promote products derived from this software
 *without specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @details	Modificado e customizado a apartir de junho de 2020 por
 *			com propósitos educacionais.
 *			Este arquivo está compatibilizado no o kit ST NUCLEO-F446RE.
 * @author	Leandro Poloni Dantas
 * @date	22/06/2020
 * @details	Histórico de Modificações: 
 * @date	22/06/2020
 * @details	A função reset passa a consider um delay maior antes de
 *			escrever em um registrador, segui recomendações do presentes
 *			no manual do driver ILI9340.
 * @date	01/07/2020
 * @details	Algumas funções foram criadas para facilitar a
 *			integração com uma câmera OV7670.
 * @date	16/07/2020
 * @details	Uma função de inicialização de GPIOs foi
 *			incluida para evitar o uso do CubeMX.
 * @date	16/07/2020
 * @details	Uma função readID() ganhou uma macro mais
 *			intuitiva tft_readID().
 * @date	16/07/2020
 * @details	Correção na macro de acesso a memória
 *			(#define pgm_read_word(addr) (*(const unsigned short *)(addr)),
 *			antes fazia referência a apenas 16 bits do endereço,
 *			agora é capaz de ler toda a memória. Esse falha estava
 *			corrompendo o acesso a tabela de fontes LCD quando tinha
 *			imagens salvas em flash e o endereço passava de 0xFFFF.
 * @date	27/08/2020
 * @details	Criação de uma nova função para escrita de caracteres com
 *			limpeza automática do fundo (write_fillbackground).
 *			Criação de novas funções para o usuário com chamada para nova função write:
 *			tft_printnewtstr_bg, tft_printstr_bg.
 *			Criação da função para troca da cor de fundo do texto tft_setTextBackColor.
 *			A documentação de algumas funções foi melhorada ou criada.
 * @date	21/08/2023
 * @details	Ajuste da descrição do nome dos arquivo para tft.h.
 *			Passagem dos protótipos das funções que estavam no arquivo functions.h para
 *			este arquivo.
 *			Melhora na descrição das funções no estilo Doxygen.
 *			Acrescentado o prefixo tft_ em todas as funções pública e a diretiva
 *			static em todas as funções privadas.
 *			O arquivo function.h não é mais necessário, todos os protótipos de funções
 *			foram passados para o arquivo tft.h.
 * @date	29/08/2023
 * @details	Correção nas variáveis cursor_x e cursor_y. Passaram de 8 bits
 *			para 16 bits. Isso permite escrever em toda a tela.
 * @date	19/02/2025
 * @details	Acrestando novo teste para identificar ID de display do
 *			tipo ST7789.
 * @date	09/06/2025
 * @details	Alteração na função tft_setFont, que passou a coletar o maior
 *			yOffset. Esse valor foi aplicado na correção da função tft_write_fillbackground,
 *			que trabalhava apenas com fontes simétricas para definir a área de background.
 *			Correção nas funções tft_setTextColor e tft_setTextBackColor, estavam
 *			atribuindo as cores invertidas.
 * @date	26/07/2025
 * @details	Biblioteca 100% compatibilizada com o formato Doxigen de comentários e documentação
 *			de código.
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TFT_H
#define __TFT_H

#ifdef __cplusplus
extern "C" {
#endif 

/* Includes -----------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include "fonts.h"
#include "user_setting.h"

/* Contantes e macros -------------------------------------------------------*/
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define true	1
#define false	0

#define MIPI_DCS_REV1   (1<<0)
#define AUTO_READINC    (1<<1)
#define READ_BGR        (1<<2)
#define READ_LOWHIGH    (1<<3)
#define READ_24BITS     (1<<4)
#define XSA_XEA_16BIT   (1<<5)
#define READ_NODUMMY    (1<<6)
#define INVERT_GS       (1<<8)
#define INVERT_SS       (1<<9)
#define MV_AXIS         (1<<10)
#define INVERT_RGB      (1<<11)
#define REV_SCREEN      (1<<12)
#define FLIP_VERT       (1<<13)
#define FLIP_HORIZ      (1<<14)

/* Protótipos de funções ---------------------------------------------------*/
uint16_t tft_color565(uint8_t r, uint8_t g, uint8_t b);
uint16_t tft_readPixel(int16_t x, int16_t y);
void tft_writeCmdData(uint16_t cmd, uint16_t dat);
int16_t tft_readGRAM(int16_t x, int16_t y, uint16_t * block, int16_t w, int16_t h);
void tft_reset(void);
void tft_init(uint16_t ID);
uint16_t tft_readID(void);
void tft_setRotation(uint8_t r);
uint8_t tft_getRotation (void);
void tft_drawPixel(int16_t x, int16_t y, uint16_t color);
void tft_vertScroll(int16_t top, int16_t scrollines, int16_t offset);
void tft_invertDisplay(uint8_t i);
void tft_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void tft_drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void tft_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void tft_drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void tft_drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color);
void tft_drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void tft_fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color);
void tft_fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void tft_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
void tft_fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
void tft_drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void tft_fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void tft_fillScreen(uint16_t color);
void tft_drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h);

/* Funções de teste ---------------------------------------------------------*/
void tft_testfillScreen();
void tft_testLines(uint16_t color);
void tft_testFastLines(uint16_t color1, uint16_t color2);
void tft_testRects(uint16_t color);
void tft_testFilledRects(uint16_t color1, uint16_t color2);
void tft_testFilledCircles(uint8_t radius, uint16_t color) ;
void tft_testCircles(uint8_t radius, uint16_t color);
void tft_testTriangles();
void tft_testFilledTriangles();
void tft_testRoundRects();
void tft_testFilledRoundRects();

/* Funções de texto ---------------------------------------------------------*/
void tft_drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);
size_t tft_write(uint8_t c);
size_t tft_write_fillbackground(uint8_t c);
void tft_setFont(const GFXfont *f);
void tft_charBounds(char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy);
void tft_getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
void tft_printnewtstr (int row, uint16_t txtcolor, const GFXfont *f, uint8_t txtsize, uint8_t *str);
void tft_printnewtstr_bc(int row, uint16_t txtcolor, uint16_t txtbackcolor, const GFXfont *f, uint8_t txtsize, uint8_t *str);
void tft_printstr (uint8_t *str);
void tft_printstr_bc (uint8_t *str);
void tft_setTextWrap(uint8_t w);
void tft_setTextColor (uint16_t color);
void tft_setTextBackColor (uint16_t color);
void tft_setTextSize (uint8_t size);
void tft_setCursor(int16_t x, int16_t y);
void tft_scrollup (uint16_t speed);
void tft_scrolldown (uint16_t speed);

/* Funções de interação com a câmera e SD Card ------------------------------*/
/* Testadas com LCD TFT ILI9340 */
void tft_desenhaPixel(uint16_t pixel);
void tft_inicioDados(void);
void tft_fimDados(void);
void tft_setAddrWindow(int16_t x, int16_t y, int16_t x1, int16_t y1);

/* Função de inicialização de GPIOs -----------------------------------------*/
void tft_gpio_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __TFT_H */

