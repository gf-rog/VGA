#include "VGAEngine.h"
#include "Screen.h"

#define DEBUG

#define addr(a) __attribute__((section(".ARM.__at_"a)))

extern void VGA(uint32_t curr_pnt, uint32_t curr_buff);

union{
	NVIC_InitTypeDef nvic;
	USART_InitTypeDef usart;
	TIM_OCInitTypeDef timoc;
	DMA_InitTypeDef dma;
} init;

uint8_t cmd[256];
uint8_t cmd_pos;
uint16_t scrn_pos;

uint8_t scrn_data[960] addr("0x20000000");
uint8_t col_data[960]  addr("0x20000400");
uint8_t scan_data[256] addr("0x20000800");
uint8_t scan_oth[256]  addr("0x20000900");
uint8_t palette[16]    addr("0x20000a00");

uint32_t curr_pnt = (uint32_t) &scrn_data;
uint32_t curr_buff = (uint32_t) &scan_data;//&dummy;//(uint32_t) &scan_data;
bool which_buff = false;
uint8_t spacing = 16;

uint8_t blink = 1;
uint8_t blink_char = 0xff;

extern uint8_t color_cycle[37];
uint8_t pal_ind;

/*void USART1_IRQHandler(){
	if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)){
		//uint8_t b = USART1->DR;
		//SendData(b);
		//cmd[cmd_pos] = (uint8_t) (b & 0xff);
		//if (b == '\n') cmd_pos=0; else cmd_pos++;
		USART_ClearFlag(USART1, USART_FLAG_RXNE);
	}
}*/

bool flash=false;

