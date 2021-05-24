#include "stdbool.h"
#include "string.h"
#include "GPIO_STM32F10x.h"

#define addr(a) __attribute__((section(".ARM.__at_"a)))

uint8_t src[256] addr("0x20000000");
uint8_t dest[256] addr("0x20000100");
DMA_InitTypeDef dma;

int main(){
	//Overclock to 80 MHz
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
	
	RCC_PLLCmd(DISABLE);
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_10);
	RCC_PLLCmd(ENABLE);
	
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	
	//Setup the clocks
	RCC_HCLKConfig(RCC_SYSCLK_Div8);
	RCC_PCLK1Config(RCC_HCLK_Div1);
	RCC_PCLK2Config(RCC_HCLK_Div1);
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIOB->CRL = 0x33333333;
	//GPIOA->ODR = 0xffff;
	
	DMA_DeInit(DMA1_Channel5);
	dma.DMA_BufferSize = 256;//sizeof(src);
	dma.DMA_DIR = DMA_DIR_PeripheralDST;
	dma.DMA_M2M = DMA_M2M_Enable;
	dma.DMA_Priority = DMA_Priority_VeryHigh;
	dma.DMA_Mode = DMA_Mode_Normal;
	
	dma.DMA_MemoryBaseAddr = (uint32_t) &src+255;
	dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
	
	dma.DMA_PeripheralBaseAddr = (uint32_t)&(GPIOB->ODR);//(uint32_t) &dest;
	dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;//Byte;
	dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	
	DMA_Init(DMA1_Channel5, &dma);
	
	for (int i=0; i<256; i++){
		src[i]=i;
	}
	DMA_Cmd(DMA1_Channel5, ENABLE);
	while (true);
}