void vgaEngineSetup(){
	//Overclock to 120 MHz
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
	
	RCC_PLLCmd(DISABLE);
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_15);
	RCC_PLLCmd(ENABLE);
	
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	
	//Setup the clocks
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	RCC_PCLK1Config(RCC_HCLK_Div1);
	RCC_PCLK2Config(RCC_HCLK_Div1);
	
	RCC_AHBPeriphClockCmd(  RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(	RCC_APB2Periph_AFIO |
													RCC_APB2Periph_GPIOA |
													RCC_APB2Periph_GPIOB |
													RCC_APB2Periph_USART1 | 
													RCC_APB2Periph_TIM1, ENABLE);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4, ENABLE);
	
	GPIO_PinConfigure(GPIOC, 13, GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT50MHZ);
	
	//Interrupts
		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);    //4 bits for preemp priority 0 bit for sub priority
		
		init.nvic.NVIC_IRQChannel = USART1_IRQn;
		init.nvic.NVIC_IRQChannelCmd = ENABLE;
		init.nvic.NVIC_IRQChannelPreemptionPriority = 15;
		init.nvic.NVIC_IRQChannelSubPriority = 0;
		NVIC_Init(&init.nvic);
		
		init.nvic.NVIC_IRQChannel = TIM2_IRQn;
		init.nvic.NVIC_IRQChannelCmd = ENABLE;
		init.nvic.NVIC_IRQChannelPreemptionPriority = 10;
		NVIC_Init(&init.nvic);
	
		init.nvic.NVIC_IRQChannel = TIM3_IRQn;
		init.nvic.NVIC_IRQChannelPreemptionPriority = 11;
		init.nvic.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&init.nvic);
		
		init.nvic.NVIC_IRQChannel = TIM4_IRQn;
		init.nvic.NVIC_IRQChannelPreemptionPriority = 11;
		init.nvic.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&init.nvic);
	
	//Pixel timer
		TIM1->PSC = 6 - 1;//Prescaler
		TIM1->ARR = 1;	//Period
		TIM1->CR1 = TIM_CounterMode_Up;	//Settings
		TIM1->CR2 = 0x04;	//set CCDS bit
		TIM1->DIER = TIM_DMA_CC1;
		
		init.timoc.TIM_OCMode = TIM_OCMode_Timing;
		init.timoc.TIM_OutputState = TIM_OutputState_Disable;
		init.timoc.TIM_Pulse = 0;
		init.timoc.TIM_OCPolarity = TIM_OCPolarity_High;
		TIM_OC1Init(TIM1, &init.timoc);
		
	//Scanline timer
	//TIM2, out (PA0 || PA1)
		//Pins
		GPIO_PinConfigure(GPIOA, 0, GPIO_AF_PUSHPULL, GPIO_MODE_OUT50MHZ);
		GPIO_PinConfigure(GPIOA, 1, GPIO_AF_PUSHPULL, GPIO_MODE_OUT50MHZ);
		GPIO_PinConfigure(GPIOA, 2, GPIO_AF_PUSHPULL, GPIO_MODE_OUT50MHZ);
		
		//TIM2 is synchronizing
		TIM2->CR2 = (TIM2->CR2 & ~TIM_CR2_MMS) | TIM_CR2_MMS_0;
		
		TIM2->PSC = 12 - 1;//0;	//Prescaler
		TIM2->ARR = 319 - 1;	//Period
		TIM2->CR1 = TIM_CounterMode_Up | TIM_CKD_DIV2;	//Settings
		
		//Enable pixel DMA
		//TIM2->DIER |= TIM_DIER_CC1DE;
		//TIM2->CR2 |= TIM_CR2_TI1S;	//XOR Mode
	
		//Channel settings
		init.timoc.TIM_OCMode = TIM_OCMode_PWM1;
		init.timoc.TIM_OutputState = TIM_OutputState_Enable;
		init.timoc.TIM_Pulse = 262;
		init.timoc.TIM_OCPolarity = TIM_OCPolarity_High;
		TIM_OC1Init(TIM2, &init.timoc);
		
		init.timoc.TIM_OCMode = TIM_OCMode_PWM2;//TIM_OCMode_PWM1;
		init.timoc.TIM_Pulse = 300;
		TIM_OC2Init(TIM2, &init.timoc);
		
		init.timoc.TIM_OCMode = TIM_OCMode_PWM1;	//Turns off DMA
		init.timoc.TIM_Pulse = 246;
		TIM_OC3Init(TIM2, &init.timoc);
		
		init.timoc.TIM_OCMode = TIM_OCMode_Timing;	//Turns on DMA for next line
		init.timoc.TIM_Pulse = 310;
		TIM_OC4Init(TIM2, &init.timoc);
		
		//Preload settings
		TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
		TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
		TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);
		TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);
		TIM_ARRPreloadConfig(TIM2, DISABLE);
		
	//Frame timer 
	//TIM3, out (PA6 || PA7)
		//Pins
		GPIO_PinConfigure(GPIOA, 6, GPIO_AF_PUSHPULL, GPIO_MODE_OUT50MHZ);
		GPIO_PinConfigure(GPIOA, 7, GPIO_AF_PUSHPULL, GPIO_MODE_OUT50MHZ);
		
		//TIM3 is synchronized
		TIM3->SMCR = (TIM3->SMCR & ~TIM_SMCR_TS) | TIM_SMCR_TS_0;
		TIM3->SMCR = (TIM3->SMCR & ~TIM_SMCR_SMS) | TIM_SMCR_SMS_0 * 0x04;
		
		TIM3->PSC = 319*12 - 1;	//Prescaler
		TIM3->ARR = 523 - 1;	//Period
		TIM3->CR1 = TIM_CounterMode_Up | TIM_CKD_DIV2;	//Settings
		
		//TIM2->CR2 |= TIM_CR2_TI1S;	//XOR Mode
	
		//Channel settings
		init.timoc.TIM_OCMode = TIM_OCMode_PWM1;
		init.timoc.TIM_Pulse = 490;
		TIM_OC1Init(TIM3, &init.timoc);
		
		init.timoc.TIM_OCMode = TIM_OCMode_PWM2;
		init.timoc.TIM_Pulse = 492;
		TIM_OC2Init(TIM3, &init.timoc);
		
		//Preload settings
		TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
		TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
		TIM_ARRPreloadConfig(TIM3, DISABLE);
		
	//TIM4 - new scanline
		//TIM4 is synchronized
		TIM4->SMCR = (TIM4->SMCR & ~TIM_SMCR_TS) | TIM_SMCR_TS_0;
		TIM4->SMCR = (TIM4->SMCR & ~TIM_SMCR_SMS) | TIM_SMCR_SMS_0 * 0x04;
		
		TIM4->PSC = 12 - 1;	//Prescaler
		TIM4->ARR = 319*2 - 1;	//Period
		TIM4->CR1 = TIM_CounterMode_Up | TIM_CKD_DIV2;	//Settings
	
		//Channel settings
		init.timoc.TIM_OCMode = TIM_OCMode_Timing;
		init.timoc.TIM_Pulse = 100;//250;//319*3 + 254;//318;//256;
		TIM_OC1Init(TIM4, &init.timoc);
		
		/*init.timoc.TIM_OCMode = TIM_OCMode_PWM2;
		init.timoc.TIM_Pulse = 492;
		TIM_OC2Init(TIM3, &init.timoc);*/
		
		//Preload settings
		TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
		TIM_ARRPreloadConfig(TIM4, DISABLE);
		
		//B0 to B7 - outputs
		GPIOB->CRH = 0x33333333;
		
		//GPIO_PinConfigure(GPIOB, 0, GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT50MHZ);
		//GPIO_PinWrite(GPIOB, 0, 1);
		DMA_DeInit(DMA1_Channel2);
		
		init.dma.DMA_BufferSize = 256;
		init.dma.DMA_DIR = DMA_DIR_PeripheralDST;//DMA_DIR_PeripheralDST;
		init.dma.DMA_M2M = DMA_M2M_Disable;//Enable;
		init.dma.DMA_Mode = DMA_Mode_Normal;//DMA_Mode_Circular;
		init.dma.DMA_Priority = DMA_Priority_VeryHigh;
		
		init.dma.DMA_MemoryBaseAddr = (uint32_t) &scan_data;//draw_scan;
		init.dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
		init.dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
		
		init.dma.DMA_PeripheralBaseAddr = (uint32_t)(&GPIOB->ODR)+1;
		init.dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;//DMA_PeripheralDataSize_Word;
		init.dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
		
//		init.dma.DMA_PeripheralBaseAddr = (uint32_t) &scan_data;
//		init.dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
//		init.dma.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
//		
//		init.dma.DMA_MemoryBaseAddr = (uint32_t)&(GPIOB->ODR);
//		init.dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
//		init.dma.DMA_MemoryInc = DMA_MemoryInc_Disable;
		DMA_Init(DMA1_Channel2, &(init.dma));
		
		//DMA_DeInit(DMA1_Channel6);
		//init.dma.DMA_MemoryBaseAddr = (uint32_t) &scan_oth;
		//DMA_Init(DMA1_Channel6,&init.dma);
		
	//#ifdef DEBUG
	//Pause the timers
		DBGMCU->CR = DBGMCU_TIM2_STOP;
		DBGMCU->CR = DBGMCU_TIM3_STOP;
		DBGMCU->CR = DBGMCU_TIM4_STOP;
	//#endif
	
	
	//USART
		//GPIO_PinConfigure(GPIOA, 9, GPIO_AF_PUSHPULL, GPIO_MODE_OUT50MHZ);	//USART1
		GPIO_PinConfigure(GPIOA, 10, GPIO_IN_PULL_DOWN, GPIO_MODE_INPUT);	//USART1
		
		init.usart.USART_BaudRate = 115200;
		init.usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		init.usart.USART_WordLength = USART_WordLength_8b;
		init.usart.USART_Mode = USART_Mode_Rx;
		init.usart.USART_Parity = USART_Parity_No;
		init.usart.USART_StopBits = USART_StopBits_1;
		
		USART_Init(USART1, &init.usart);
		
	for (int i = 0; i < 960; i++) {
		scrn_data[i] = scrn_contents[i];
		col_data[i] = (i%32)*15/32 + 1;
	}
	
	// Generate noise as start image
	/*
	const uint16_t start_state = 0xACE1;
	uint16_t lfsr = start_state;
	uint16_t bit;
	for (int i=0; i<960; i++){
		//scrn_data[i]=(i%32<24) ? scrn_contents[i%32+(i/32)*24] : ' ';//(i%32<24) ? ' ' : ' ';//' ';
		//scrn_data[i] = scrn_contents[i];
		uint8_t gen_byte = 0;
		for (int b=0; b<8; b++){
			bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
			gen_byte = (gen_byte << 1) + bit;
			lfsr = (lfsr >> 1) | (bit << 15);
		}
		//gen_byte &= 0x03;
		scrn_data[i] = 0xfc + gen_byte%3;
	}*/
	for (int i=0; i<16; i++){
		palette[i] = 0x00;
	}
	
	//Turn on peripherals
		TIM_ITConfig(TIM2, TIM_IT_CC4, ENABLE);
		TIM_ITConfig(TIM2, TIM_IT_CC3, ENABLE);
	
		TIM_ITConfig(TIM3, TIM_IT_CC1, ENABLE);
		TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
	
		TIM_ITConfig(TIM4, TIM_IT_CC1, ENABLE);
	
		//TIM_ITConfig(TIM3, TIM_IT_CC3, ENABLE);
		TIM_Cmd(TIM4, ENABLE);
		TIM_Cmd(TIM3, ENABLE);
		TIM_Cmd(TIM2, ENABLE);
		TIM_Cmd(TIM1, ENABLE);
		//DMA_Cmd(DMA1_Channel5, ENABLE);
	
		//USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
		USART_Cmd(USART1, ENABLE);
}

uint8_t special_char = 0;
void vgaEngineLoop(){
	#define blink_scrn scrn_data[scrn_pos] = blink_char
		
		if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)){
			uint8_t data = USART1->DR;
			if (special_char == 0){
				switch (data){
					case '@'-'@':	// Ctrl+Space - full block character
						scrn_data[scrn_pos++] = 0xff;
						break;
					case 0x7F: //Backspace key
						scrn_data[scrn_pos] = blink_char;
						/*if ((scrn_pos&31) == 0 && scrn_pos!=0){
							scrn_pos-=8;
							//while ((scrn_pos&31)>0) scrn_pos--;
						}*/
						if (scrn_pos>0) scrn_pos--;
						scrn_data[scrn_pos] = ' ';
						break;
					case 'L'-'@':	// Ctrl+L - clear the screen
						for (int i=0; i<960; i++) scrn_data[i] = 0x00;
						scrn_pos=0;
						break;
					case 'T'-'@': // Ctrl+T - go to the top of the screen
						blink_scrn;
						scrn_pos=0;
						break;
					case '\r':		// Newline
						blink_scrn;
						scrn_pos = scrn_pos - scrn_pos%32 + 32;
						break;
					case '\033':	// Special character
						special_char = 1;
						break;
					default:
						scrn_data[scrn_pos++] = USART1->DR;
						break;
				}
				if (USART1->DR != '\033'){
					blink_char = scrn_data[scrn_pos];
					scrn_data[scrn_pos] = 0xff;
				}
			}else{
				if (special_char == 2){
					switch (data){	// Arrow keys
						case 'A':
							blink_scrn;
							scrn_pos = (scrn_pos + 32*30 - 32) % (32*30);
							break;
						case 'B':
							blink_scrn;
							scrn_pos = (scrn_pos + 32) % (32*30);
							break;
						case 'C':
							blink_scrn;
							scrn_pos = scrn_pos - (scrn_pos % 32) + (scrn_pos + 1) % 32;
							break;
						case 'D':
							blink_scrn;
							scrn_pos = scrn_pos - (scrn_pos % 32) + (scrn_pos + 32 - 1) % 32;
							break;
					}
					special_char = 0;
					blink_char = scrn_data[scrn_pos];
					scrn_data[scrn_pos] = 0xff;
				}else{
					special_char++;
				}
			}
			blink = 15;
			
			//if ((scrn_pos&31) >= 24) scrn_pos+=8;
			if (scrn_pos>=32*30){
				scrn_pos=0;
				blink_char = scrn_data[0];
			}
			USART_ClearFlag(USART1, USART_FLAG_RXNE);
		}
		if (blink==0){
			blink=30;
			scrn_data[scrn_pos]=(scrn_data[scrn_pos]==0xff) ? blink_char : 0xff;
		}
}

bool toggle=false;
bool allow=true;

uint8_t data_count = 0;
uint8_t delay = 5;

uint32_t gpiob_addr = (uint32_t)&GPIOB->ODR;

inline void buffer_switch(){
	DMA1_Channel2->CMAR = curr_buff ^ ((uint32_t)&scan_data ^ (uint32_t)&scan_oth);// ^ (uint32_t)&scan_oth;
	__asm("ISB");
	DMA1_Channel2->CCR |= DMA_CCR1_EN;
}

void TIM2_IRQHandler(void){
	if (TIM_GetITStatus(TIM2, TIM_IT_CC4)){	//Turn on DMA for next line
		__asm("ISB");
		if (allow) buffer_switch();
		TIM_ClearITPendingBit(TIM2, TIM_IT_CC4);
	}
	if (TIM_GetITStatus(TIM2, TIM_IT_CC3)){	//Turn off DMA, prepare for next line
		DMA1_Channel2->CCR &= ~DMA_CCR1_EN;
		//DMA1_Channel6->CCR &= ~DMA_CCR1_EN;
		GPIOB->ODR = 0x0000;
		DMA_SetCurrDataCounter(DMA1_Channel2, 256);
		//DMA_SetCurrDataCounter(DMA1_Channel6, 256);
		TIM_ClearITPendingBit(TIM2, TIM_IT_CC3);
	}
}

void TIM3_IRQHandler(void){
	if (TIM_GetITStatus(TIM3, TIM_IT_CC1)){
		DMA1_Channel2->CCR &= ~DMA_CCR1_EN;
		//DMA1_Channel6->CCR &= ~DMA_CCR1_EN;
		GPIOB->ODR = 0x0000;
		allow=false;
		curr_pnt=(uint32_t) &scrn_data;
		
		if (--delay==0){
			for (int i=0; i<15; i++){
				palette[i+1] = color_cycle[(pal_ind+i)%37];
				//palette[i+2]=color_cycle[7];
			}
			pal_ind++;
			pal_ind%=37;
			delay=10;
		}
		blink--;
		/*if (--delay == 0){
			for (int i=0; i<224; i++){
				scan_data[i] = ((i==0) || (i==203)) ? 0xff : 0x00;//i == (data_count & 31) ? 0xff : 0x00;//(data_count >> i) & 1 ? 0xff : 0x00;
			}
			data_count++;
			delay=15;
		}*/
		//GPIOB->CRL = 0x44444444;
		//VGA(curr_pnt);
		//if ((curr_pnt & 0x07) == 0x07) curr_pnt = curr_pnt & (~0x00000007) + 32; else curr_pnt++;
		vgaEngineLoop();
		TIM_ClearITPendingBit(TIM3, TIM_IT_CC1);
	}
	if (TIM_GetITStatus(TIM3, TIM_IT_Update)){
		allow=true;
		TIM_SetCounter(TIM4, 0);
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
		which_buff = false;
		curr_buff = (uint32_t)&scan_data;
		VGA(curr_pnt, curr_buff);
	}
	
}

void TIM4_IRQHandler(void){
	__asm("ISB");
	TIM_ClearITPendingBit(TIM4, TIM_IT_CC1);
	if (allow){
		which_buff = !which_buff;
		curr_buff ^= ((uint32_t)&scan_data ^ (uint32_t)&scan_oth);
		//buffer_switch();
		VGA(curr_pnt, curr_buff);
		
		
		
		if ((curr_pnt & 0x07) == 0x07){
			curr_pnt=curr_pnt-7+32;
		}else{
			curr_pnt++;
		}
	}
	//GPIOB->ODR ^= 0xffff;
}